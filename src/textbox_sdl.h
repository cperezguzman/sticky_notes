#pragma once

// textbox_sdl — SDL3 adapter between platform events and the textbox_input seam.
//
// textbox_handle_sdl_event translates SDL key/text events into TextboxKeyEvent and
// updates EditorSession. textbox_render_panel draws the sticky chrome (title bar,
// close/dock buttons) and the scrolled multiline body.

#include "gui_theme.h"
#include "note_editor.h"
#include "text_font.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

// SDL adapter: maps platform events to the textbox_input seam.
bool textbox_handle_sdl_event(EditorSession& session, const SDL_Event& event, bool& quit_requested,
			      std::size_t max_body_columns = 0, bool quit_on_escape = true);

bool textbox_handle_sdl_event_width(EditorSession& session, const SDL_Event& event,
				    bool& quit_requested, float max_body_width_px,
				    NoteTypography typo, bool quit_on_escape = true);

std::size_t textbox_body_max_columns(float panel_width);

std::size_t textbox_body_max_columns_for(float panel_width, NoteTypography typo);

// Pixel width available for body glyphs (padding + scrollbar gutter reserved).
float textbox_body_wrap_width(float panel_width);

float textbox_body_wrap_width_for(float panel_width, NoteTypography typo);

std::size_t textbox_visible_body_lines(float panel_height);

std::size_t textbox_visible_body_lines_for(float panel_height, NoteTypography typo);

bool textbox_handle_body_click(EditorSession& session, TextboxViewport& viewport, float panel_x,
			       float panel_y, float panel_width, float panel_height, float mx,
			       float my, bool extend_selection);

bool textbox_handle_mouse_wheel(EditorSession& session, TextboxViewport& viewport, float panel_height,
				float wheel_y);

// Body scrollbar (shown when the note is taller than the panel).
bool textbox_body_scrollbar_needed(const EditorSession& session, float panel_height);

void textbox_body_scrollbar_track_rect(float panel_x, float panel_y, float panel_width,
				       float panel_height, float& out_x, float& out_y, float& out_w,
				       float& out_h);

void textbox_body_scrollbar_thumb_rect(const EditorSession& session, const TextboxViewport& viewport,
				       float panel_x, float panel_y, float panel_width,
				       float panel_height, float& out_x, float& out_y, float& out_w,
				       float& out_h);

bool textbox_point_in_body_scrollbar(const EditorSession& session, float panel_x, float panel_y,
				     float panel_width, float panel_height, float mx, float my);

// Map a pointer Y (and grab offset inside the thumb) onto first_visible_line.
void textbox_scroll_body_from_thumb_y(EditorSession& session, TextboxViewport& viewport,
				      float panel_x, float panel_y, float panel_width,
				      float panel_height, float my, float drag_offset_y);

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
    bool show_resize_grip = true;
    bool title_caret_visible = false;
    std::size_t title_caret_col = 0;
};

void textbox_render_panel(SDL_Renderer* renderer, float x, float y, float width, float height,
			  EditorSession& session, TextboxViewport& viewport, bool focused,
			  const PanelChrome& chrome, const StickyGuiTheme& theme);
