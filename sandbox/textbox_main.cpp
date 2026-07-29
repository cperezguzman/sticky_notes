#include "sticky_gui.h"
#include "sticky_popup.h"
#include "text_font_render.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>

namespace {
void process_sidebar_pop_out(StickyGui& gui, StickyPopupManager& popups) {
    StickyPanel panel{};
    int screen_x = 100;
    int screen_y = 100;
    if (!sticky_gui_consume_sidebar_pop_out_request(gui, panel, screen_x, screen_y)) {
	return;
    }
    sticky_popup_open(popups, std::move(panel), gui.theme_id, screen_x, screen_y);
}

void process_pop_out(StickyGui& gui, StickyPopupManager& popups, SDL_Window* desk_window) {
    StickyPanel panel{};
    if (!sticky_gui_consume_pop_out_request(gui, panel)) {
	return;
    }
    int screen_x = 100;
    int screen_y = 100;
    sticky_gui_pop_out_screen_position(desk_window, panel, screen_x, screen_y);
    sticky_popup_open(popups, std::move(panel), gui.theme_id, screen_x, screen_y);
}

void process_dock(StickyGui& gui, StickyPopupManager& popups) {
    StickyPanel panel{};
    if (!sticky_popup_consume_dock_request(popups, panel)) {
	return;
    }
    sticky_gui_attach_panel(gui, std::move(panel));
}

SDL_WindowID window_id(SDL_Window* window) {
    return window != nullptr ? SDL_GetWindowID(window) : static_cast<SDL_WindowID>(0);
}
} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // GNOME/KDE (Wayland + X11): app id must match sticky-notes.desktop / StartupWMClass.
    SDL_SetAppMetadata("Sticky Notes", "1.0", "sticky-notes");
    SDL_SetHint(SDL_HINT_APP_ID, "sticky-notes");
    SDL_SetHint(SDL_HINT_APP_NAME, "Sticky Notes");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
	std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
	return 1;
    }

    SDL_Window* desk_window = SDL_CreateWindow("Sticky Notes", 960, 640,
					       SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);
    if (desk_window == nullptr) {
	std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
	SDL_Quit();
	return 1;
    }
    sticky_gui_install_desk_hit_test(desk_window);

    SDL_Renderer* desk_renderer = SDL_CreateRenderer(desk_window, nullptr);
    if (desk_renderer == nullptr) {
	std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
	SDL_DestroyWindow(desk_window);
	SDL_Quit();
	return 1;
    }

    SDL_StartTextInput(desk_window);

    StickyGui gui{};
    sticky_gui_init(gui);

    StickyPopupManager popups{};
    const SDL_WindowID desk_id = window_id(desk_window);

    bool quit_requested = false;
    while (!quit_requested) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
	    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED
		&& event.window.windowID == desk_id) {
		SDL_StartTextInput(desk_window);
	    }

	    sticky_popup_handle_event(popups, gui, event, desk_id, quit_requested);
	    sticky_gui_handle_event(gui, event, quit_requested, desk_id);
	}

	process_pop_out(gui, popups, desk_window);
	process_sidebar_pop_out(gui, popups);
	process_dock(gui, popups);

	SDL_SetRenderDrawColor(desk_renderer, sticky_gui_desk_color(gui).r,
			       sticky_gui_desk_color(gui).g, sticky_gui_desk_color(gui).b,
			       sticky_gui_desk_color(gui).a);
	SDL_RenderClear(desk_renderer);
	sticky_gui_render(desk_renderer, gui, desk_window);
	SDL_RenderPresent(desk_renderer);

	sticky_popup_render_all(popups);

	popups.theme_id = gui.theme_id;

	if (!sticky_popup_any_dragging(popups)) {
	    SDL_Delay(16);
	}
    }

    sticky_gui_save_all(gui);
    sticky_popup_save_all(popups);

    sticky_popup_close_all(popups);

    text_font_shutdown(desk_renderer);
    SDL_StopTextInput(desk_window);
    SDL_DestroyRenderer(desk_renderer);
    SDL_DestroyWindow(desk_window);
    SDL_Quit();
    return 0;
}
