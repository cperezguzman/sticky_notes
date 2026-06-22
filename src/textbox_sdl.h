#pragma once

#include "note_editor.h"

#include <SDL3/SDL.h>

// SDL adapter: maps platform events to the textbox_input seam.
bool textbox_handle_sdl_event(EditorSession& session, const SDL_Event& event, bool& quit_requested);

void textbox_render(SDL_Renderer* renderer, float x, float y, float width, float height,
		    const EditorSession& session);
