#include "sticky_gui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
	std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
	return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Sticky Notes - Phase 4", 960, 640, 0);
    if (window == nullptr) {
	std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
	SDL_Quit();
	return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
	std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 1;
    }

    SDL_StartTextInput(window);

    StickyGui gui{};
    sticky_gui_init(gui);

    bool quit_requested = false;
    while (!quit_requested) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
	    sticky_gui_handle_event(gui, event, quit_requested);
	}

	SDL_SetRenderDrawColor(renderer, 28, 28, 32, 255);
	SDL_RenderClear(renderer);

	sticky_gui_render(renderer, gui);

	SDL_RenderPresent(renderer);
	SDL_Delay(16);
    }

    sticky_gui_save_all(gui);

    SDL_StopTextInput(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
