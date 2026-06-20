#include "note_editor.h"
#include "note_store.h"
#include "parser.h"
#include "sticky_note.h"

#include <cassert>
#include <filesystem>
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
    check(session.note.text.size() == 2, "newline splits or inserts second line");
    check(session.current_line == 1, "cursor on second line");
    delete_current_line(session);
    check(session.note.text.size() == 1, "delete line removes one line");
}

void test_insert_at_cursor() {
    EditorSession session{};
    write_to_current_line(session, "hello");
    goto_line(session, 1, 3);
    insert_at_cursor(session, "XX");
    check(session.note.text[0] == "heXXllo", "insert at column");
    check(format_line_with_cursor(session, 0) == "heXX|llo", "cursor marker position");
}

void test_erase_before_cursor() {
    EditorSession session{};
    write_to_current_line(session, "hello");
    goto_line(session, 1, 4);
    erase_from_current_line(session, {"erase"});
    check(session.note.text[0] == "helo", "erase deletes char before cursor");
    check(session.current_column == 2, "cursor moves back after erase");
}

void test_move_left_right() {
    EditorSession session{};
    write_to_current_line(session, "ab");
    goto_line(session, 1, 3);
    move_left(session);
    check(session.current_column == 1, "left within line");
    move_home(session);
    check(session.current_column == 0, "home");
    move_end(session);
    check(session.current_column == 2, "end");
}

void test_delete_at_cursor() {
    EditorSession session{};
    write_to_current_line(session, "abc");
    goto_line(session, 1, 2);
    delete_at_cursor(session);
    check(session.note.text[0] == "ac", "del removes char at cursor");
}

void test_undo_write() {
    EditorSession session{};
    write_to_current_line(session, "first");
    write_to_current_line(session, "second");
    editor_undo(session);
    check(session.note.text[0] == "first", "undo restores previous text");
}

void test_find_text() {
    EditorSession session{};
    write_to_current_line(session, "hello world");
    goto_line(session, 1, 1);
    check(find_text(session, "world"), "find locates needle");
    check(session.note.text[session.current_line].substr(session.current_column, 5) == "world",
	  "cursor at match");
}

void test_find_wrap_and_findnext() {
    EditorSession session{};
    write_to_current_line(session, "foo world world");
    move_end(session);
    check(find_text(session, "world"), "find wraps from end of line");
    check(session.current_column == 4, "find wraps to first match");

    check(find_next(session), "findnext locates second match");
    check(session.current_column == 10, "findnext cursor at second match");
}

void test_yank_and_paste() {
    EditorSession session{};
    write_to_current_line(session, "copy me");
    yank_line(session);
    paste_line(session);
    check(session.note.text.size() == 2, "paste adds a line");
    check(session.note.text[1] == "copy me", "pasted line matches yanked line");
}

void test_format_note_as_markdown() {
    sticky_note sn;
    sn.id = 42;
    sn.title = "Sample \"Title\"";
    sn.note_path = "notes/note_42.txt";
    sn.text = {"line one", "line two"};

    const std::string md = format_note_as_markdown(sn);
    check(md.find("id: 42") != std::string::npos, "markdown frontmatter id");
    check(md.find("title: \"Sample \\\"Title\\\"\"") != std::string::npos, "markdown escapes title");
    check(md.find("source: \"notes/note_42.txt\"") != std::string::npos, "markdown source path");
    check(md.find("line one\nline two\n") != std::string::npos, "markdown body");
}

void test_export_note_to_markdown() {
    sticky_note sn;
    sn.id = 7;
    sn.title = "Export Me";
    sn.note_path = "notes/note_7.txt";
    sn.text = {"hello"};

    const std::string path = "exports/test_export_note_7.md";
    ensure_markdown_export_dir();
    check(export_note_to_markdown(sn, path), "export writes file");

    std::ifstream in(path);
    check(in.good(), "exported file readable");
    std::ostringstream contents;
    contents << in.rdbuf();
    check(contents.str().find("title: \"Export Me\"") != std::string::npos, "exported content");
    std::filesystem::remove(path);
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
    test_insert_at_cursor();
    test_erase_before_cursor();
    test_move_left_right();
    test_delete_at_cursor();
    test_undo_write();
    test_find_text();
    test_find_wrap_and_findnext();
    test_yank_and_paste();
    test_format_note_as_markdown();
    test_export_note_to_markdown();

    if (failures == 0) {
	std::cout << "All tests passed.\n";
	return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
