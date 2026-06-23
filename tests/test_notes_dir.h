#pragma once

#include "note_store.h"
#include "sticky_note.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

namespace test_notes {

inline void write_note_file(int id, const std::string& title, const std::string& body) {
    const std::string path = "notes/note_" + std::to_string(id) + ".txt";
    std::ofstream out(path);
    out << "Title:\n" << title << "\n";
    out << "ID:\n" << id << "\n";
    out << "Created:\nCreated: June 20, 2026 at 12:00 PM\n";
    out << "Last Edited:\nLast Edited: June 20, 2026 at 12:00 PM\n";
    out << "Body:\n" << body;
    if (!body.empty() && body.back() != '\n') {
	out << '\n';
    }
}

class TempNotesDir {
public:
    explicit TempNotesDir(const std::string& prefix = "sticky_notes_test_") {
	root_ = std::filesystem::temp_directory_path()
	    / (prefix + std::to_string(static_cast<unsigned long>(counter_++)));
	std::filesystem::create_directories(root_ / "notes");
	previous_ = std::filesystem::current_path();
	std::filesystem::current_path(root_);
    }

    ~TempNotesDir() {
	std::error_code ec;
	std::filesystem::current_path(previous_, ec);
	std::filesystem::remove_all(root_, ec);
    }

    TempNotesDir(const TempNotesDir&) = delete;
    TempNotesDir& operator=(const TempNotesDir&) = delete;

    const std::filesystem::path& root() const { return root_; }

private:
    static inline unsigned counter_ = 0;
    std::filesystem::path root_;
    std::filesystem::path previous_;
};

inline sticky_note make_note_on_disk(int id, const std::string& title, const std::string& body_line) {
    write_note_file(id, title, body_line + "\n");
    sticky_note note{};
    load_note_from_path(note, "notes/note_" + std::to_string(id) + ".txt");
    return note;
}

} // namespace test_notes
