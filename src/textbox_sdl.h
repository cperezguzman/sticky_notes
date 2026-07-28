#pragma once

// textbox_sdl — SDL3 adapter between platform events and the textbox_input seam.
//
// textbox_handle_sdl_event translates SDL key/text events into TextboxKeyEvent and
// updates EditorSession. textbox_render_panel draws the sticky chrome (title bar,
// close/dock buttons) and the scrolled multiline body.

#include "gui_theme.h"
#include "note_editor.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

// SDL adapter: maps platform events to the textbox_input seam.
bool textbox_handle_sdl_event(EditorSession& session, const SDL_Event& event, bool& quit_requested,
			      std::size_t max_body_columns = 0, bool quit_on_escape = true);

std::size_t textbox_body_max_columns(float panel_width);

// Sticky-note panel: title bar + multiline body (clipped / scrolled via viewport).
float sticky_panel_title_bar_height();

float sticky_panel_close_button_width();

float sticky_panel_dock_button_width();

void sticky_panel_close_button_rect(float panel_x, float panel_y, float panel_width, float& out_x,
				  float& out_y, float& out_w, float& out_h);

void sticky_panel_dock_button_rect(float panel_x, float panel_y, float panel_width,
				   bool has_close_button, float& out_x, float& out_y, float& out_w,
				   float& out_h);

struct PanelChrome {
    const char* title_text = nullptr;
    bool show_close_button = false;
    bool show_dock_button = false;
    bool title_caret_visible = false;
    std::size_t title_caret_col = 0;
};

void textbox_render_panel(SDL_Renderer* renderer, float x, float y, float width, float height,
			  EditorSession& session, TextboxViewport& viewport, bool focused,
			  const PanelChrome& chrome, const StickyGuiTheme& theme);
