#pragma once

#include "sticky_note.h"

#include <cstddef>
#include <string>
#include <vector>

struct EditorSession {
    sticky_note note;
    // 0-based index; may equal note.text.size() for the virtual "new line at end" slot.
    std::size_t current_line = 0;
};

void editor_reset_cursor(EditorSession& session);

void write_to_current_line(EditorSession& session, const std::string& text);

void erase_from_current_line(EditorSession& session, const std::vector<std::string>& fields);

bool goto_line(EditorSession& session, int line_1based);

void show_note(const EditorSession& session);
