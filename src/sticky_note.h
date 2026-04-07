#include <string>
#include <vector>
#include <chrono>

struct sticky_note {
    std::vector<std::string> text;
    int id;
    std::chrono::time_point<std::chrono::system_clock> created = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> last_edited = created;
    std::string title;

};

// std::string get_last_edit_date(const sticky_note& sn);

// std::string get_last_edit_time(const sticky_note& sn);

// std::string get_created_date(const sticky_note& sn);

// std::string get_created_time(const sticky_note& sn);

std::string get_last_edit(const sticky_note& sn, const std::string& choice);

void update_last_edit(sticky_note& sn);

void change_title(sticky_note& sn);

void update_text(sticky_note& sn);
