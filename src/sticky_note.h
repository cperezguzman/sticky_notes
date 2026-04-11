#ifndef STICKY_NOTE_H
#define STICKY_NOTE_H

#include <string>
#include <vector>
#include <chrono>

struct sticky_note {
    std::vector<std::string> text;
    int id;
    std::chrono::time_point<std::chrono::system_clock> created = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> last_edited = created;
    std::string title;
    std::string note_path;

};

std::string get_last_edit(const sticky_note& sn, const std::string& choice);

std::string get_created(const sticky_note& sn);

void update_last_edit(sticky_note& sn);

void set_title(sticky_note& sn, const std::string& title);

void update_text(sticky_note& sn);

#endif
