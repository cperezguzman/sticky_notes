#pragma once

#include "note_editor.h"

#include <cstdint>
#include <string>

// Platform-neutral key events for the single-line textbox seam.
// SDL (or any frontend) translates raw input into these before calling textbox_apply_key.

enum class TextboxKeyKind {
    Character,
    Backspace,
    Delete,
    Left,
    Right,
    Home,
    End,
};

struct TextboxKeyEvent {
    TextboxKeyKind kind = TextboxKeyKind::Character;
    char32_t character = 0;
};

// Ensures the session is a single-line textbox (line 0, empty body if needed).
void textbox_init_session(EditorSession& session);

// Applies one key event through the existing editor core. Returns false if ignored.
bool textbox_apply_key(EditorSession& session, TextboxKeyEvent event);

// Current line text and 0-based cursor column for rendering.
std::string textbox_line_text(const EditorSession& session);

std::size_t textbox_cursor_column(const EditorSession& session);
