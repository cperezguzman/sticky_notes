#include "sticky_note.h"
#include "parser.h"

#include <climits>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::cout;
using std::cin;

static void trim_in_place(string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
	s.clear();
	return;
    }
    auto end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
}

static string prompt_yes_no(const char* prompt) {
    for (;;) {
	cout << prompt;
	string line;
	if (!std::getline(cin, line)) {
	    return "n";
	}
	trim_in_place(line);
	if (line == "y" || line == "n") {
	    return line;
	}
	cout << "Please enter y or n.\n";
    }
}

static void save_note(const sticky_note& sn) {
    if (sn.note_path.empty()) {
	return;
    }
    std::ofstream out(sn.note_path);
    out << "Title:\n" << sn.title << "\n";
    out << "ID:\n" << sn.id << "\n";
    out << "Created:\n" << get_created(sn) << "\n";
    out << "Last Edited:\n" << get_last_edit(sn, "date_time") << "\n";
    out << "Body:\n";
    for (const auto& t : sn.text) {
	out << t << "\n";
    }
}

static bool parse_positive_int(const string& s, int& out) {
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

static void erase_suffix_until_space(string& line) {
    while (!line.empty() && line.back() != ' ') {
	line.pop_back();
    }
}

static void erase_delimiter_space(string& line) {
    if (!line.empty() && line.back() == ' ') {
	line.pop_back();
    }
}

static int count_words(const string& line) {
    std::istringstream iss(line);
    string w;
    int n = 0;
    while (iss >> w) {
	++n;
    }
    return n;
}

void erase_from_note(sticky_note& sn, const std::vector<std::string>& fields) {
    if (sn.text.empty()) {
	cout << "Error: Nothing to erase yet. Please enter another command.\n";
	return;
    }

    string& line = sn.text.back();

    if (fields.size() == 1) {
	erase_suffix_until_space(line);
	cout << "The most recent word has been deleted.\n";
	update_last_edit(sn);
	return;
    }

    if (fields[1] == "char") {
	if (fields.size() == 2) {
	    if (line.empty()) {
		cout << "Error: Current line is empty; nothing to erase.\n";
		return;
	    }
	    line.pop_back();
	    cout << "The most recent character has been deleted.\n";
	    update_last_edit(sn);
	    return;
	}

	if (fields.size() == 3) {
	    int n_chars = 0;
	    if (!parse_positive_int(fields[2], n_chars)) {
		cout << "Error: Invalid character count.\n";
		return;
	    }
	    const auto max_take = static_cast<int>(line.size());
	    if (n_chars > max_take) {
		n_chars = max_take;
	    }
	    line.resize(line.size() - static_cast<std::size_t>(n_chars));
	    cout << "The last " << n_chars << " character(s) have been deleted.\n";
	    update_last_edit(sn);
	    return;
	}

	cout << "Error: Too many arguments for erase char.\n";
	return;
    }

    if (fields[1] == "word") {
	if (fields.size() == 2) {
	    erase_suffix_until_space(line);
	    cout << "The most recent word has been deleted.\n";
	    update_last_edit(sn);
	    return;
	}

	if (fields.size() == 3) {
	    int n_words = 0;
	    if (!parse_positive_int(fields[2], n_words)) {
		cout << "Error: Invalid word count.\n";
		return;
	    }
	    int available = count_words(line);
	    if (available == 0) {
		cout << "Error: No words on the current line.\n";
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
	    cout << "The last " << n_words << " word(s) have been deleted.\n";
	    update_last_edit(sn);
	    return;
	}

	cout << "Error: Too many arguments for erase word.\n";
	return;
    }

    cout << "Error: Second argument to erase must be 'char' or 'word'.\n";
}

void write_to_note(sticky_note& sn, const std::vector<std::string>& fields) {
    sn.text.push_back(fields[1]);
    update_last_edit(sn);
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
    } else {
	cout << "A note will be created.\n";
    }

    cout << "Please enter its title: ";
    string title;
    std::getline(cin, title);

    set_title(sn, title);

    std::ifstream in("notes/next_note_id.txt");
    string count;
    std::getline(in, count);

    set_noteid(sn, count);

    std::ofstream out(sn.note_path);

    out << "Title:\n" << sn.title << "\n";
    out << "ID:\n" << sn.id << "\n";
    out << "Created:\n" << get_created(sn) << "\n";
    out << "Last Edited:\n" << get_last_edit(sn, "date_time") << "\n";
    out << "Body:\n";
    for (const auto& t : sn.text) {
	out << t << "\n";
    }

    return sn;
}

void print_usage() {
    std::cout << "This is the command list the user can use:\n"
	      << "write <text>				add a line to the body\n"
	      << "erase [char|word] [n]			erase last n chars/words (default: word, 1 if you type only erase)\n"
	      << "save					save the current note\n"
	      << "delete					delete the current note file\n"
	      << "create					create a new note (current note is saved first)\n"
	      << "list					list saved notes\n"
	      << "open <title>				open a note by title\n"
	      << "view <title>				print a note by title\n"
	      << "quit					save the current note and quit\n"
	      << "help					show this menu\n";
}

static bool load_note_from_path(sticky_note& sn, const string& path) {
    std::ifstream in(path);
    bool ok = true;
    std::vector<std::string> fi = parse_file_info(in, ok);
    if (!ok || fi[0].empty() || fi[1].empty()) {
	return false;
    }

    sn = sticky_note{};
    sn.title = fi[0];
    try {
	sn.id = std::stoi(fi[1]);
    } catch (const std::exception&) {
	return false;
    }
    sn.note_path = path;
    sn.text.clear();
    if (fi.size() > 4) {
	std::istringstream body(fi[4]);
	string bline;
	while (std::getline(body, bline)) {
	    sn.text.push_back(bline);
	}
    }
    const auto now = std::chrono::system_clock::now();
    sn.created = now;
    sn.last_edited = now;
    return true;
}

using NoteIndex = std::map<int, std::pair<string, string>>;

static NoteIndex build_note_index() {
    std::map<int, string> titles;
    std::unordered_map<int, string> paths;

    for (const auto& entry : std::filesystem::directory_iterator("notes")) {
	if (!std::filesystem::is_regular_file(entry.path())) {
	    continue;
	}
	if (entry.path().extension() != ".txt") {
	    continue;
	}
	if (!entry.path().stem().string().starts_with("note_")) {
	    continue;
	}

	std::ifstream in(entry.path());
	bool ok = true;
	std::vector<std::string> file_info = parse_file_info(in, ok);

	if (!ok) {
	    continue;
	}
	if (file_info.size() < 2 || file_info[1].empty()) {
	    continue;
	}

	int note_id = 0;
	try {
	    note_id = std::stoi(file_info[1]);
	} catch (const std::exception&) {
	    continue;
	}

	const string& title = file_info[0];
	titles[note_id] = title;
	paths[note_id] = entry.path().string();
    }

    NoteIndex out;
    for (const auto& e : titles) {
	out[e.first] = {e.second, paths[e.first]};
    }
    return out;
}

static void list_notes() {
    const auto idx = build_note_index();
    for (const auto& n : idx) {
	cout << n.first << " : " << n.second.first << "\n";
    }
}

static const string* find_path_by_title(const NoteIndex& idx, const string& title) {
    for (const auto& e : idx) {
	if (e.second.first == title) {
	    return &e.second.second;
	}
    }
    return nullptr;
}

static void print_note_by_title(const string& title) {
    const auto idx = build_note_index();
    const string* path = find_path_by_title(idx, title);
    if (!path) {
	cout << "No note with that title was found.\n";
	return;
    }
    std::ifstream in(*path);
    bool ok = true;
    auto fi = parse_file_info(in, ok);
    if (!ok) {
	cout << "Could not read that note file.\n";
	return;
    }
    cout << "--- " << title << " ---\n";
    if (fi.size() > 4) {
	cout << fi[4] << "\n";
    }
}

static bool open_note_by_title(sticky_note& sn, const string& title) {
    const auto idx = build_note_index();
    const string* path = find_path_by_title(idx, title);
    if (!path) {
	cout << "No note with that title was found.\n";
	return false;
    }
    if (!load_note_from_path(sn, *path)) {
	cout << "Failed to load that note.\n";
	return false;
    }
    cout << "Opened note \"" << sn.title << "\" (id " << sn.id << ").\n";
    return true;
}

int main() {
    std::ifstream in("notes/next_note_id.txt");

    if (!in) {
	std::cerr << "Error: Could not find file that contains the next viable id number.\n";
	return 1;
    }

    string note_num;
    std::getline(in, note_num);

    const bool first_time = (note_num == "0");
    sticky_note sn{};

    if (first_time) {
	sn = create_note(true);
    } else {
	for (;;) {
	    cout << "Welcome back. Would you like to open a note or create a note (open/create): \n";
	    string line;
	    if (!std::getline(cin, line)) {
		return 1;
	    }
	    trim_in_place(line);
	    if (line == "open") {
		list_notes();
		cout << "Enter the exact title of the note to open: ";
		string title;
		if (!std::getline(cin, title)) {
		    return 1;
		}
		trim_in_place(title);
		if (title.empty()) {
		    cout << "Empty title; starting with a blank in-memory note (use create/open later).\n";
		} else if (!open_note_by_title(sn, title)) {
		    cout << "Continuing with an empty in-memory note; use create or open.\n";
		}
		break;
	    }
	    if (line == "create") {
		sn = create_note(false);
		break;
	    }
	    cout << "Please type open or create.\n";
	}
    }

    print_usage();

    while (true) {
	string command;

	cout << "Please enter a command: ";
	if (!std::getline(cin, command)) {
	    break;
	}

	if (command.empty()) {
	    cout << "Error: no command entered. Please try again.\n";
	    continue;
	}

	std::vector<std::string> fields = parse_command(command);

	if (fields.empty()) {
	    continue;
	}

	if (fields[0] == "write") {
	    if (fields.size() < 2) {
		cout << "Error: write needs text on the same line.\n";
		continue;
	    }
	    write_to_note(sn, fields);
	}

	else if (fields[0] == "erase") {
	    erase_from_note(sn, fields);
	}

	else if (fields[0] == "save") {
	    save_note(sn);
	    cout << "Saved.\n";
	}

	else if (fields[0] == "list") {
	    list_notes();
	}

	else if (fields[0] == "view") {
	    if (fields.size() < 2) {
		cout << "Error: view needs a note title (view <title>).\n";
		continue;
	    }
	    print_note_by_title(fields[1]);
	}

	else if (fields[0] == "open") {
	    if (fields.size() < 2) {
		cout << "Error: open needs a note title (open <title>).\n";
		continue;
	    }
	    list_notes();
	    if (!open_note_by_title(sn, fields[1])) {
		cout << "Open failed.\n";
	    }
	}

	else if (fields[0] == "delete") {
	    const string choice = prompt_yes_no(
		"This will permanently delete this note file.\nAre you sure? (y/n): ");

	    if (choice == "y") {
		if (!sn.note_path.empty()) {
		    std::error_code ec;
		    std::filesystem::remove(sn.note_path, ec);
		    if (ec) {
			cout << "Could not delete file: " << ec.message() << "\n";
		    }
		}
		sn = sticky_note{};
		cout << "Note removed from disk; in-memory note is cleared.\n";
		continue;
	    }

	    cout << "Returning to command menu.\n";
	    print_usage();
	}

	else if (fields[0] == "create") {
	    if (!sn.note_path.empty()) {
		save_note(sn);
	    }
	    sn = create_note(false);
	}

	else if (fields[0] == "quit") {
	    const string choice =
		prompt_yes_no("Are you sure you want to exit the program? (y/n): ");

	    if (choice == "y") {
		save_note(sn);
		cout << "Saved current note. Thank you for using my lil project.\n";
		return 0;
	    }

	    cout << "Going back to command menu.\n";
	    print_usage();
	}

	else if (fields[0] == "help") {
	    print_usage();
	}

	else {
	    cout << "Unknown command: \"" << fields[0] << "\". Type help for a list.\n";
	}
    }

    return 0;
}
