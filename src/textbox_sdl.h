#pragma once

#include "note_editor.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

// SDL adapter: maps platform events to the textbox_input seam.
bool textbox_handle_sdl_event(EditorSession& session, const SDL_Event& event, bool& quit_requested);

// Sticky-note panel: title bar + multiline body (clipped / scrolled via viewport).
float sticky_panel_title_bar_height();

void textbox_render_panel(SDL_Renderer* renderer, float x, float y, float width, float height,
			  const EditorSession& session, TextboxViewport& viewport, bool focused = true);
