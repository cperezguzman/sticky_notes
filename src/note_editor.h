#pragma once

// note_editor — in-memory multiline text editor used by the CLI, SDL textbox, and tests.
//
// EditorSession owns the sticky_note body plus a line/column cursor, undo/redo stacks,
// find state, and hard-break flags (Enter vs soft wrap). Most mutating ops push an
// undo snapshot first. EditStatus returns structured outcomes so GUI/CLI can show
// messages without parsing strings.
//
// Coordinates: lines and columns are 0-based internally; goto_line takes 1-based args.

#include "sticky_note.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct EditorSnapshot {
    std::vector<std::string> text;
    std::vector<bool> hard_line_break_after;
    NoteStyles styles;
    std::size_t current_line = 0;
    std::size_t current_column = 0;
    uint8_t typing_style = 0;
};

struct EditorSession {
    sticky_note note;
    // 0-based line; may equal note.text.size() for the virtual "new line at end" slot.
    std::size_t current_line = 0;
    // 0-based column within the line; may equal line.length() (insert after last char).
    std::size_t current_column = 0;
    // Selection anchor; active when has_selection is true.
    std::size_t sel_anchor_line = 0;
    std::size_t sel_anchor_column = 0;
    bool has_selection = false;
    // Style flags applied to newly typed characters.
    uint8_t typing_style = 0;
    // hard_line_break_after[i] == true: user pressed Enter after line i (not soft wrap).
    std::vector<bool> hard_line_break_after;
    std::vector<EditorSnapshot> undo_stack;
    std::vector<EditorSnapshot> redo_stack;
    std::string find_needle;
    bool find_active = false;
};

enum class EditStatus {
    Ok,
    NothingToUndo,
    NothingToRedo,
    NoLineAtCursor,
    NothingBeforeCursor,
    NothingAtCursor,
    AtStart,
    AtEnd,
    EmptyNeedle,
    NoTextToSearch,
    NotFound,
    FindNotActive,
    NoMoreMatches,
    NoLineToYank,
    ClipboardEmpty,
    LineNumberTooSmall,
    ColumnNumberTooSmall,
    LineOutOfRange,
    ColumnOutOfRange,
};

void editor_reset_cursor(EditorSession& session);

void editor_clear_history(EditorSession& session);

EditStatus editor_undo(EditorSession& session);

EditStatus editor_redo(EditorSession& session);

void write_to_current_line(EditorSession& session, const std::string& text);

void append_to_current_line(EditorSession& session, const std::string& text);

void insert_at_cursor(EditorSession& session, const std::string& text);

EditStatus erase_char_before(EditorSession& session);

EditStatus erase_chars_before(EditorSession& session, int n_chars);

EditStatus erase_words_before(EditorSession& session, int n_words);

EditStatus goto_line(EditorSession& session, int line_1based, int col_1based = 1);

EditStatus move_left(EditorSession& session);

EditStatus move_right(EditorSession& session);

EditStatus move_up(EditorSession& session);

EditStatus move_down(EditorSession& session);

EditStatus move_home(EditorSession& session);

EditStatus move_end(EditorSession& session);

void insert_newline_at_cursor(EditorSession& session);

EditStatus join_with_previous_line(EditorSession& session);

EditStatus delete_current_line(EditorSession& session);

EditStatus delete_at_cursor(EditorSession& session);

bool find_text(EditorSession& session, const std::string& needle);

bool find_next(EditorSession& session);

EditStatus yank_line(EditorSession& session);

EditStatus paste_line(EditorSession& session);

void editor_clear_selection(EditorSession& session);

void editor_set_selection_anchor_to_cursor(EditorSession& session);

void editor_get_normalized_selection(const EditorSession& session, std::size_t& a_line,
				     std::size_t& a_col, std::size_t& b_line, std::size_t& b_col);

bool editor_selection_active(const EditorSession& session);

EditStatus editor_delete_selection(EditorSession& session);

void editor_toggle_style_flag(EditorSession& session, uint8_t flag);

uint8_t editor_active_style_flags(const EditorSession& session);

std::string format_cursor_position(const EditorSession& session);

std::string format_note_for_display(const EditorSession& session);

std::string format_line_with_cursor(const EditorSession& session, std::size_t line_index);
