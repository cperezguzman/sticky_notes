#include "textbox_input.h"
#include "textbox_sdl.h"

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

    SDL_Window* window = SDL_CreateWindow("Sticky Notes — Phase 2 textbox", 640, 120, 0);
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

    EditorSession session{};
    textbox_init_session(session);

    bool quit_requested = false;
    bool esc_was_down = false;
    while (!quit_requested) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
	    textbox_handle_sdl_event(session, event, quit_requested);
	}

	const bool* keys = SDL_GetKeyboardState(nullptr);
	if (keys != nullptr) {
	    const bool esc_down = keys[SDL_SCANCODE_ESCAPE];
	    if (esc_down && !esc_was_down) {
		quit_requested = true;
	    }
	    esc_was_down = esc_down;
	}

	SDL_SetRenderDrawColor(renderer, 28, 28, 32, 255);
	SDL_RenderClear(renderer);

	textbox_render(renderer, 32.0f, 32.0f, 576.0f, 40.0f, session);

	SDL_RenderPresent(renderer);
	SDL_Delay(16);
    }

    SDL_StopTextInput(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
