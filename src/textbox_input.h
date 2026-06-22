#pragma once

#include "note_editor.h"

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

bool textbox_apply_key(EditorSession& session, TextboxKeyEvent event);

std::size_t textbox_line_count(const EditorSession& session);

std::string textbox_line_at(const EditorSession& session, std::size_t line_index);

std::size_t textbox_cursor_line(const EditorSession& session);

std::size_t textbox_cursor_column(const EditorSession& session);

// Current line text (convenience for single-line callers and tests).
std::string textbox_line_text(const EditorSession& session);

// Keep cursor row inside the visible window (call after key handling).
void textbox_scroll_to_cursor(TextboxViewport& viewport, const EditorSession& session,
			      std::size_t visible_line_count);
