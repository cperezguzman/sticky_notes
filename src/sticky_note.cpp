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

    auto duration = now - sn.last_edit;

    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);

    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);

    auto years = std::chrono::duration_cast<std::chrono::years>(duration);

    auto months = std::chrono::duration_cast<std::chrono::months>(duration);

    auto days = std::chrono::duration_cast<std::chrono::days>(duration);

    if (choice == "day_only" && (hours > 23)) {
	return std::format("Days Elapsed: {} days", days.count());
    }

    else if (choice == "month_only") {
	
    }

    else if (choice == "year_only") {}

    else if (choice == "hour_only") {}

    else if (choice == "minutes_only") {}

    else if (choice == "seconds_only") {}

    else if (choice == "date") {}

    else if (choice == "time") {}
