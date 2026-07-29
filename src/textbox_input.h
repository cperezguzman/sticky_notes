#pragma once

#include "note_editor.h"
#include "text_font.h"

#include <cstdint>
#include <string>

// Platform-neutral key events for the multiline textbox seam.
// SDL (or any frontend) translates raw input into these before calling textbox_apply_key.

enum class TextboxKeyKind {
    Character,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    Newline,
};

struct TextboxKeyEvent {
    TextboxKeyKind kind = TextboxKeyKind::Character;
    char32_t character = 0;
};

// Scroll state for multiline body rendering (clip / scroll).
struct TextboxViewport {
    std::size_t first_visible_line = 0;
};

// One empty body line; same EditorSession shape as the terminal editor.
void textbox_init_session(EditorSession& session);

bool textbox_apply_key(EditorSession& session, TextboxKeyEvent event, std::size_t max_columns = 0);

// Like textbox_apply_key, then soft-wrap to a pixel width using typography advances.
bool textbox_apply_key_width(EditorSession& session, TextboxKeyEvent event, float max_width_px,
			     NoteTypography typo);

void textbox_init_hard_breaks_for_loaded_note(EditorSession& session);

// Join lines that look like soft-wrap artifacts from older saves (mid-word / after-space).
void textbox_repair_persisted_soft_wraps(std::vector<std::string>& lines);

// Hard-break paragraphs only — suitable for writing Body: to disk (no soft wraps).
std::vector<std::string> textbox_storage_lines(const EditorSession& session);

// Styles collapsed to match textbox_storage_lines (for disk Styles: section).
NoteStyles textbox_storage_styles(const EditorSession& session);

// Soft-wrap by character columns (Debug / tests). Delegates to width using metrics.char_w.
void textbox_enforce_wrap(EditorSession& session, std::size_t max_columns);

// Soft-wrap paragraphs so each visual line fits in max_width_px (real glyph advances).
void textbox_enforce_wrap_width(EditorSession& session, float max_width_px, NoteTypography typo);

std::size_t textbox_line_count(const EditorSession& session);

std::string textbox_line_at(const EditorSession& session, std::size_t line_index);

std::size_t textbox_cursor_line(const EditorSession& session);

std::size_t textbox_cursor_column(const EditorSession& session);

// Current line text (convenience for single-line callers and tests).
std::string textbox_line_text(const EditorSession& session);

// Keep cursor row inside the visible window (call after key handling).
void textbox_scroll_to_cursor(TextboxViewport& viewport, const EditorSession& session,
			      std::size_t visible_line_count);

// Clamp scroll after reflow/resize. If content fits, snap to the start.
void textbox_clamp_viewport(TextboxViewport& viewport, const EditorSession& session,
			    std::size_t visible_line_count);

// Show the beginning of the note (used when the panel shrinks).
void textbox_pin_viewport_to_start(TextboxViewport& viewport);

// Place the cursor at a line/column (clamped to the note).
void textbox_set_cursor(EditorSession& session, std::size_t line, std::size_t column);

// Map a click in body content coordinates (origin = body top-left, y grows down)
// onto a cursor position using the current viewport.
void textbox_click_body(EditorSession& session, TextboxViewport& viewport,
			std::size_t visible_line_count, float local_x, float local_y, float char_w,
			float line_h, float padding);

// Scroll the viewport by delta_lines (negative = up). Returns true if it moved.
bool textbox_scroll_lines(TextboxViewport& viewport, const EditorSession& session,
			  std::size_t visible_line_count, int delta_lines);

// Max value of first_visible_line for the current content / view size.
std::size_t textbox_max_first_visible(const EditorSession& session,
				      std::size_t visible_line_count);

// Jump scroll; clamps to a valid range. Returns true if the viewport changed.
bool textbox_set_first_visible(TextboxViewport& viewport, const EditorSession& session,
			       std::size_t visible_line_count, std::size_t first);
