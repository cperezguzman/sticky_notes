#include "note_editor.h"

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

void erase_suffix_until_space(std::string& line) {
    while (!line.empty() && line.back() != ' ') {
	line.pop_back();
    }
}

void erase_delimiter_space(std::string& line) {
    if (!line.empty() && line.back() == ' ') {
	line.pop_back();
    }
}

int count_words(const std::string& line) {
    std::istringstream iss(line);
    std::string w;
    int n = 0;
    while (iss >> w) {
	++n;
    }
    return n;
}

bool current_line_exists(const EditorSession& session) {
    return session.current_line < session.note.text.size();
}

std::string& current_line_ref(EditorSession& session) {
    return session.note.text[session.current_line];
}
} // namespace

void editor_reset_cursor(EditorSession& session) {
    if (session.note.text.empty()) {
	session.current_line = 0;
    } else {
	session.current_line = session.note.text.size() - 1;
    }
}

void write_to_current_line(EditorSession& session, const std::string& text) {
    if (session.current_line < session.note.text.size()) {
	session.note.text[session.current_line] = text;
    } else {
	session.note.text.push_back(text);
	session.current_line = session.note.text.size() - 1;
    }
    update_last_edit(session.note);
}

void append_to_current_line(EditorSession& session, const std::string& text) {
    if (session.current_line < session.note.text.size()) {
	session.note.text[session.current_line] += text;
    } else {
	session.note.text.push_back(text);
	session.current_line = session.note.text.size() - 1;
    }
    update_last_edit(session.note);
}

void erase_from_current_line(EditorSession& session, const std::vector<std::string>& fields) {
    if (!current_line_exists(session)) {
	std::cout << "Error: No line at the cursor to erase. Use goto or write first.\n";
	return;
    }

    std::string& line = current_line_ref(session);

    if (fields.size() == 1) {
	erase_suffix_until_space(line);
	std::cout << "The most recent word has been deleted.\n";
	update_last_edit(session.note);
	return;
    }

    if (fields[1] == "char") {
	if (fields.size() == 2) {
	    if (line.empty()) {
		std::cout << "Error: Current line is empty; nothing to erase.\n";
		return;
	    }
	    line.pop_back();
	    std::cout << "The most recent character has been deleted.\n";
	    update_last_edit(session.note);
	    return;
	}

	if (fields.size() == 3) {
	    int n_chars = 0;
	    if (!parse_positive_int(fields[2], n_chars)) {
		std::cout << "Error: Invalid character count.\n";
		return;
	    }
	    const auto max_take = static_cast<int>(line.size());
	    if (n_chars > max_take) {
		n_chars = max_take;
	    }
	    line.resize(line.size() - static_cast<std::size_t>(n_chars));
	    std::cout << "The last " << n_chars << " character(s) have been deleted.\n";
	    update_last_edit(session.note);
	    return;
	}

	std::cout << "Error: Too many arguments for erase char.\n";
	return;
    }

    if (fields[1] == "word") {
	if (fields.size() == 2) {
	    erase_suffix_until_space(line);
	    std::cout << "The most recent word has been deleted.\n";
	    update_last_edit(session.note);
	    return;
	}

	if (fields.size() == 3) {
	    int n_words = 0;
	    if (!parse_positive_int(fields[2], n_words)) {
		std::cout << "Error: Invalid word count.\n";
		return;
	    }
	    int available = count_words(line);
	    if (available == 0) {
		std::cout << "Error: No words on the current line.\n";
		return;
	    }
	    if (n_words > available) {
		n_words = available;
	    }
	    for (int i = 0; i < n_words; ++i) {
		if (line.empty()) {
		    break;
		}
		erase_suffix_until_space(line);
		erase_delimiter_space(line);
	    }
	    std::cout << "The last " << n_words << " word(s) have been deleted.\n";
	    update_last_edit(session.note);
	    return;
	}

	std::cout << "Error: Too many arguments for erase word.\n";
	return;
    }

    std::cout << "Error: Second argument to erase must be 'char' or 'word'.\n";
}

bool goto_line(EditorSession& session, int line_1based) {
    if (line_1based < 1) {
	std::cout << "Error: Line numbers start at 1.\n";
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
    std::cout << "Cursor on line " << line_1based;
    if (session.current_line == line_count) {
	std::cout << " (new line)";
    }
    std::cout << ".\n";
    return true;
}

void insert_newline_at_cursor(EditorSession& session) {
    if (session.current_line < session.note.text.size()) {
	session.note.text.insert(session.note.text.begin()
				+ static_cast<std::ptrdiff_t>(session.current_line + 1),
				"");
	session.current_line += 1;
    } else {
	session.note.text.push_back("");
	session.current_line = session.note.text.size() - 1;
    }
    update_last_edit(session.note);
    std::cout << "Cursor on line " << (session.current_line + 1) << ".\n";
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
    } else if (session.current_line >= session.note.text.size()) {
	session.current_line = session.note.text.size() - 1;
    }

    update_last_edit(session.note);
    std::cout << "Line deleted. Cursor on line " << (session.current_line + 1) << ".\n";
    return true;
}

void show_note(const EditorSession& session) {
    if (session.note.text.empty()) {
	std::cout << "(empty note — cursor on line 1, new line)\n";
	return;
    }

    for (std::size_t i = 0; i < session.note.text.size(); ++i) {
	const bool here = (i == session.current_line);
	std::cout << (here ? "> " : "  ") << (i + 1) << ": " << session.note.text[i] << "\n";
    }

    if (session.current_line == session.note.text.size()) {
	std::cout << "> " << (session.note.text.size() + 1) << ": (new line)\n";
    }
}
