/* $Id$ */
/*
   Copyright (C) 2006 - 2009 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playlevel Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "controller_base.hpp"
#include "dialogs.hpp"
#include "mouse_handler_base.hpp"

controller_base::controller_base(
		int ticks, const config& game_config, CVideo& /*video*/) :
	game_config_(game_config),
	ticks_(ticks),
	key_(),
	browse_(false),
	scrolling_(false)
{
}

controller_base::~controller_base()
{
}

int controller_base::get_ticks() {
	return ticks_;
}

bool controller_base::can_execute_command(
		hotkey::HOTKEY_COMMAND /*command*/, int /*index*/) const
{
	return false;
}

void controller_base::handle_event(const SDL_Event& event)
{
	if(gui::in_dialog()) {
		return;
	}

	switch(event.type) {
	case SDL_KEYDOWN:
		// Detect key press events, unless there something that has keyboard focus
		// in which case the key press events should go only to it.
		if(have_keyboard_focus()) {
			hotkey::key_event(get_display(),event.key,this);
		} else {
			process_keydown_event(event);
			break;
		}
		// intentionally fall-through
	case SDL_KEYUP:
		process_keyup_event(event);
		break;
	case SDL_MOUSEMOTION:
		// Ignore old mouse motion events in the event queue
		SDL_Event new_event;
		if(SDL_PeepEvents(&new_event,1,SDL_GETEVENT,
					SDL_EVENTMASK(SDL_MOUSEMOTION)) > 0) {
			while(SDL_PeepEvents(&new_event,1,SDL_GETEVENT,
						SDL_EVENTMASK(SDL_MOUSEMOTION)) > 0) {};
			get_mouse_handler_base().mouse_motion_event(new_event.motion, browse_);
		} else {
			get_mouse_handler_base().mouse_motion_event(event.motion, browse_);
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		get_mouse_handler_base().mouse_press(event.button, browse_);
		post_mouse_press(event);
		if (get_mouse_handler_base().get_show_menu()){
			show_menu(get_display().get_theme().context_menu()->items(),event.button.x,event.button.y,true);
		}
		break;
	case SDL_ACTIVEEVENT:
		if (event.active.type == SDL_APPMOUSEFOCUS && event.active.gain == 0) {
			if (get_mouse_handler_base().is_dragging()) {
				//simulate mouse button up when the app has lost mouse focus
				//this should be a general fix for the issue when the mouse
				//is dragged out of the game window and then the button is released
				int x, y;
				Uint8 mouse_flags = SDL_GetMouseState(&x, &y);
				if ((mouse_flags & SDL_BUTTON_LEFT) == 0) {
					SDL_Event e;
					e.type = SDL_MOUSEBUTTONUP;
					e.button.state = SDL_RELEASED;
					e.button.button = SDL_BUTTON_LEFT;
					e.button.x = x;
					e.button.y = y;
					get_mouse_handler_base().mouse_press(event.button, browse_);
					post_mouse_press(event);
				}
			}
		}
		break;
	default:
		break;
	}
}

bool controller_base::have_keyboard_focus()
{
	return true;
}

void controller_base::process_keydown_event(const SDL_Event& /*event*/)
{
	//no action by default
}

void controller_base::process_keyup_event(const SDL_Event& /*event*/) {
	//no action by default
}

void controller_base::post_mouse_press(const SDL_Event& /*event*/) {
	//no action by default
}

bool controller_base::handle_scroll(CKey& key, int mousex, int mousey, int mouse_flags)
{
	bool scrolling = false;
	bool mouse_in_window = SDL_GetAppState() & SDL_APPMOUSEFOCUS;
	const int scroll_threshold = (preferences::mouse_scroll_enabled())
			? preferences::mouse_scroll_threshold()
			: 0;

	if ((key[SDLK_UP] && have_keyboard_focus())
	|| (mousey < scroll_threshold && mouse_in_window)) {
		get_display().scroll(0,-preferences::scroll_speed());
		scrolling = true;
	}
	if ((key[SDLK_DOWN] && have_keyboard_focus())
	|| (mousey > get_display().h() - scroll_threshold && mouse_in_window)) {
		get_display().scroll(0,preferences::scroll_speed());
		scrolling = true;
	}
	if ((key[SDLK_LEFT] && have_keyboard_focus())
	|| (mousex < scroll_threshold && mouse_in_window)) {
		get_display().scroll(-preferences::scroll_speed(),0);
		scrolling = true;
	}
	if ((key[SDLK_RIGHT] && have_keyboard_focus())
	|| (mousex > get_display().w() - scroll_threshold && mouse_in_window)) {
		get_display().scroll(preferences::scroll_speed(),0);
		scrolling = true;
	}
	if ((mouse_flags & SDL_BUTTON_MMASK) != 0) { //middle mouse button pressed
		const SDL_Rect& rect = get_display().map_outside_area();
		if (point_in_rect(mousex, mousey,rect)) {
			// relative distance from the center to the border
			// NOTE: the view is a rectangle, so can be more sensible in one direction
			// but seems intuitive to use and it's useful since you must
			// more often scroll in the direction where the view is shorter
			const double xdisp = ((1.0*mousex / rect.w) - 0.5);
			const double ydisp = ((1.0*mousey / rect.h) - 0.5);
			// 4.0 give twice the normal speed when mouse is at border (xdisp=0.5)
			const double scroll_speed = 4.0 * preferences::scroll_speed();
			const int xspeed = round_double(xdisp * scroll_speed);
			const int yspeed = round_double(ydisp * scroll_speed);
			get_display().scroll(xspeed,yspeed);
			scrolling = true;
		}
	}
	return scrolling;
}

void controller_base::play_slice()
{
	CKey key;
	events::pump();
	events::raise_process_event();
	events::raise_draw_event();

	slice_before_scroll();
	const theme::menu* const m = get_display().menu_pressed();
	if(m != NULL) {
		const SDL_Rect& menu_loc = m->location(get_display().screen_area());
		show_menu(m->items(),menu_loc.x+1,menu_loc.y + menu_loc.h + 1,false);
		return;
	}

	int mousex, mousey;
	Uint8 mouse_flags = SDL_GetMouseState(&mousex, &mousey);
	tooltips::process(mousex, mousey);
	bool was_scrolling = scrolling_;
	scrolling_ = handle_scroll(key, mousex, mousey, mouse_flags);

	get_display().draw();
	if (!scrolling_) {
		if (was_scrolling) {
			// scrolling ended, update the cursor and the brightened hex
			get_mouse_handler_base().mouse_update(browse_);
		}
		get_display().delay(20);
	}
	slice_end();
}

void controller_base::slice_before_scroll()
{
	//no action by default
}

void controller_base::slice_end()
{
	//no action by default
}

void controller_base::show_menu(const std::vector<std::string>& items_arg, int xloc, int yloc, bool context_menu)
{
	std::vector<std::string> items = items_arg;
	hotkey::HOTKEY_COMMAND command;
	std::vector<std::string>::iterator i = items.begin();
	while(i != items.end()) {
		command = hotkey::get_hotkey(*i).get_id();
		if(!can_execute_command(command)
		|| (context_menu && !in_context_menu(command))) {
			i = items.erase(i);
			continue;
		}
		++i;
	}
	if(items.empty())
		return;
	command_executor::show_menu(items, xloc, yloc, context_menu, get_display());
}

bool controller_base::in_context_menu(hotkey::HOTKEY_COMMAND /*command*/) const
{
	return true;
}
