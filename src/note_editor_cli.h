#pragma once

#include "note_editor.h"

#include <string>
#include <vector>

// CLI adapter: maps terminal commands to the silent edit core and prints feedback.

void cli_show_cursor(const EditorSession& session);

void cli_show_note(const EditorSession& session);

void cli_undo(EditorSession& session);

void cli_redo(EditorSession& session);

void cli_erase(EditorSession& session, const std::vector<std::string>& fields);

void cli_delete_at_cursor(EditorSession& session);

void cli_find(EditorSession& session, const std::string& needle);

void cli_find_next(EditorSession& session);

void cli_yank(EditorSession& session);

void cli_paste(EditorSession& session);

void cli_move_left(EditorSession& session);

void cli_move_right(EditorSession& session);

void cli_move_home(EditorSession& session);

void cli_move_end(EditorSession& session);

void cli_goto(EditorSession& session, int line_1based, int col_1based);

void cli_newline(EditorSession& session);

void cli_delete_line(EditorSession& session);
