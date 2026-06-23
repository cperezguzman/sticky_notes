#pragma once

#include "note_editor.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

// SDL adapter: maps platform events to the textbox_input seam.
bool textbox_handle_sdl_event(EditorSession& session, const SDL_Event& event, bool& quit_requested);

// Sticky-note panel: title bar + multiline body (clipped / scrolled via viewport).
float sticky_panel_title_bar_height();

float sticky_panel_close_button_width();

struct PanelChrome {
    const char* title_text = nullptr;
    bool show_close_button = false;
    bool title_caret_visible = false;
    std::size_t title_caret_col = 0;
};

void textbox_render_panel(SDL_Renderer* renderer, float x, float y, float width, float height,
			  const EditorSession& session, TextboxViewport& viewport, bool focused,
			  const PanelChrome& chrome = {});
