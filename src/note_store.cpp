#include "note_store.h"

#include "note_file_codec.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace {
void set_noteid(sticky_note& sn, std::string& count) {
    const std::string note_path = "notes/note_" + count + ".txt";
    sn.note_path = note_path;

    std::ofstream out(note_path);
    out.close();

    sn.id = std::stoi(count);

    std::ofstream counter_out("notes/next_note_id.txt");
    counter_out << (sn.id + 1);
    counter_out.close();
}

void write_new_note_file(const sticky_note& sn) {
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

bool apply_parsed_note(sticky_note& sn, const ParsedNoteFile& parsed, const std::string& path) {
    if (parsed.title.empty() || parsed.id.empty()) {
	return false;
    }

    sn = sticky_note{};
    sn.title = parsed.title;
    try {
	sn.id = std::stoi(parsed.id);
    } catch (const std::exception&) {
	return false;
    }
    sn.note_path = path;
    sn.text = body_lines_from_parsed(parsed);

    const auto now = std::chrono::system_clock::now();
    sn.created = now;
    sn.last_edited = now;
    if (!parsed.created_line.empty()) {
	std::chrono::system_clock::time_point t;
	if (parse_saved_timestamp_line(parsed.created_line, t)) {
	    sn.created = t;
	}
    }
    if (!parsed.last_edited_line.empty()) {
	std::chrono::system_clock::time_point t;
	if (parse_saved_timestamp_line(parsed.last_edited_line, t)) {
	    sn.last_edited = t;
	}
    }
    if (sn.last_edited < sn.created) {
	sn.last_edited = sn.created;
    }
    return true;
}
} // namespace

NoteIndex build_note_index() {
    std::map<int, std::string> titles;
    std::unordered_map<int, std::string> paths;

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
	ParsedNoteFile parsed{};
	if (!parse_note_file(in, parsed) || parsed.id.empty()) {
	    continue;
	}

	int note_id = 0;
	try {
	    note_id = std::stoi(parsed.id);
	} catch (const std::exception&) {
	    continue;
	}

	titles[note_id] = parsed.title;
	paths[note_id] = entry.path().string();
    }

    NoteIndex out;
    for (const auto& e : titles) {
	out[e.first] = {e.second, paths[e.first]};
    }
    return out;
}

void list_notes() {
    const auto idx = build_note_index();
    for (const auto& n : idx) {
	std::cout << n.first << " : " << n.second.first << "\n";
    }
}

const std::string* find_path_by_title(const NoteIndex& idx, const std::string& title) {
    for (const auto& e : idx) {
	if (e.second.first == title) {
	    return &e.second.second;
	}
    }
    return nullptr;
}

const std::string* find_path_by_id(const NoteIndex& idx, int id) {
    const auto it = idx.find(id);
    if (it == idx.end()) {
	return nullptr;
    }
    return &it->second.second;
}

bool load_note_from_path(sticky_note& sn, const std::string& path) {
    std::ifstream in(path);
    ParsedNoteFile parsed{};
    if (!parse_note_file(in, parsed)) {
	return false;
    }
    return apply_parsed_note(sn, parsed, path);
}

void save_note(const sticky_note& sn) {
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

bool open_note_by_title(sticky_note& sn, const std::string& title) {
    const auto idx = build_note_index();
    const std::string* path = find_path_by_title(idx, title);
    if (!path) {
	std::cout << "No note with that title was found.\n";
	return false;
    }
    if (!load_note_from_path(sn, *path)) {
	std::cout << "Failed to load that note.\n";
	return false;
    }
    std::cout << "Opened note \"" << sn.title << "\" (id " << sn.id << ").\n";
    return true;
}

bool open_note_by_id(sticky_note& sn, int id) {
    const auto idx = build_note_index();
    const std::string* path = find_path_by_id(idx, id);
    if (!path) {
	std::cout << "No note with id " << id << " was found.\n";
	return false;
    }
    if (!load_note_from_path(sn, *path)) {
	std::cout << "Failed to load that note.\n";
	return false;
    }
    std::cout << "Opened note \"" << sn.title << "\" (id " << sn.id << ").\n";
    return true;
}

static void print_note_body(const std::string& label, const std::string& path) {
    std::ifstream in(path);
    ParsedNoteFile parsed{};
    if (!parse_note_file(in, parsed)) {
	std::cout << "Could not read that note file.\n";
	return;
    }
    std::cout << "--- " << label << " ---\n";
    std::cout << parsed.body << "\n";
}

void print_note_by_title(const std::string& title) {
    const auto idx = build_note_index();
    const std::string* path = find_path_by_title(idx, title);
    if (!path) {
	std::cout << "No note with that title was found.\n";
	return;
    }
    print_note_body(title, *path);
}

void print_note_by_id(int id) {
    const auto idx = build_note_index();
    const auto it = idx.find(id);
    if (it == idx.end()) {
	std::cout << "No note with id " << id << " was found.\n";
	return;
    }
    print_note_body(it->second.first, it->second.second);
}

sticky_note create_note(bool first_time) {
    if (first_time) {
	std::cout << "Welcome to Sticky_Note.V1 [Terminal Draft]! This is my starter sticky note project.\n"
		  << "Since it appears to be your first time using this program, I'll automatically create a note for you.\n";
    } else {
	std::cout << "A note will be created.\n";
    }

    std::cout << "Please enter its title: ";
    std::string title;
    std::getline(std::cin, title);

    return create_note_silent(title);
}

sticky_note create_note_silent(const std::string& title) {
    sticky_note sn;
    set_title(sn, title.empty() ? "Untitled" : title);

    std::ifstream in("notes/next_note_id.txt");
    std::string count;
    std::getline(in, count);

    set_noteid(sn, count);
    write_new_note_file(sn);
    return sn;
}

void ensure_notes_data_dir() {
    std::error_code ec;
    std::filesystem::create_directories("notes", ec);
    if (!std::filesystem::exists("notes/next_note_id.txt")) {
	std::ofstream out("notes/next_note_id.txt");
	out << "0";
    }
}

namespace {
std::string yaml_double_quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
	if (c == '\\' || c == '"') {
	    out += '\\';
	}
	out += c;
    }
    out += '"';
    return out;
}

std::string timestamp_value_for_yaml(const std::string& line) {
    const std::string created_prefix = "Created: ";
    const std::string edited_prefix = "Last Edited: ";
    if (line.starts_with(created_prefix)) {
	return line.substr(created_prefix.size());
    }
    if (line.starts_with(edited_prefix)) {
	return line.substr(edited_prefix.size());
    }
    return line;
}
} // namespace

std::string format_note_as_markdown(const sticky_note& sn) {
    std::ostringstream out;
    out << "---\n";
    out << "id: " << sn.id << "\n";
    out << "title: " << yaml_double_quote(sn.title) << "\n";
    out << "created: " << yaml_double_quote(timestamp_value_for_yaml(get_created(sn))) << "\n";
    out << "last_edited: "
	<< yaml_double_quote(timestamp_value_for_yaml(get_last_edit(sn, "date_time"))) << "\n";
    if (!sn.note_path.empty()) {
	out << "source: " << yaml_double_quote(sn.note_path) << "\n";
    }
    out << "---\n\n";
    for (std::size_t i = 0; i < sn.text.size(); ++i) {
	if (i > 0) {
	    out << '\n';
	}
	out << sn.text[i];
    }
    if (!sn.text.empty()) {
	out << '\n';
    }
    return out.str();
}

std::string default_markdown_export_path(const sticky_note& sn) {
    return "exports/note_" + std::to_string(sn.id) + ".md";
}

void ensure_markdown_export_dir() {
    std::error_code ec;
    std::filesystem::create_directories("exports", ec);
}

bool export_note_to_markdown(const sticky_note& sn, const std::string& path) {
    if (sn.note_path.empty()) {
	return false;
    }

    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
	std::error_code ec;
	std::filesystem::create_directories(parent, ec);
	if (ec) {
	    return false;
	}
    }

    std::ofstream out(path);
    if (!out) {
	return false;
    }
    out << format_note_as_markdown(sn);
    return static_cast<bool>(out);
}

bool rename_current_note(sticky_note& sn, const std::string& new_title) {
    if (sn.note_path.empty()) {
	std::cout << "Error: No note loaded to rename.\n";
	return false;
    }
    if (new_title.empty()) {
	std::cout << "Error: Title cannot be empty.\n";
	return false;
    }

    set_title(sn, new_title);
    save_note(sn);
    std::cout << "Renamed note to \"" << sn.title << "\".\n";
    return true;
}
