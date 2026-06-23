#include "note_editor.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <sstream>

namespace {
constexpr std::size_t kMaxUndoDepth = 100;

std::string g_line_clipboard;

bool current_line_exists(const EditorSession& session) {
    return session.current_line < session.note.text.size();
}

std::string& current_line_ref(EditorSession& session) {
    return session.note.text[session.current_line];
}

const std::string& current_line_ref(const EditorSession& session) {
    return session.note.text[session.current_line];
}

std::size_t current_line_length(const EditorSession& session) {
    if (!current_line_exists(session)) {
	return 0;
    }
    return current_line_ref(session).size();
}

void clamp_column_to_line(EditorSession& session) {
    const std::size_t max_col = current_line_length(session);
    if (session.current_column > max_col) {
	session.current_column = max_col;
    }
}

void touch_edit(EditorSession& session) {
    update_last_edit(session.note);
}

EditorSnapshot make_snapshot(const EditorSession& session) {
    return EditorSnapshot{session.note.text, session.hard_line_break_after, session.current_line,
			  session.current_column};
}

void apply_snapshot(EditorSession& session, const EditorSnapshot& snap) {
    session.note.text = snap.text;
    session.hard_line_break_after = snap.hard_line_break_after;
    session.current_line = snap.current_line;
    session.current_column = snap.current_column;
    clamp_column_to_line(session);
    touch_edit(session);
}

void editor_push_undo(EditorSession& session) {
    session.undo_stack.push_back(make_snapshot(session));
    if (session.undo_stack.size() > kMaxUndoDepth) {
	session.undo_stack.erase(session.undo_stack.begin());
    }
    session.redo_stack.clear();
}

void erase_word_before_in_line(std::string& line, std::size_t& col) {
    if (col == 0) {
	return;
    }

    std::size_t end = col;
    while (end > 0 && line[end - 1] == ' ') {
	--end;
    }
    if (end == 0) {
	return;
    }

    std::size_t start = end;
    while (start > 0 && line[start - 1] != ' ') {
	--start;
    }

    line.erase(start, end - start);
    col = start;
}

void erase_chars_before_in_line(std::string& line, std::size_t& col, int n_chars) {
    if (n_chars <= 0 || col == 0) {
	return;
    }
    const int max_take = static_cast<int>(col);
    if (n_chars > max_take) {
	n_chars = max_take;
    }
    col -= static_cast<std::size_t>(n_chars);
    line.erase(col, static_cast<std::size_t>(n_chars));
}

bool search_from(EditorSession& session, const std::string& needle, std::size_t start_line,
		 std::size_t start_col, std::size_t wrap_line, std::size_t wrap_col) {
    if (session.note.text.empty()) {
	return false;
    }

    const std::size_t nlines = session.note.text.size();
    for (std::size_t offset = 0; offset < nlines; ++offset) {
	const std::size_t line_idx = (start_line + offset) % nlines;
	const std::string& line = session.note.text[line_idx];
	const std::size_t from = (offset == 0) ? start_col : 0;
	const std::size_t pos = line.find(needle, from);
	if (pos != std::string::npos) {
	    session.current_line = line_idx;
	    session.current_column = pos;
	    return true;
	}
    }

    for (std::size_t line_idx = 0; line_idx < nlines; ++line_idx) {
	const std::string& line = session.note.text[line_idx];
	const std::size_t limit = (line_idx == wrap_line) ? wrap_col : line.size();
	if (limit == 0) {
	    continue;
	}
	const std::size_t pos = line.find(needle, 0);
	if (pos != std::string::npos && pos < limit) {
	    session.current_line = line_idx;
	    session.current_column = pos;
	    return true;
	}
    }

    return false;
}
} // namespace

std::string format_cursor_position(const EditorSession& session) {
    std::ostringstream out;
    out << "Cursor on line " << (session.current_line + 1)
	<< ", column " << (session.current_column + 1) << ".";
    return out.str();
}

void editor_reset_cursor(EditorSession& session) {
    session.current_column = 0;
    if (session.note.text.empty()) {
	session.current_line = 0;
	return;
    }
    session.current_line = session.note.text.size() - 1;
    session.current_column = current_line_length(session);
}

void editor_clear_history(EditorSession& session) {
    session.undo_stack.clear();
    session.redo_stack.clear();
    session.find_needle.clear();
    session.find_active = false;
}

EditStatus editor_undo(EditorSession& session) {
    if (session.undo_stack.empty()) {
	return EditStatus::NothingToUndo;
    }

    session.redo_stack.push_back(make_snapshot(session));
    const EditorSnapshot snap = session.undo_stack.back();
    session.undo_stack.pop_back();
    apply_snapshot(session, snap);
    return EditStatus::Ok;
}

EditStatus editor_redo(EditorSession& session) {
    if (session.redo_stack.empty()) {
	return EditStatus::NothingToRedo;
    }

    session.undo_stack.push_back(make_snapshot(session));
    const EditorSnapshot snap = session.redo_stack.back();
    session.redo_stack.pop_back();
    apply_snapshot(session, snap);
    return EditStatus::Ok;
}

void write_to_current_line(EditorSession& session, const std::string& text) {
    editor_push_undo(session);
    if (session.current_line < session.note.text.size()) {
	session.note.text[session.current_line] = text;
    } else {
	session.note.text.push_back(text);
	session.current_line = session.note.text.size() - 1;
    }
    session.current_column = text.size();
    touch_edit(session);
}

void append_to_current_line(EditorSession& session, const std::string& text) {
    editor_push_undo(session);
    if (session.current_line < session.note.text.size()) {
	session.note.text[session.current_line] += text;
    } else {
	session.note.text.push_back(text);
	session.current_line = session.note.text.size() - 1;
    }
    session.current_column = current_line_length(session);
    touch_edit(session);
}

void insert_at_cursor(EditorSession& session, const std::string& text) {
    editor_push_undo(session);
    if (!current_line_exists(session)) {
	session.note.text.push_back(text);
	session.current_line = 0;
	session.current_column = text.size();
	touch_edit(session);
	return;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);
    line.insert(session.current_column, text);
    session.current_column += text.size();
    touch_edit(session);
}

EditStatus erase_char_before(EditorSession& session) {
    if (!current_line_exists(session)) {
	return EditStatus::NoLineAtCursor;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);

    if (session.current_column == 0) {
	return EditStatus::NothingBeforeCursor;
    }

    editor_push_undo(session);
    line.erase(session.current_column - 1, 1);
    --session.current_column;
    touch_edit(session);
    return EditStatus::Ok;
}

EditStatus erase_chars_before(EditorSession& session, int n_chars) {
    if (!current_line_exists(session)) {
	return EditStatus::NoLineAtCursor;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);

    if (session.current_column == 0 || n_chars <= 0) {
	return EditStatus::NothingBeforeCursor;
    }

    editor_push_undo(session);
    erase_chars_before_in_line(line, session.current_column, n_chars);
    touch_edit(session);
    return EditStatus::Ok;
}

EditStatus erase_words_before(EditorSession& session, int n_words) {
    if (!current_line_exists(session)) {
	return EditStatus::NoLineAtCursor;
    }
    if (n_words <= 0) {
	return EditStatus::NothingBeforeCursor;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);

    const std::size_t col_before = session.current_column;
    std::string trial = line;
    std::size_t trial_col = session.current_column;
    for (int i = 0; i < n_words; ++i) {
	if (trial_col == 0) {
	    break;
	}
	const std::size_t before = trial_col;
	erase_word_before_in_line(trial, trial_col);
	if (trial_col == before) {
	    break;
	}
    }

    if (trial_col == col_before) {
	return EditStatus::NothingBeforeCursor;
    }

    editor_push_undo(session);
    line = std::move(trial);
    session.current_column = trial_col;
    touch_edit(session);
    return EditStatus::Ok;
}

EditStatus goto_line(EditorSession& session, int line_1based, int col_1based) {
    if (line_1based < 1) {
	return EditStatus::LineNumberTooSmall;
    }
    if (col_1based < 1) {
	return EditStatus::ColumnNumberTooSmall;
    }

    const auto line_count = session.note.text.size();
    const auto max_goto = line_count + 1;
    if (static_cast<std::size_t>(line_1based) > max_goto) {
	return EditStatus::LineOutOfRange;
    }

    session.current_line = static_cast<std::size_t>(line_1based - 1);

    if (!current_line_exists(session)) {
	session.current_column = 0;
	return EditStatus::Ok;
    }

    const std::size_t max_col = current_line_length(session);
    const std::size_t col_0 = static_cast<std::size_t>(col_1based - 1);
    if (col_0 > max_col) {
	return EditStatus::ColumnOutOfRange;
    }

    session.current_column = col_0;
    return EditStatus::Ok;
}

EditStatus move_left(EditorSession& session) {
    if (current_line_exists(session) && session.current_column > 0) {
	--session.current_column;
	return EditStatus::Ok;
    }

    if (session.current_line == 0) {
	return EditStatus::AtStart;
    }

    --session.current_line;
    session.current_column = current_line_length(session);
    return EditStatus::Ok;
}

EditStatus move_right(EditorSession& session) {
    if (current_line_exists(session) && session.current_column < current_line_length(session)) {
	++session.current_column;
	return EditStatus::Ok;
    }

    const auto next_line = session.current_line + 1;
    if (next_line > session.note.text.size()) {
	return EditStatus::AtEnd;
    }

    session.current_line = next_line;
    session.current_column = 0;
    return EditStatus::Ok;
}

EditStatus move_up(EditorSession& session) {
    if (session.current_line == 0) {
	return EditStatus::AtStart;
    }

    const std::size_t target_col = session.current_column;
    --session.current_line;
    const std::size_t len = current_line_length(session);
    session.current_column = std::min(target_col, len);
    return EditStatus::Ok;
}

EditStatus move_down(EditorSession& session) {
    const std::size_t target_col = session.current_column;
    const auto next_line = session.current_line + 1;
    if (next_line > session.note.text.size()) {
	return EditStatus::AtEnd;
    }

    session.current_line = next_line;
    const std::size_t len = current_line_length(session);
    session.current_column = std::min(target_col, len);
    return EditStatus::Ok;
}

EditStatus move_home(EditorSession& session) {
    session.current_column = 0;
    return EditStatus::Ok;
}

EditStatus move_end(EditorSession& session) {
    session.current_column = current_line_length(session);
    return EditStatus::Ok;
}

void insert_newline_at_cursor(EditorSession& session) {
    editor_push_undo(session);
    if (!current_line_exists(session)) {
	session.note.text.push_back("");
	session.current_line = session.note.text.size() - 1;
	session.current_column = 0;
	touch_edit(session);
	return;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);
    const std::string tail = line.substr(session.current_column);
    line.resize(session.current_column);

    session.note.text.insert(session.note.text.begin()
			     + static_cast<std::ptrdiff_t>(session.current_line + 1),
			     tail);
    session.current_line += 1;
    session.current_column = 0;

    session.hard_line_break_after.resize(session.note.text.size(), false);
    session.hard_line_break_after[session.current_line - 1] = true;

    touch_edit(session);
}

EditStatus join_with_previous_line(EditorSession& session) {
    if (!current_line_exists(session) || session.current_line == 0) {
	return EditStatus::NothingBeforeCursor;
    }

    editor_push_undo(session);
    std::string& prev = session.note.text[session.current_line - 1];
    const std::size_t join_at = prev.size();
    prev += current_line_ref(session);
    session.note.text.erase(session.note.text.begin()
			    + static_cast<std::ptrdiff_t>(session.current_line));
    if (session.current_line > 0
	&& session.current_line - 1 < session.hard_line_break_after.size()) {
	session.hard_line_break_after.erase(
	    session.hard_line_break_after.begin()
	    + static_cast<std::ptrdiff_t>(session.current_line - 1));
    }
    session.current_line -= 1;
    session.current_column = join_at;
    touch_edit(session);
    return EditStatus::Ok;
}

EditStatus delete_current_line(EditorSession& session) {
    if (!current_line_exists(session)) {
	return EditStatus::NoLineAtCursor;
    }

    editor_push_undo(session);
    session.note.text.erase(session.note.text.begin()
			    + static_cast<std::ptrdiff_t>(session.current_line));

    if (session.note.text.empty()) {
	session.current_line = 0;
	session.current_column = 0;
    } else if (session.current_line >= session.note.text.size()) {
	session.current_line = session.note.text.size() - 1;
	clamp_column_to_line(session);
    } else {
	clamp_column_to_line(session);
    }

    touch_edit(session);
    return EditStatus::Ok;
}

EditStatus delete_at_cursor(EditorSession& session) {
    if (!current_line_exists(session)) {
	return EditStatus::NoLineAtCursor;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);
    if (session.current_column >= line.size()) {
	return EditStatus::NothingAtCursor;
    }

    editor_push_undo(session);
    line.erase(session.current_column, 1);
    touch_edit(session);
    return EditStatus::Ok;
}

bool find_text(EditorSession& session, const std::string& needle) {
    if (needle.empty()) {
	return false;
    }
    if (session.note.text.empty()) {
	return false;
    }

    session.find_needle = needle;
    session.find_active = true;

    const std::size_t wrap_line = session.current_line;
    const std::size_t wrap_col = session.current_column;
    return search_from(session, needle, wrap_line, wrap_col, wrap_line, wrap_col);
}

bool find_next(EditorSession& session) {
    if (!session.find_active || session.find_needle.empty()) {
	return false;
    }

    const std::string& needle = session.find_needle;
    const std::size_t wrap_line = session.current_line;
    const std::size_t wrap_col = session.current_column;

    std::size_t start_line = session.current_line;
    std::size_t start_col = session.current_column + needle.size();
    if (current_line_exists(session)
	&& start_col > current_line_ref(session).size()) {
	start_col = 0;
	start_line = (start_line + 1) % session.note.text.size();
    }

    return search_from(session, needle, start_line, start_col, wrap_line, wrap_col);
}

EditStatus yank_line(EditorSession& session) {
    if (!current_line_exists(session)) {
	return EditStatus::NoLineToYank;
    }

    g_line_clipboard = session.note.text[session.current_line];
    return EditStatus::Ok;
}

EditStatus paste_line(EditorSession& session) {
    if (g_line_clipboard.empty()) {
	return EditStatus::ClipboardEmpty;
    }

    editor_push_undo(session);
    const std::size_t insert_at = session.current_line + 1;
    if (insert_at > session.note.text.size()) {
	session.note.text.push_back(g_line_clipboard);
    } else {
	session.note.text.insert(session.note.text.begin()
				+ static_cast<std::ptrdiff_t>(insert_at),
				g_line_clipboard);
    }
    session.current_line = insert_at;
    session.current_column = g_line_clipboard.size();
    touch_edit(session);
    return EditStatus::Ok;
}

std::string format_line_with_cursor(const EditorSession& session, std::size_t line_index) {
    if (line_index >= session.note.text.size()) {
	return "(new line)";
    }

    const std::string& line = session.note.text[line_index];
    if (line_index != session.current_line) {
	return line;
    }

    const std::size_t col = std::min(session.current_column, line.size());
    return line.substr(0, col) + "|" + line.substr(col);
}

std::string format_note_for_display(const EditorSession& session) {
    std::ostringstream out;
    if (session.note.text.empty() && session.current_line == 0) {
	out << "(empty note — cursor on line 1, column 1)\n";
	return out.str();
    }

    for (std::size_t i = 0; i < session.note.text.size(); ++i) {
	const bool here = (i == session.current_line);
	out << (here ? "> " : "  ") << (i + 1) << ": "
	    << format_line_with_cursor(session, i) << "\n";
    }

    if (session.current_line == session.note.text.size()) {
	out << "> " << (session.note.text.size() + 1) << ": |\n";
    }

    return out.str();
}
