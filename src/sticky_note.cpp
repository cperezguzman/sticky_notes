#include "sticy_note.h"

#include <format>

//std::string get_last_edit_date(const sticky_note& sn) {
//    auto timestamp = sn.last_edited;

//    std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(timestamp)};

//    std::string date = std::format("{:%B %d, %Y}", ymd);

//    return date;
//}

//std::string get_last_edit_time(const sticky_note& sn) {
//    auto now = std::chrono::system_clock::now();

//    std::chrono::hh_mm_ss hms{now - 


std::string get_last_edit(const sticky_note& sn, const std::string& choice) {
    auto now = std::chrono::system_clock::now();

    auto duration = now - sn.last_edited;

    std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(sn.last_edited)};

    std::chrono::hh_mm_ss hms{std::chrono::floor<std::chrono::seconds>(sn.last_edited)};

    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);

    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);

    auto years = std::chrono::duration_cast<std::chrono::years>(duration);

    auto months = std::chrono::duration_cast<std::chrono::months>(duration);

    auto days = std::chrono::duration_cast<std::chrono::days>(duration);

    if (choice == "day_only" && (hours > 23)) {
	return std::format("Last Edited: {} days ago", days.count());
    }

    else if (choice == "month_only" && (days > 30)) {
	return std::format("Last Edited: {} months ago", months.count());
    }

    else if (choice == "year_only" && (months > 11)) {
	return std::format("Last Edited: {} years ago", years.count());
    }

    else if (choice == "hour_only" && (minutes > 59)) {
	return std::format("Last Edited: {} hoursa ago", hours.count());
    }

    else if (choice == "minutes_only" && (seconds > 59)) {
	return std::format("Last Edited: {} minutes ago", minutes.count()):
    }

    else if (choice == "seconds_only") {
	return std::format("Last Edited: {} seconds ago", seconds.count());
    }

    else if (choice == "date_time") {
	return std::format("Last Edited: {:%B %d, %Y at %I:%M %p}", sn.last_edited);
    }
}


std::string get_created(const sticky_note& sn) {
    return std::format("Created: {:%B %d, %Y at %I:%M %p}", sn.created);
}

void update_last_edit(sticky_note& sn) {
    sn.last_edited = std::chrono::system_clock::now();
}

void set_title(sticky_note& sn, const std::string& title) {
    sn.title = title;
}

void update_text(sticky_note& sn, const std::string& new_text) {
    sn.text.push_back(new_text);
}
