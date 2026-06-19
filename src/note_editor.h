#pragma once

#include "sticky_note.h"

#include <cstddef>
#include <string>
#include <vector>

struct EditorSession {
    sticky_note note;
    // 0-based line; may equal note.text.size() for the virtual "new line at end" slot.
    std::size_t current_line = 0;
    // 0-based column within the line; may equal line.length() (insert after last char).
    std::size_t current_column = 0;
};

void editor_reset_cursor(EditorSession& session);

void write_to_current_line(EditorSession& session, const std::string& text);

void append_to_current_line(EditorSession& session, const std::string& text);

void insert_at_cursor(EditorSession& session, const std::string& text);

void erase_from_current_line(EditorSession& session, const std::vector<std::string>& fields);

bool goto_line(EditorSession& session, int line_1based, int col_1based = 1);

void move_left(EditorSession& session);

void move_right(EditorSession& session);

void move_home(EditorSession& session);

void move_end(EditorSession& session);

void insert_newline_at_cursor(EditorSession& session);

bool delete_current_line(EditorSession& session);

void delete_at_cursor(EditorSession& session);

void show_note(const EditorSession& session);

// For tests: render current line with a | at the cursor (no trailing newline).
std::string format_line_with_cursor(const EditorSession& session, std::size_t line_index);
