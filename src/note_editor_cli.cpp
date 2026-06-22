#include "note_editor_cli.h"

#include <climits>
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

void print_cursor_if_ok(const EditorSession& session) {
    std::cout << format_cursor_position(session) << "\n";
}
} // namespace

void cli_show_cursor(const EditorSession& session) {
    std::cout << format_cursor_position(session) << "\n";
}

void cli_show_note(const EditorSession& session) {
    std::cout << format_note_for_display(session);
}

void cli_undo(EditorSession& session) {
    const EditStatus status = editor_undo(session);
    if (status == EditStatus::NothingToUndo) {
	std::cout << "Nothing to undo.\n";
	return;
    }
    std::cout << "Undone.\n";
    print_cursor_if_ok(session);
}

void cli_redo(EditorSession& session) {
    const EditStatus status = editor_redo(session);
    if (status == EditStatus::NothingToRedo) {
	std::cout << "Nothing to redo.\n";
	return;
    }
    std::cout << "Redone.\n";
    print_cursor_if_ok(session);
}

void cli_erase(EditorSession& session, const std::vector<std::string>& fields) {
    if (fields.size() == 1) {
	const EditStatus status = erase_char_before(session);
	if (status == EditStatus::NoLineAtCursor) {
	    std::cout << "Error: No line at the cursor to erase. Use goto or write first.\n";
	    return;
	}
	if (status == EditStatus::NothingBeforeCursor) {
	    std::cout << "Error: Nothing before the cursor to erase.\n";
	    return;
	}
	std::cout << "Deleted character before cursor.\n";
	return;
    }

    if (fields[1] == "char") {
	if (fields.size() == 2) {
	    const EditStatus status = erase_char_before(session);
	    if (status == EditStatus::NoLineAtCursor) {
		std::cout << "Error: No line at the cursor to erase. Use goto or write first.\n";
		return;
	    }
	    if (status == EditStatus::NothingBeforeCursor) {
		std::cout << "Error: Nothing before the cursor to erase.\n";
		return;
	    }
	    std::cout << "Deleted character before cursor.\n";
	    return;
	}

	if (fields.size() == 3) {
	    int n_chars = 0;
	    if (!parse_positive_int(fields[2], n_chars)) {
		std::cout << "Error: Invalid character count.\n";
		return;
	    }
	    const EditStatus status = erase_chars_before(session, n_chars);
	    if (status == EditStatus::NoLineAtCursor) {
		std::cout << "Error: No line at the cursor to erase. Use goto or write first.\n";
		return;
	    }
	    if (status == EditStatus::NothingBeforeCursor) {
		std::cout << "Error: Nothing before the cursor to erase.\n";
		return;
	    }
	    std::cout << "Deleted " << n_chars << " character(s) before cursor.\n";
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

	const EditStatus status = erase_words_before(session, n_words);
	if (status == EditStatus::NoLineAtCursor) {
	    std::cout << "Error: No line at the cursor to erase. Use goto or write first.\n";
	    return;
	}
	if (status == EditStatus::NothingBeforeCursor) {
	    std::cout << "Error: No word before the cursor to erase.\n";
	    return;
	}
	std::cout << "Deleted word(s) before cursor.\n";
	return;
    }

    std::cout << "Error: Second argument to erase must be 'char' or 'word'.\n";
}

void cli_delete_at_cursor(EditorSession& session) {
    const EditStatus status = delete_at_cursor(session);
    switch (status) {
    case EditStatus::NoLineAtCursor:
	std::cout << "Error: No line at the cursor.\n";
	break;
    case EditStatus::NothingAtCursor:
	std::cout << "Error: Nothing at the cursor to delete.\n";
	break;
    case EditStatus::Ok:
	std::cout << "Deleted character at cursor.\n";
	break;
    default:
	break;
    }
}

void cli_find(EditorSession& session, const std::string& needle) {
    if (needle.empty()) {
	std::cout << "Error: find needs non-empty text.\n";
	return;
    }
    if (session.note.text.empty()) {
	std::cout << "No text to search.\n";
	return;
    }

    if (find_text(session, needle)) {
	std::cout << "Match found.\n";
	print_cursor_if_ok(session);
	return;
    }

    std::cout << "Not found.\n";
}

void cli_find_next(EditorSession& session) {
    if (!session.find_active || session.find_needle.empty()) {
	std::cout << "Run find <text> first.\n";
	return;
    }

    if (find_next(session)) {
	std::cout << "Match found.\n";
	print_cursor_if_ok(session);
	return;
    }

    std::cout << "No more matches.\n";
}

void cli_yank(EditorSession& session) {
    const EditStatus status = yank_line(session);
    if (status == EditStatus::NoLineToYank) {
	std::cout << "Error: No line to yank.\n";
	return;
    }
    std::cout << "Line yanked.\n";
}

void cli_paste(EditorSession& session) {
    const EditStatus status = paste_line(session);
    if (status == EditStatus::ClipboardEmpty) {
	std::cout << "Error: Yank a line first (yank).\n";
	return;
    }
    std::cout << "Line pasted.\n";
    print_cursor_if_ok(session);
}

void cli_move_left(EditorSession& session) {
    const EditStatus status = move_left(session);
    if (status == EditStatus::AtStart) {
	std::cout << "Already at start of note.\n";
	return;
    }
    print_cursor_if_ok(session);
}

void cli_move_right(EditorSession& session) {
    const EditStatus status = move_right(session);
    if (status == EditStatus::AtEnd) {
	std::cout << "Already at end of note.\n";
	return;
    }
    print_cursor_if_ok(session);
}

void cli_move_home(EditorSession& session) {
    move_home(session);
    print_cursor_if_ok(session);
}

void cli_move_end(EditorSession& session) {
    move_end(session);
    print_cursor_if_ok(session);
}

void cli_goto(EditorSession& session, int line_1based, int col_1based) {
    const EditStatus status = goto_line(session, line_1based, col_1based);
    switch (status) {
    case EditStatus::LineNumberTooSmall:
	std::cout << "Error: Line numbers start at 1.\n";
	break;
    case EditStatus::ColumnNumberTooSmall:
	std::cout << "Error: Column numbers start at 1.\n";
	break;
    case EditStatus::LineOutOfRange: {
	std::ostringstream msg;
	const auto max_goto = session.note.text.size() + 1;
	msg << "Error: Line " << line_1based << " is out of range (1–" << max_goto << ").";
	std::cout << msg.str() << "\n";
	break;
    }
    case EditStatus::ColumnOutOfRange: {
	std::ostringstream msg;
	const std::size_t max_col = (session.current_line < session.note.text.size())
	    ? session.note.text[session.current_line].size()
	    : 0;
	msg << "Error: Column " << col_1based << " is out of range (1–" << (max_col + 1)
	    << ") on line " << line_1based << ".";
	std::cout << msg.str() << "\n";
	break;
    }
    case EditStatus::Ok:
	if (session.current_line >= session.note.text.size()) {
	    std::cout << "Cursor on line " << line_1based << " (new line), column 1.\n";
	} else {
	    print_cursor_if_ok(session);
	}
	break;
    default:
	break;
    }
}

void cli_newline(EditorSession& session) {
    insert_newline_at_cursor(session);
    print_cursor_if_ok(session);
}

void cli_delete_line(EditorSession& session) {
    const EditStatus status = delete_current_line(session);
    if (status == EditStatus::NoLineAtCursor) {
	std::cout << "Error: No line at the cursor to delete.\n";
	return;
    }
    print_cursor_if_ok(session);
}
