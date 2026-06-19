#include "note_editor.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <iostream>
#include <sstream>

namespace {
bool parse_positive_int(const std::string& s, int& out) {
    try {
	const long v = std::stol(s);
	if (v <= 0 || v > static_cast<long>(INT_MAX)) {
	    return false;
	}
	out = static_cast<int>(v);
	return true;
    } catch (const std::exception&) {
	return false;
    }
}

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

void erase_word_before_cursor(std::string& line, std::size_t& col) {
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

void erase_chars_before_cursor(std::string& line, std::size_t& col, int n_chars) {
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

void print_cursor_status(const EditorSession& session) {
    std::cout << "Cursor on line " << (session.current_line + 1)
	      << ", column " << (session.current_column + 1) << ".\n";
}
} // namespace

void editor_reset_cursor(EditorSession& session) {
    session.current_column = 0;
    if (session.note.text.empty()) {
	session.current_line = 0;
	return;
    }
    session.current_line = session.note.text.size() - 1;
    session.current_column = current_line_length(session);
}

void write_to_current_line(EditorSession& session, const std::string& text) {
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

void erase_from_current_line(EditorSession& session, const std::vector<std::string>& fields) {
    if (!current_line_exists(session)) {
	std::cout << "Error: No line at the cursor to erase. Use goto or write first.\n";
	return;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);

    if (fields.size() == 1) {
	if (session.current_column == 0) {
	    std::cout << "Error: Nothing before the cursor to erase.\n";
	    return;
	}
	line.erase(session.current_column - 1, 1);
	--session.current_column;
	std::cout << "Deleted character before cursor.\n";
	touch_edit(session);
	return;
    }

    if (fields[1] == "char") {
	if (fields.size() == 2) {
	    if (session.current_column == 0) {
		std::cout << "Error: Nothing before the cursor to erase.\n";
		return;
	    }
	    line.erase(session.current_column - 1, 1);
	    --session.current_column;
	    std::cout << "Deleted character before cursor.\n";
	    touch_edit(session);
	    return;
	}

	if (fields.size() == 3) {
	    int n_chars = 0;
	    if (!parse_positive_int(fields[2], n_chars)) {
		std::cout << "Error: Invalid character count.\n";
		return;
	    }
	    erase_chars_before_cursor(line, session.current_column, n_chars);
	    std::cout << "Deleted " << n_chars << " character(s) before cursor.\n";
	    touch_edit(session);
	    return;
	}

	std::cout << "Error: Too many arguments for erase char.\n";
	return;
    }

    if (fields[1] == "word") {
	int n_words = 1;
	if (fields.size() == 3) {
	    if (!parse_positive_int(fields[2], n_words)) {
		std::cout << "Error: Invalid word count.\n";
		return;
	    }
	} else if (fields.size() > 3) {
	    std::cout << "Error: Too many arguments for erase word.\n";
	    return;
	}

	const std::size_t col_before = session.current_column;
	for (int i = 0; i < n_words; ++i) {
	    if (session.current_column == 0) {
		break;
	    }
	    const std::size_t before = session.current_column;
	    erase_word_before_cursor(line, session.current_column);
	    if (session.current_column == before) {
		break;
	    }
	}

	if (session.current_column == col_before) {
	    std::cout << "Error: No word before the cursor to erase.\n";
	    return;
	}

	std::cout << "Deleted word(s) before cursor.\n";
	touch_edit(session);
	return;
    }

    std::cout << "Error: Second argument to erase must be 'char' or 'word'.\n";
}

bool goto_line(EditorSession& session, int line_1based, int col_1based) {
    if (line_1based < 1) {
	std::cout << "Error: Line numbers start at 1.\n";
	return false;
    }
    if (col_1based < 1) {
	std::cout << "Error: Column numbers start at 1.\n";
	return false;
    }

    const auto line_count = session.note.text.size();
    const auto max_goto = line_count + 1;
    if (static_cast<std::size_t>(line_1based) > max_goto) {
	std::cout << "Error: Line " << line_1based << " is out of range (1–"
		  << max_goto << ").\n";
	return false;
    }

    session.current_line = static_cast<std::size_t>(line_1based - 1);

    if (!current_line_exists(session)) {
	session.current_column = 0;
	std::cout << "Cursor on line " << line_1based << " (new line), column 1.\n";
	return true;
    }

    const std::size_t max_col = current_line_length(session);
    const std::size_t col_0 = static_cast<std::size_t>(col_1based - 1);
    if (col_0 > max_col) {
	std::cout << "Error: Column " << col_1based << " is out of range (1–"
		  << (max_col + 1) << ") on line " << line_1based << ".\n";
	return false;
    }

    session.current_column = col_0;
    print_cursor_status(session);
    return true;
}

void move_left(EditorSession& session) {
    if (current_line_exists(session) && session.current_column > 0) {
	--session.current_column;
	print_cursor_status(session);
	return;
    }

    if (session.current_line == 0) {
	std::cout << "Already at start of note.\n";
	return;
    }

    --session.current_line;
    session.current_column = current_line_length(session);
    print_cursor_status(session);
}

void move_right(EditorSession& session) {
    if (current_line_exists(session) && session.current_column < current_line_length(session)) {
	++session.current_column;
	print_cursor_status(session);
	return;
    }

    const auto next_line = session.current_line + 1;
    if (next_line > session.note.text.size()) {
	std::cout << "Already at end of note.\n";
	return;
    }

    session.current_line = next_line;
    session.current_column = current_line_exists(session) ? 0 : 0;
    print_cursor_status(session);
}

void move_home(EditorSession& session) {
    session.current_column = 0;
    print_cursor_status(session);
}

void move_end(EditorSession& session) {
    session.current_column = current_line_length(session);
    print_cursor_status(session);
}

void insert_newline_at_cursor(EditorSession& session) {
    if (!current_line_exists(session)) {
	session.note.text.push_back("");
	session.current_line = session.note.text.size() - 1;
	session.current_column = 0;
	touch_edit(session);
	print_cursor_status(session);
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
    touch_edit(session);
    print_cursor_status(session);
}

bool delete_current_line(EditorSession& session) {
    if (!current_line_exists(session)) {
	std::cout << "Error: No line at the cursor to delete.\n";
	return false;
    }

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
    print_cursor_status(session);
    return true;
}

void delete_at_cursor(EditorSession& session) {
    if (!current_line_exists(session)) {
	std::cout << "Error: No line at the cursor.\n";
	return;
    }

    clamp_column_to_line(session);
    std::string& line = current_line_ref(session);
    if (session.current_column >= line.size()) {
	std::cout << "Error: Nothing at the cursor to delete.\n";
	return;
    }

    line.erase(session.current_column, 1);
    touch_edit(session);
    std::cout << "Deleted character at cursor.\n";
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

void show_note(const EditorSession& session) {
    if (session.note.text.empty() && session.current_line == 0) {
	std::cout << "(empty note — cursor on line 1, column 1)\n";
	return;
    }

    for (std::size_t i = 0; i < session.note.text.size(); ++i) {
	const bool here = (i == session.current_line);
	std::cout << (here ? "> " : "  ") << (i + 1) << ": "
		  << format_line_with_cursor(session, i) << "\n";
    }

    if (session.current_line == session.note.text.size()) {
	std::cout << "> " << (session.note.text.size() + 1) << ": |\n";
    }
}
