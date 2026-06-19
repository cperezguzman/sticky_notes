#include "note_editor.h"
#include "parser.h"
#include "sticky_note.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
int failures = 0;

void check(bool ok, const char* label) {
    if (!ok) {
	std::cerr << "FAIL: " << label << "\n";
	++failures;
    }
}

void test_parse_command_write_rest_of_line() {
    const auto fields = parse_command("write hello world");
    check(fields.size() == 2, "write splits into verb + rest");
    check(fields[0] == "write", "write verb");
    check(fields[1] == "hello world", "write rest of line");
}

void test_parse_command_goto() {
    const auto fields = parse_command("goto 3");
    check(fields.size() == 2, "goto has two fields");
    check(fields[0] == "goto", "goto verb");
    check(fields[1] == "3", "goto line number");
}

void test_parse_file_info_fixture() {
    std::ifstream in("tests/fixtures/note_sample.txt");
    bool ok = false;
    const auto fi = parse_file_info(in, ok);
    check(ok, "fixture parses");
    check(fi.size() >= 5, "fixture has five slots");
    check(fi[0] == "Sample Title", "fixture title");
    check(fi[1] == "42", "fixture id");
    check(fi[4] == "line one\nline two", "fixture body");
}

void test_parse_saved_timestamp_line() {
    std::chrono::system_clock::time_point tp{};
    const bool ok = parse_saved_timestamp_line(
	"Created: April 21, 2026 at 10:39 PM", tp);
    check(ok, "timestamp parses");

    sticky_note sn;
    sn.created = tp;
    const std::string formatted = get_created(sn);
    check(formatted.find("April") != std::string::npos, "round-trip month");
    check(formatted.find("2026") != std::string::npos, "round-trip year");
}

void test_parse_saved_timestamp_invalid() {
    std::chrono::system_clock::time_point tp{};
    check(!parse_saved_timestamp_line("not a date", tp), "invalid timestamp rejected");
}

void test_append_to_current_line() {
    EditorSession session{};
    write_to_current_line(session, "hello");
    append_to_current_line(session, " world");
    check(session.note.text.size() == 1, "append stays on one line");
    check(session.note.text[0] == "hello world", "append concatenates");
}

void test_newline_and_delete_line() {
    EditorSession session{};
    write_to_current_line(session, "a");
    insert_newline_at_cursor(session);
    check(session.note.text.size() == 2, "newline inserts blank line");
    check(session.note.text[1].empty(), "new line is empty");
    check(session.current_line == 1, "cursor on new line");
    delete_current_line(session);
    check(session.note.text.size() == 1, "delete line removes one line");
}
} // namespace

int main() {
    test_parse_command_write_rest_of_line();
    test_parse_command_goto();
    test_parse_file_info_fixture();
    test_parse_saved_timestamp_line();
    test_parse_saved_timestamp_invalid();
    test_append_to_current_line();
    test_newline_and_delete_line();

    if (failures == 0) {
	std::cout << "All tests passed.\n";
	return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
