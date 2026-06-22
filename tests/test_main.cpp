#include "note_editor.h"
#include "note_file_codec.h"
#include "note_store.h"
#include "parser.h"
#include "sticky_note.h"
#include "textbox_input.h"

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

void test_parse_note_file_fixture() {
    std::ifstream in("tests/fixtures/note_sample.txt");
    ParsedNoteFile parsed{};
    check(parse_note_file(in, parsed), "fixture parses");
    check(parsed.title == "Sample Title", "fixture title");
    check(parsed.id == "42", "fixture id");
    check(parsed.body == "line one\nline two", "fixture body");
}

void test_parse_note_file_malformed() {
    const std::string path = "exports/test_malformed_note.txt";
    ensure_markdown_export_dir();
    {
	std::ofstream out(path);
	out << "Title:\nGood\nGarbage:\n";
    }
    std::ifstream bad(path);
    ParsedNoteFile parsed{};
    check(!parse_note_file(bad, parsed), "malformed file rejected");
    std::filesystem::remove(path);
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
    check(delete_current_line(session) == EditStatus::Ok, "delete line succeeds");
    check(session.note.text.size() == 1, "delete line removes one line");
}

void test_insert_at_cursor() {
    EditorSession session{};
    write_to_current_line(session, "hello");
    check(goto_line(session, 1, 3) == EditStatus::Ok, "goto for insert test");
    insert_at_cursor(session, "XX");
    check(session.note.text[0] == "heXXllo", "insert at column");
    check(format_line_with_cursor(session, 0) == "heXX|llo", "cursor marker position");
}

void test_erase_before_cursor() {
    EditorSession session{};
    write_to_current_line(session, "hello");
    check(goto_line(session, 1, 4) == EditStatus::Ok, "goto for erase test");
    check(erase_char_before(session) == EditStatus::Ok, "erase succeeds");
    check(session.note.text[0] == "helo", "erase deletes char before cursor");
    check(session.current_column == 2, "cursor moves back after erase");
}

void test_move_left_right() {
    EditorSession session{};
    write_to_current_line(session, "ab");
    check(goto_line(session, 1, 3) == EditStatus::Ok, "goto end of line");
    check(move_left(session) == EditStatus::Ok, "left within line");
    check(session.current_column == 1, "left within line");
    check(move_home(session) == EditStatus::Ok, "home");
    check(session.current_column == 0, "home");
    check(move_end(session) == EditStatus::Ok, "end");
    check(session.current_column == 2, "end");
}

void test_delete_at_cursor() {
    EditorSession session{};
    write_to_current_line(session, "abc");
    check(goto_line(session, 1, 2) == EditStatus::Ok, "goto for del test");
    check(delete_at_cursor(session) == EditStatus::Ok, "del succeeds");
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
    check(goto_line(session, 1, 1) == EditStatus::Ok, "goto for find test");
    check(find_text(session, "world"), "find locates needle");
    check(session.note.text[session.current_line].substr(session.current_column, 5) == "world",
	  "cursor at match");
}

void test_find_wrap_and_findnext() {
    EditorSession session{};
    write_to_current_line(session, "foo world world");
    check(move_end(session) == EditStatus::Ok, "move to end for find wrap");
    check(find_text(session, "world"), "find wraps from end of line");
    check(session.current_column == 4, "find wraps to first match");

    check(find_next(session), "findnext locates second match");
    check(session.current_column == 10, "findnext cursor at second match");
}

void test_yank_and_paste() {
    EditorSession session{};
    write_to_current_line(session, "copy me");
    check(yank_line(session) == EditStatus::Ok, "yank succeeds");
    check(paste_line(session) == EditStatus::Ok, "paste succeeds");
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

void test_textbox_init_and_type() {
    EditorSession session{};
    textbox_init_session(session);
    check(session.note.text.size() == 1, "textbox starts with one line");
    check(session.note.text[0].empty(), "textbox line empty");

    textbox_apply_key(session, {TextboxKeyKind::Character, U'h'});
    textbox_apply_key(session, {TextboxKeyKind::Character, U'i'});
    check(textbox_line_text(session) == "hi", "textbox inserts characters");
    check(textbox_cursor_column(session) == 2, "textbox cursor after insert");
}

void test_textbox_backspace_and_navigation() {
    EditorSession session{};
    textbox_init_session(session);
    textbox_apply_key(session, {TextboxKeyKind::Character, U'a'});
    textbox_apply_key(session, {TextboxKeyKind::Character, U'b'});
    textbox_apply_key(session, {TextboxKeyKind::Character, U'c'});
    textbox_apply_key(session, {TextboxKeyKind::Left, 0});
    textbox_apply_key(session, {TextboxKeyKind::Backspace, 0});
    check(textbox_line_text(session) == "ac", "textbox backspace at cursor");
    textbox_apply_key(session, {TextboxKeyKind::Home, 0});
    check(textbox_cursor_column(session) == 0, "textbox home");
    textbox_apply_key(session, {TextboxKeyKind::End, 0});
    check(textbox_cursor_column(session) == 2, "textbox end");
}

void test_textbox_multiline_newline_and_arrows() {
    EditorSession session{};
    textbox_init_session(session);
    textbox_apply_key(session, {TextboxKeyKind::Character, U'a'});
    textbox_apply_key(session, {TextboxKeyKind::Newline, 0});
    textbox_apply_key(session, {TextboxKeyKind::Character, U'b'});
    check(textbox_line_count(session) == 2, "newline creates second line");
    check(textbox_line_at(session, 0) == "a", "first line after split");
    check(textbox_line_at(session, 1) == "b", "second line after split");
    check(textbox_cursor_line(session) == 1, "cursor on second line");

    textbox_apply_key(session, {TextboxKeyKind::Up, 0});
    check(textbox_cursor_line(session) == 0, "up moves to first line");
    textbox_apply_key(session, {TextboxKeyKind::Down, 0});
    check(textbox_cursor_line(session) == 1, "down moves to second line");
}

void test_textbox_join_lines_on_backspace() {
    EditorSession session{};
    textbox_init_session(session);
    textbox_apply_key(session, {TextboxKeyKind::Character, U'h'});
    textbox_apply_key(session, {TextboxKeyKind::Character, U'i'});
    textbox_apply_key(session, {TextboxKeyKind::Newline, 0});
    textbox_apply_key(session, {TextboxKeyKind::Character, U'!'});
    check(textbox_line_count(session) == 2, "two lines before join");

    textbox_apply_key(session, {TextboxKeyKind::Home, 0});
    textbox_apply_key(session, {TextboxKeyKind::Backspace, 0});
    check(textbox_line_count(session) == 1, "backspace at line start merges");
    check(textbox_line_at(session, 0) == "hi!", "merged line text");
    check(textbox_cursor_column(session) == 2, "cursor at join point");
}

void test_move_up_down() {
    EditorSession session{};
    write_to_current_line(session, "ab");
    insert_newline_at_cursor(session);
    insert_at_cursor(session, "c");
    check(move_up(session) == EditStatus::Ok, "move up ok");
    check(session.current_line == 0, "on first line");
    check(move_down(session) == EditStatus::Ok, "move down ok");
    check(session.current_line == 1, "on second line");
}
} // namespace

int main() {
    test_parse_command_write_rest_of_line();
    test_parse_command_goto();
    test_parse_note_file_fixture();
    test_parse_note_file_malformed();
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
    test_textbox_init_and_type();
    test_textbox_backspace_and_navigation();
    test_textbox_multiline_newline_and_arrows();
    test_textbox_join_lines_on_backspace();
    test_move_up_down();

    if (failures == 0) {
	std::cout << "All tests passed.\n";
	return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
