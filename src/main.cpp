#include "sticky_note.h"
#include "parser.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <map>
#include <unordered_map>
#include <string>

using std::string;
using std::getline;
using std::cout;
using std::cin;
using std::vector;

void print_usage() {
  // FIXME: Keep help text in sync with parser + dispatch (command names and arguments).
  std::cout << "This is the command list the user can use:\n"
	    << "write <text user wants to insert>		the input of the user will be added to the body of text\n"
	    << "erase <char/word> <n>				erase the last n characters or words of the most recent input line (default is word and 1)\n"
	    << "save						saves the current note\n"
	    << "delete						deletes the current note\n"
	    << "create						creates new note\n"
	    << "list						lists the notes made\n"
	    << "open <note_name>				opens a note\n"
            << "view <note_name>				prints out the contents of a note\n"
            << "quit						saves the current note and quits the program\n"
	    << "help						prints out this command menu\n";
}

void set_noteid(sticky_note& sn, string& count) {
    string note_path = "notes/note_" + count + ".txt";
    sn.note_path = note_path;

    std::ofstream out(note_path);
    out.close();

    sn.id = std::stoi(count);

    std::ofstream counter_out("notes/next_note_id.txt");
    counter_out << (sn.id + 1);
    counter_out.close();
}

sticky_note create_note(bool first_time) {
    sticky_note sn;


    if (first_time) {
	cout << "Welcome to Sticky_Note.V1 [Terminal Draft]! This is my starter sticky note project.\n"
	     << "Since it appears to be your first time using this program, I'll automatically create a note for you.\n";
    }

    else {
	cout << "A note will be created.\n";
    }


    cout  << "Please enter its title: ";
    string title;
    getline(cin, title);

    set_title(sn, title);

    std::ifstream in("notes/next_note_id.txt");
    string count;
    getline(in, count);

    set_noteid(sn, count);

    std::ofstream out(sn.note_path);
    out << "ID: " << sn.id << "\n";
    out << "Title: " << sn.title << "\n";
    out << "Created: " << sn.created << "\n";
    out << "Last Edited: " << sn.last_edited;
    // FIXME: On-disk format should match parse_file_info (same line layout / keys). Consider serializing timestamps with get_created / get_last_edit, not raw time_point <<.

    return sn;
}

void write_to_note(sticky_note& sn, const std::vector<std::string>& fields) {
    sn.text.push_back(fields[1]);
}

void erase_from_note(sticky_note& sn, const std::vector<std::string>& fields) {
    if (sn.text.empty()) {
	cout << "Error: Nothing to erase yet. Please enter another command.\n";
	// FIXME: Caller (main) may want to distinguish failure vs success; consider bool return instead of silent return.
	return;
    }

    if (fields.size() == 1) {
	while (sn.text.back().back() != ' ') {
	    sn.text.back().pop_back();
	}
	cout << "The most recent word has been deleted.\n";
    }

    else if (fields[1] == "char") {
	if (fields.size() == 2) {
	    sn.text.back().pop_back();
	    cout << "The most recent character has been deleted.\n";
	}

	else if (fields.size() == 3) {
	    // FIXME: Validate stoi result, bounds vs line length, handle exceptions from stoi.
	    const int n_chars = std::stoi(fields[2]);
	    sn.text.back().resize(sn.text.back().size() - static_cast<std::size_t>(n_chars));
	    cout << "The last " << fields[2] << "characters have been deleted.\n";
	}

	else {
	    cout << "Error: Too many arguments passed for the 'erase' command. Please try again.\n";
	    return;
	}
    }

    else if (fields[1] == "word") {
	if (fields.size() == 2) {
	    while (sn.text.back().back() != ' ') {
	        sn.text.back().pop_back();
	    }

	    cout << "The most recent word has been deleted.\n";
	}

	else if (fields.size() == 3) {
	    // FIXME: Validate stoi / bounds (empty line, word count).
	    const int n_words = std::stoi(fields[2]);
	    for (int i = 0; i < n_words; i++) {
		while (sn.text.back().back() != ' ') {
		    sn.text.back().pop_back();
		}
		sn.text.back().pop_back();
	    }

	    cout << "The last " << fields[2] << "characters have been deleted.\n";
	}

	else {
	    cout << "Error: Too many arguments passed for the 'erase' command. Please try again.\n";
	    return;
	}
    }
}

// TODO: CONTINUE THIS FUNCTION AFTER MAKING THE LIST ONE
void open_note() {
    // FIXME: Finish implementation: prompt, resolve note file, load into a sticky_note / update main's current note.
    cout << "Please enter the note title\n";
}

// TODO: CONTINUE THIS FUNCTION AFTER MAKING THE PARSER
void list_notes() {
    std::map<int, string> file_listing; // contains the id --> note title
    std::unordered_map<int, string> file_address; // contains the id --> filename
    // FIXME: Skip helper files (e.g. next_note_id.txt) so you only parse real note files.

    for (const auto& entry : std::filesystem::directory_iterator("notes")) {
	if (std::filesystem::is_regular_file(entry.path())) {
	    std::ifstream in(entry.path());
	    bool ok = true;
	    std::vector<std::string> file_info = parse_file_info(in, ok);

	    // FIXME: Guard `ok` and `file_info.size()` before indexing; use std::stoi and catch/validate id string.
	    string title = file_info[0];
	    int note_id = std::stoi(file_info[1]);

	    file_listing[note_id] = title;
	    file_address[note_id] = entry.path().string();
    	}
    }

    for (const auto& n : file_listing) {
	cout << n.first << " : " << n.second << "\n";
    }

}

int main() {
    std::ifstream in("notes/next_note_id.txt");

    if (!in) {
	std::cerr << "Error: Could not find file that contains the next viable id number.\n";
	return 1;
    }

    string note_num;
    getline(in, note_num);

    const bool first_time = (note_num == "0");
    string choice;
    sticky_note sn{};

    if (first_time) {
	sn = create_note(true);
    } else {
	cout << "Welcome back. Would you like to open a note or create a note. (open/create): \n";
	// FIXME: Mixing `cin >>` with `getline` for commands — use `std::getline` for choice or `std::cin >> std::ws` / `ignore` before the command loop.
	cin >> choice;
	if (choice == "open") {
	    list_notes();
	    open_note();
	    // FIXME: open_note() does not load parsed data into `sn` yet — current note state is still default until you implement load.
	} else if (choice == "create") {
	    sn = create_note(false);
	}
	// FIXME: Else branch for invalid choice; ensure `sn` is loaded for all non-first_time paths you care about.
    }

    print_usage();

    while(true) {
        string command;

        cout << "Please enter a command: ";
        getline(std::cin, command);

        if (command.empty()) {
	    cout << "Error: no command entered. Please try again.\n";
	    continue;
        }

        std::vector<std::string> fields = parse_command(command);

	if (fields[0] == "write") {write_to_note(sn, fields);}

	else if (fields[0] == "erase") {erase_from_note(sn, fields);}

	else if (fields[0] == "save") {
	    std::ofstream out(sn.note_path);

	    // FIXME: Save should match your agreed on-disk format (metadata + body), not only raw lines, if loaders expect headers.
	    for (const auto& t : sn.text) {
		out << t << "\n";
	    }

	    out.close();
	}

	else if (fields[0] == "delete") {
	    string choice;

	    cout << "This will permanently delete this note from memory.\n"
	         << "Are you sure? (y/n): ";
	    cin >> choice;

	    while ((choice != "y") && (choice != "n")) {
	        cout << "Error: Input is not a choice. Please input the command and try again.\n";

	        cout << "Are you sure you want to exit the delete this note? (y/n): \n";
	        cin >> choice;
	    }

	    if (choice == "y") {
		// FIXME: Decide policy: remove file only, clear `sn`, switch to another note, or exit program — current code exits after delete.
		std::filesystem::remove(sn.note_path);
		while (!sn.text.empty()) {
		    sn.text.pop_back();
		}

		return 0;
	    }

	    else if (choice == "n") {
		cout << "Returning to command menu\n";
		print_usage();
		continue;
	    }
	}

	else if (fields[0] == "create") {
	    // FIXME: Creating mid-session may need to save/close the previous note first; confirm ID/path/counter rules.
	    sn = create_note(false);
        }


	else if (fields[0] == "quit") {
	    string choice;

	    cout << "Are you sure you want to exit the program? (y/n): \n";
	    cin >> choice;

	    while ((choice != "y") && (choice != "n")) {
	        cout << "Error: Input is not a choice. Please input the command and try again.\n";

	        cout << "Are you sure you want to exit the program? (y/n): \n";
	        cin >> choice;
	    }

	    if (choice == "y") {
	        cout << "Saved current note. Thank you for using my lil project.\n";
	        return 0;
	    }

	    else if (choice == "n") {
	        cout << "Going back to command menu.\n";
	        print_usage();
	        continue;
	    }
	}

	else if (fields[0] == "help") {print_usage();}

	// FIXME: Optional: `else` branch for unknown commands so the user gets feedback.
    }
}
