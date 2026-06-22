#include "note_editor.h"
#include "note_editor_cli.h"
#include "note_store.h"
#include "parser.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
void trim_in_place(std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
	s.clear();
	return;
    }
    auto end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
}

std::string prompt_yes_no(const char* prompt) {
    for (;;) {
	std::cout << prompt;
	std::string line;
	if (!std::getline(std::cin, line)) {
	    return "n";
	}
	trim_in_place(line);
	if (line == "y" || line == "n") {
	    return line;
	}
	std::cout << "Please enter y or n.\n";
    }
}

bool is_all_digits(const std::string& s) {
    if (s.empty()) {
	return false;
    }
    for (unsigned char c : s) {
	if (!std::isdigit(c)) {
	    return false;
	}
    }
    return true;
}

bool open_note_by_key(EditorSession& session, const std::string& key) {
    if (is_all_digits(key)) {
	try {
	    const int id = std::stoi(key);
	    if (open_note_by_id(session.note, id)) {
		editor_reset_cursor(session);
		editor_clear_history(session);
		return true;
	    }
	    return false;
	} catch (const std::exception&) {
	    return false;
	}
    }
    if (open_note_by_title(session.note, key)) {
	editor_reset_cursor(session);
	editor_clear_history(session);
	return true;
    }
    return false;
}

void view_note_by_key(const std::string& key) {
    if (is_all_digits(key)) {
	try {
	    print_note_by_id(std::stoi(key));
	} catch (const std::exception&) {
	    std::cout << "Invalid note id.\n";
	}
	return;
    }
    print_note_by_title(key);
}

void print_usage() {
    std::cout << "This is the command list the user can use:\n"
	      << "write <text>				replace text on the current line\n"
	      << "append <text>				add text to the end of the current line\n"
	      << "insert <text>				insert text at the cursor\n"
	      << "erase [char|word] [n]			delete before cursor (default: one char)\n"
	      << "del					delete character at cursor (forward delete)\n"
	      << "undo / redo				undo or redo the last body edit\n"
	      << "find <text> / findnext		search forward from cursor\n"
	      << "yank / paste				copy current line / insert after cursor\n"
	      << "left / right / home / end		move cursor within the note\n"
	      << "pos					show current line and column\n"
	      << "goto <line> [col]			jump to line (and optional column, 1-based)\n"
	      << "newline					split line at cursor, or insert blank line\n"
	      << "delete line				remove the line at the cursor\n"
	      << "show					print body; | marks the cursor\n"
	      << "save					save the current note\n"
	      << "rename <title>			change the current note title\n"
	      << "delete					delete the current note file\n"
	      << "create					create a new note (current note is saved first)\n"
	      << "list					list saved notes (id : title)\n"
	      << "open <title|id>			open a note by exact title or numeric id\n"
	      << "view <title|id>			print a note by title or id\n"
	      << "export markdown [path]		write current note as Markdown (default: exports/)\n"
	      << "quit					save the current note and quit\n"
	      << "help					show this menu\n";
}
} // namespace

int main() {
    ensure_notes_data_dir();
    ensure_markdown_export_dir();

    std::ifstream in("notes/next_note_id.txt");

    if (!in) {
	std::cerr << "Error: Could not read notes/next_note_id.txt.\n";
	return 1;
    }

    std::string note_num;
    std::getline(in, note_num);

    const bool first_time = (note_num == "0");
    EditorSession session{};

    if (first_time) {
	session.note = create_note(true);
	editor_reset_cursor(session);
	editor_clear_history(session);
    } else {
	for (;;) {
	    std::cout << "Welcome back. Would you like to open a note or create a note (open/create): \n";
	    std::string line;
	    if (!std::getline(std::cin, line)) {
		return 1;
	    }
	    trim_in_place(line);
	    if (line == "open") {
		list_notes();
		std::cout << "Enter the note title or id to open: ";
		std::string key;
		if (!std::getline(std::cin, key)) {
		    return 1;
		}
		trim_in_place(key);
		if (key.empty()) {
		    std::cout << "Empty input; starting with a blank in-memory note (use create/open later).\n";
		} else if (!open_note_by_key(session, key)) {
		    std::cout << "Continuing with an empty in-memory note; use create or open.\n";
		}
		break;
	    }
	    if (line == "create") {
		session.note = create_note(false);
		editor_reset_cursor(session);
		editor_clear_history(session);
		break;
	    }
	    std::cout << "Please type open or create.\n";
	}
    }

    print_usage();

    while (true) {
	std::string command;

	std::cout << "Please enter a command: ";
	if (!std::getline(std::cin, command)) {
	    break;
	}

	if (command.empty()) {
	    std::cout << "Error: no command entered. Please try again.\n";
	    continue;
	}

	const std::vector<std::string> fields = parse_command(command);

	if (fields.empty()) {
	    continue;
	}

	if (fields[0] == "write") {
	    if (fields.size() < 2) {
		std::cout << "Error: write needs text on the same line.\n";
		continue;
	    }
	    write_to_current_line(session, fields[1]);
	}

	else if (fields[0] == "append") {
	    if (fields.size() < 2) {
		std::cout << "Error: append needs text on the same line.\n";
		continue;
	    }
	    append_to_current_line(session, fields[1]);
	}

	else if (fields[0] == "insert") {
	    if (fields.size() < 2) {
		std::cout << "Error: insert needs text on the same line.\n";
		continue;
	    }
	    insert_at_cursor(session, fields[1]);
	}

	else if (fields[0] == "erase") {
	    cli_erase(session, fields);
	}

	else if (fields[0] == "del") {
	    cli_delete_at_cursor(session);
	}

	else if (fields[0] == "undo") {
	    cli_undo(session);
	}

	else if (fields[0] == "redo") {
	    cli_redo(session);
	}

	else if (fields[0] == "find") {
	    if (fields.size() < 2) {
		std::cout << "Error: find needs text (find <text>).\n";
		continue;
	    }
	    cli_find(session, fields[1]);
	}

	else if (fields[0] == "findnext") {
	    cli_find_next(session);
	}

	else if (fields[0] == "yank") {
	    cli_yank(session);
	}

	else if (fields[0] == "paste") {
	    cli_paste(session);
	}

	else if (fields[0] == "pos") {
	    cli_show_cursor(session);
	}

	else if (fields[0] == "left") {
	    cli_move_left(session);
	}

	else if (fields[0] == "right") {
	    cli_move_right(session);
	}

	else if (fields[0] == "home") {
	    cli_move_home(session);
	}

	else if (fields[0] == "end") {
	    cli_move_end(session);
	}

	else if (fields[0] == "goto") {
	    if (fields.size() < 2) {
		std::cout << "Error: goto needs a line number (goto <line> [col]).\n";
		continue;
	    }
	    int line = 0;
	    int col = 1;
	    try {
		line = std::stoi(fields[1]);
		if (fields.size() >= 3) {
		    col = std::stoi(fields[2]);
		}
	    } catch (const std::exception&) {
		std::cout << "Error: Invalid line or column number.\n";
		continue;
	    }
	    cli_goto(session, line, col);
	}

	else if (fields[0] == "show") {
	    cli_show_note(session);
	}

	else if (fields[0] == "newline") {
	    cli_newline(session);
	}

	else if (fields[0] == "save") {
	    save_note(session.note);
	    std::cout << "Saved.\n";
	}

	else if (fields[0] == "rename") {
	    if (fields.size() < 2) {
		std::cout << "Error: rename needs a title (rename <title>).\n";
		continue;
	    }
	    rename_current_note(session.note, fields[1]);
	}

	else if (fields[0] == "list") {
	    list_notes();
	}

	else if (fields[0] == "view") {
	    if (fields.size() < 2) {
		std::cout << "Error: view needs a note title or id (view <title|id>).\n";
		continue;
	    }
	    view_note_by_key(fields[1]);
	}

	else if (fields[0] == "open") {
	    if (fields.size() < 2) {
		std::cout << "Error: open needs a note title or id (open <title|id>).\n";
		continue;
	    }
	    list_notes();
	    if (!open_note_by_key(session, fields[1])) {
		std::cout << "Open failed.\n";
	    }
	}

	else if (fields[0] == "export") {
	    if (fields.size() < 2 || fields[1] != "markdown") {
		std::cout << "Error: use export markdown [path].\n";
		continue;
	    }
	    if (session.note.note_path.empty()) {
		std::cout << "Error: no note loaded to export.\n";
		continue;
	    }

	    std::string path;
	    const std::string prefix = "export markdown";
	    if (command.size() > prefix.size()) {
		path = command.substr(prefix.size());
		trim_in_place(path);
	    }
	    if (path.empty()) {
		path = default_markdown_export_path(session.note);
	    }

	    if (export_note_to_markdown(session.note, path)) {
		std::cout << "Exported to " << path << ".\n";
	    } else {
		std::cout << "Export failed.\n";
	    }
	}

	else if (fields[0] == "delete") {
	    if (fields.size() >= 2 && fields[1] == "line") {
		cli_delete_line(session);
		continue;
	    }

	    const std::string choice = prompt_yes_no(
		"This will permanently delete this note file.\nAre you sure? (y/n): ");

	    if (choice == "y") {
		if (!session.note.note_path.empty()) {
		    std::error_code ec;
		    std::filesystem::remove(session.note.note_path, ec);
		    if (ec) {
			std::cout << "Could not delete file: " << ec.message() << "\n";
		    }
		}
		session = EditorSession{};
		editor_clear_history(session);
		std::cout << "Note removed from disk; in-memory note is cleared.\n";
		continue;
	    }

	    std::cout << "Returning to command menu.\n";
	    print_usage();
	}

	else if (fields[0] == "create") {
	    if (!session.note.note_path.empty()) {
		save_note(session.note);
	    }
	    session.note = create_note(false);
	    editor_reset_cursor(session);
	    editor_clear_history(session);
	}

	else if (fields[0] == "quit") {
	    const std::string choice =
		prompt_yes_no("Are you sure you want to exit the program? (y/n): ");

	    if (choice == "y") {
		save_note(session.note);
		std::cout << "Saved current note. Thank you for using my lil project.\n";
		return 0;
	    }

	    std::cout << "Going back to command menu.\n";
	    print_usage();
	}

	else if (fields[0] == "help") {
	    print_usage();
	}

	else {
	    std::cout << "Unknown command: \"" << fields[0] << "\". Type help for a list.\n";
	}
    }

    return 0;
}
