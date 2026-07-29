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
    return EditorSnapshot{session.note.text, session.hard_line_break_after, session.note.styles,
			  session.current_line, session.current_column, session.typing_style};
}

void apply_snapshot(EditorSession& session, const EditorSnapshot& snap) {
    session.note.text = snap.text;
    session.hard_line_break_after = snap.hard_line_break_after;
    session.note.styles = snap.styles;
    session.current_line = snap.current_line;
    session.current_column = snap.current_column;
    session.typing_style = snap.typing_style;
    session.has_selection = false;
    note_styles_ensure(session.note.styles, session.note.text);
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
    const bool replaced_selection = editor_selection_active(session);
    if (replaced_selection) {
	editor_delete_selection(session);
    } else {
	editor_push_undo(session);
    }
    note_styles_ensure(session.note.styles, session.note.text);
    if (!current_line_exists(session)) {
	session.note.text.push_back(text);
	session.current_line = 0;
	session.current_column = text.size();
	note_styles_ensure(session.note.styles, session.note.text);
	for (std::size_t i = 0; i < text.size(); ++i) {
	    session.note.styles.lines[0][i] = session.typing_style;
	}
	touch_edit(session);
	return;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);
    line.insert(session.current_column, text);
    note_styles_insert(session.note.styles, session.current_line, session.current_column,
		       text.size(), session.typing_style);
    session.current_column += text.size();
    editor_clear_selection(session);
    touch_edit(session);
}

EditStatus erase_char_before(EditorSession& session) {
    if (editor_selection_active(session)) {
	return editor_delete_selection(session);
    }
    if (!current_line_exists(session)) {
	return EditStatus::NoLineAtCursor;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);

    if (session.current_column == 0) {
	return EditStatus::NothingBeforeCursor;
    }

    editor_push_undo(session);
    note_styles_erase(session.note.styles, session.current_line, session.current_column - 1, 1);
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
    const bool replaced_selection = editor_selection_active(session);
    if (replaced_selection) {
	editor_delete_selection(session);
    } else {
	editor_push_undo(session);
    }
    note_styles_ensure(session.note.styles, session.note.text);
    if (!current_line_exists(session)) {
	session.note.text.push_back("");
	session.current_line = session.note.text.size() - 1;
	session.current_column = 0;
	note_styles_ensure(session.note.styles, session.note.text);
	touch_edit(session);
	return;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);
    const std::string tail = line.substr(session.current_column);
    line.resize(session.current_column);
    note_styles_split_line(session.note.styles, session.current_line, session.current_column);

    session.note.text.insert(session.note.text.begin()
			     + static_cast<std::ptrdiff_t>(session.current_line + 1),
			     tail);
    session.current_line += 1;
    session.current_column = 0;

    session.hard_line_break_after.resize(session.note.text.size(), false);
    session.hard_line_break_after[session.current_line - 1] = true;
    note_styles_ensure(session.note.styles, session.note.text);
    editor_clear_selection(session);
    touch_edit(session);
}

EditStatus join_with_previous_line(EditorSession& session) {
    if (!current_line_exists(session) || session.current_line == 0) {
	return EditStatus::NothingBeforeCursor;
    }

    editor_push_undo(session);
    note_styles_ensure(session.note.styles, session.note.text);
    std::string& prev = session.note.text[session.current_line - 1];
    const std::size_t join_at = prev.size();
    prev += current_line_ref(session);
    note_styles_join_with_previous(session.note.styles, session.current_line);
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
    note_styles_ensure(session.note.styles, session.note.text);
    editor_clear_selection(session);
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
    if (editor_selection_active(session)) {
	return editor_delete_selection(session);
    }
    if (!current_line_exists(session)) {
	return EditStatus::NoLineAtCursor;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);
    if (session.current_column >= line.size()) {
	return EditStatus::NothingAtCursor;
    }

    editor_push_undo(session);
    note_styles_erase(session.note.styles, session.current_line, session.current_column, 1);
    line.erase(session.current_column, 1);
    touch_edit(session);
    return EditStatus::Ok;
}

void editor_clear_selection(EditorSession& session) {
    session.has_selection = false;
    session.sel_anchor_line = session.current_line;
    session.sel_anchor_column = session.current_column;
}

void editor_set_selection_anchor_to_cursor(EditorSession& session) {
    session.sel_anchor_line = session.current_line;
    session.sel_anchor_column = session.current_column;
    session.has_selection = true;
}

bool editor_selection_active(const EditorSession& session) {
    if (!session.has_selection) {
	return false;
    }
    return !(session.sel_anchor_line == session.current_line
	     && session.sel_anchor_column == session.current_column);
}

void editor_get_normalized_selection(const EditorSession& session, std::size_t& a_line,
				     std::size_t& a_col, std::size_t& b_line, std::size_t& b_col) {
    a_line = session.sel_anchor_line;
    a_col = session.sel_anchor_column;
    b_line = session.current_line;
    b_col = session.current_column;
    if (a_line > b_line || (a_line == b_line && a_col > b_col)) {
	std::swap(a_line, b_line);
	std::swap(a_col, b_col);
    }
    if (!session.note.text.empty()) {
	a_line = std::min(a_line, session.note.text.size() - 1);
	b_line = std::min(b_line, session.note.text.size() - 1);
	a_col = std::min(a_col, session.note.text[a_line].size());
	b_col = std::min(b_col, session.note.text[b_line].size());
    }
}

EditStatus editor_delete_selection(EditorSession& session) {
    if (!editor_selection_active(session)) {
	return EditStatus::NothingAtCursor;
    }
    editor_push_undo(session);
    std::size_t a_line = 0;
    std::size_t a_col = 0;
    std::size_t b_line = 0;
    std::size_t b_col = 0;
    editor_get_normalized_selection(session, a_line, a_col, b_line, b_col);
    note_styles_ensure(session.note.styles, session.note.text);

    if (a_line == b_line) {
	session.note.text[a_line].erase(a_col, b_col - a_col);
	note_styles_erase(session.note.styles, a_line, a_col, b_col - a_col);
    } else {
	std::string head = session.note.text[a_line].substr(0, a_col);
	std::string tail = session.note.text[b_line].substr(b_col);
	std::vector<uint8_t> head_s;
	std::vector<uint8_t> tail_s;
	if (a_line < session.note.styles.lines.size()) {
	    const auto& row = session.note.styles.lines[a_line];
	    head_s.assign(row.begin(),
			  row.begin() + static_cast<std::ptrdiff_t>(std::min(a_col, row.size())));
	}
	if (b_line < session.note.styles.lines.size()) {
	    const auto& row = session.note.styles.lines[b_line];
	    if (b_col < row.size()) {
		tail_s.assign(row.begin() + static_cast<std::ptrdiff_t>(b_col), row.end());
	    }
	}
	session.note.text[a_line] = head + tail;
	head_s.insert(head_s.end(), tail_s.begin(), tail_s.end());
	session.note.styles.lines[a_line] = std::move(head_s);

	session.note.text.erase(session.note.text.begin()
				    + static_cast<std::ptrdiff_t>(a_line + 1),
				session.note.text.begin()
				    + static_cast<std::ptrdiff_t>(b_line + 1));
	if (a_line + 1 < session.note.styles.lines.size()) {
	    session.note.styles.lines.erase(
		session.note.styles.lines.begin() + static_cast<std::ptrdiff_t>(a_line + 1),
		session.note.styles.lines.begin()
		    + static_cast<std::ptrdiff_t>(
			std::min(b_line + 1, session.note.styles.lines.size())));
	}
	if (a_line < session.hard_line_break_after.size()) {
	    const std::size_t erase_end =
		std::min(b_line, session.hard_line_break_after.size());
	    if (a_line < erase_end) {
		session.hard_line_break_after.erase(
		    session.hard_line_break_after.begin()
			+ static_cast<std::ptrdiff_t>(a_line),
		    session.hard_line_break_after.begin()
			+ static_cast<std::ptrdiff_t>(erase_end));
	    }
	}
    }

    session.current_line = a_line;
    session.current_column = a_col;
    note_styles_ensure(session.note.styles, session.note.text);
    editor_clear_selection(session);
    touch_edit(session);
    return EditStatus::Ok;
}

void editor_toggle_style_flag(EditorSession& session, uint8_t flag) {
    if (editor_selection_active(session)) {
	editor_push_undo(session);
	std::size_t a_line = 0;
	std::size_t a_col = 0;
	std::size_t b_line = 0;
	std::size_t b_col = 0;
	editor_get_normalized_selection(session, a_line, a_col, b_line, b_col);
	note_styles_toggle_range(session.note.styles, session.note.text, a_line, a_col, b_line,
				 b_col, flag);
	touch_edit(session);
	return;
    }
    if ((session.typing_style & flag) != 0) {
	session.typing_style = static_cast<uint8_t>(session.typing_style & static_cast<uint8_t>(~flag));
    } else {
	session.typing_style = static_cast<uint8_t>(session.typing_style | flag);
    }
}

uint8_t editor_active_style_flags(const EditorSession& session) {
    if (editor_selection_active(session)) {
	std::size_t a_line = 0;
	std::size_t a_col = 0;
	std::size_t b_line = 0;
	std::size_t b_col = 0;
	editor_get_normalized_selection(session, a_line, a_col, b_line, b_col);
	return note_styles_flags_in_range(session.note.styles, session.note.text, a_line, a_col,
					  b_line, b_col);
    }
    return session.typing_style;
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
