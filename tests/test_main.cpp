#include "note_editor.h"
#include "note_file_codec.h"
#include "note_store.h"
#include "parser.h"
#include "gui_theme.h"
#include "sticky_gui.h"
#include "sticky_note.h"
#include "textbox_input.h"
#include "textbox_sdl.h"

#include "sdl_event_helpers.h"
#include "test_notes_dir.h"

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

void test_textbox_wrap_long_line() {
    EditorSession session{};
    textbox_init_session(session);
    for (char c = 'a'; c <= 'j'; ++c) {
	textbox_apply_key(session, {TextboxKeyKind::Character, static_cast<char32_t>(c)}, 4);
    }
    check(session.note.text.size() == 3, "wrap splits overflow into new lines");
    check(session.note.text[0] == "abcd", "first wrapped segment");
    check(session.note.text[1] == "efgh", "second wrapped segment");
    check(session.note.text[2] == "ij", "final wrapped segment");
    check(textbox_body_max_columns(280.0f) >= 10, "panel width yields usable column count");
}

void test_textbox_reflow_on_widen() {
    EditorSession session{};
    textbox_init_session(session);
    for (char c = 'a'; c <= 'h'; ++c) {
	textbox_apply_key(session, {TextboxKeyKind::Character, static_cast<char32_t>(c)}, 4);
    }
    check(session.note.text.size() == 2, "narrow wrap creates two soft lines");

    textbox_enforce_wrap(session, 20);
    check(session.note.text.size() == 1, "widening merges soft-wrapped lines");
    check(session.note.text[0] == "abcdefgh", "merged paragraph text preserved");

    textbox_apply_key(session, {TextboxKeyKind::Newline, 0}, 20);
    textbox_apply_key(session, {TextboxKeyKind::Character, U'z'}, 20);
    textbox_enforce_wrap(session, 20);
    check(session.note.text.size() == 2, "hard newline stays after reflow");
    check(session.note.text[0] == "abcdefgh", "first paragraph unchanged");
    check(session.note.text[1] == "z", "second paragraph preserved");
}

void test_textbox_word_boundary_wrap() {
    EditorSession session{};
    textbox_init_session(session);
    const std::string phrase = "hello world again";
    for (char c : phrase) {
	textbox_apply_key(session, {TextboxKeyKind::Character, static_cast<char32_t>(c)}, 10);
    }
    check(session.note.text.size() >= 2, "phrase wraps onto multiple lines");
    check(session.note.text[0] == "hello ", "wrap prefers break after space");
    check(session.note.text[0].find(' ') != std::string::npos, "first soft line keeps trailing space");
    for (const std::string& line : session.note.text) {
	check(line.size() <= 10, "each soft line fits max columns");
    }
}

void test_textbox_storage_lines_collapse_soft_wraps() {
    EditorSession session{};
    textbox_init_session(session);
    for (char c : std::string("abcdefghi")) {
	textbox_apply_key(session, {TextboxKeyKind::Character, static_cast<char32_t>(c)}, 3);
    }
    check(session.note.text.size() == 3, "soft wrap created multiple display lines");
    const std::vector<std::string> stored = textbox_storage_lines(session);
    check(stored.size() == 1, "storage collapses soft wraps to one paragraph");
    check(stored[0] == "abcdefghi", "storage paragraph preserves characters");

    textbox_apply_key(session, {TextboxKeyKind::Newline, 0}, 3);
    textbox_apply_key(session, {TextboxKeyKind::Character, U'z'}, 3);
    const std::vector<std::string> stored2 = textbox_storage_lines(session);
    check(stored2.size() == 2, "hard Enter becomes a storage newline");
    check(stored2[0] == "abcdefghi", "paragraph before Enter preserved");
    check(stored2[1] == "z", "paragraph after Enter preserved");
}

void test_textbox_repair_persisted_soft_wraps() {
    std::vector<std::string> lines = {
	"Kind of li",
	"ke linking notes",
	"",
	"Ex:",
	"You made a sticky note to take down details of a future trip, so when you go back t",
	"o the website that you were planning around, the sticky note pops up again.",
    };
    textbox_repair_persisted_soft_wraps(lines);
    check(lines.size() == 4, "repair joins soft wraps but keeps blank/title breaks");
    check(lines[0] == "Kind of like linking notes", "mid-word soft wrap rejoined");
    check(lines[1].empty(), "blank line preserved as paragraph break");
    check(lines[2] == "Ex:", "short title line not merged into following sentence");
    check(lines[3].find("back to the website") != std::string::npos, "long wrapped sentence rejoined");

    std::vector<std::string> spaced = {"smooth animation pop up.", " maybe even be able"};
    textbox_repair_persisted_soft_wraps(spaced);
    check(spaced.size() == 1, "leading-space continuation joins prior line");
    check(spaced[0] == "smooth animation pop up. maybe even be able", "leading space kept as normal word space");
}

void test_textbox_viewport_resets_when_content_fits() {
    EditorSession session{};
    textbox_init_session(session);
    for (int i = 0; i < 40; ++i) {
	textbox_apply_key(session, {TextboxKeyKind::Character, U'a'}, 8);
	textbox_apply_key(session, {TextboxKeyKind::Newline, 0}, 8);
    }
    session.current_line = session.note.text.size() - 1;
    session.current_column = session.note.text.back().size();

    TextboxViewport viewport{};
    textbox_scroll_to_cursor(viewport, session, 5);
    check(viewport.first_visible_line > 0, "narrow view scrolls down to keep cursor visible");

    // Simulate widen/taller panel: more rows visible than total lines after reflow.
    textbox_enforce_wrap(session, 80);
    textbox_clamp_viewport(viewport, session, 80);
    check(viewport.first_visible_line == 0, "when content fits after resize, viewport returns to top");
    check(textbox_line_count(session) <= 80, "widened wrap fits in tall viewport");
}

void test_textbox_resize_pins_viewport_to_start() {
    EditorSession session{};
    textbox_init_session(session);
    for (int i = 0; i < 30; ++i) {
	textbox_apply_key(session, {TextboxKeyKind::Character, U'x'}, 6);
	textbox_apply_key(session, {TextboxKeyKind::Newline, 0}, 6);
    }
    session.current_line = session.note.text.size() - 1;
    session.current_column = 0;

    TextboxViewport viewport{};
    textbox_scroll_to_cursor(viewport, session, 4);
    check(viewport.first_visible_line > 0, "typing follow scrolled away from start");

    textbox_pin_viewport_to_start(viewport);
    textbox_clamp_viewport(viewport, session, 4);
    check(viewport.first_visible_line == 0, "smaller resize keeps showing the start of the note");
    check(textbox_line_count(session) > 4, "note still taller than the small viewport");
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

void test_delete_note_file() {
    test_notes::TempNotesDir dir;
    test_notes::write_note_file(40, "Delete", "x\n");
    check(std::filesystem::exists("notes/note_40.txt"), "delete fixture exists");
    check(delete_note_file("notes/note_40.txt"), "delete_note_file ok");
    check(!std::filesystem::exists("notes/note_40.txt"), "delete_note_file removes file");
    check(!delete_note_file(""), "delete_note_file rejects empty path");
}

void test_sticky_gui_focus_raises_z_order() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(0, "First", "a"), 260.0f, 40.0f);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(1, "Second", "b"), 300.0f, 80.0f);

    bool quit = false;
    sticky_gui_handle_event(gui, sdl_test::mouse_button_down(270.0f, 50.0f), quit);
    sticky_gui_handle_event(gui, sdl_test::mouse_button_up(270.0f, 50.0f), quit);

    check(sticky_gui_focused_note(gui).title == "First", "click raises back panel to focus");
    check(sticky_gui_focused_index(gui) == sticky_gui_panel_count(gui) - 1,
	  "focused panel is top of z-order");
}

void test_sticky_gui_delete_confirm_y_no_resave() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(41, "Gone", "z"), 40.0f, 40.0f);

    bool quit = false;
    sticky_gui_handle_event(gui, sdl_test::ctrl_shift(SDL_SCANCODE_W), quit);
    sticky_gui_handle_event(gui, sdl_test::key_down(SDL_SCANCODE_Y), quit);

    check(sticky_gui_panel_count(gui) == 0, "delete y closes panel");
    check(!std::filesystem::exists("notes/note_41.txt"), "delete y does not re-save file (BUG-005)");
}

void test_sticky_gui_panel_hit_targets() {
    StickyPanel panel{};
    panel.x = 100.0f;
    panel.y = 50.0f;
    panel.width = 280.0f;
    panel.height = 200.0f;

    StickyPanelHitTargets hit{};
    sticky_gui_panel_hit_targets(panel, hit);

    check(hit.title_x > panel.x && hit.title_x < panel.x + panel.width, "title hit inside panel width");
    check(hit.title_y >= panel.y && hit.title_y <= panel.y + sticky_panel_title_bar_height(),
	  "title hit inside title bar");
    check(hit.grip_x > panel.x + panel.width - 15.0f, "grip hit near bottom-right");
    check(hit.close_x > panel.x + panel.width - sticky_panel_close_button_width() - 16.0f,
	  "close hit near top-right");
}

void test_sticky_gui_title_edit_commit() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(42, "Before", ""), 40.0f, 40.0f);

    bool quit = false;
    sticky_gui_handle_event(gui, sdl_test::key_down(SDL_SCANCODE_F2), quit);
    check(gui.editing_title, "f2 starts title edit");
    sticky_gui_handle_event(gui, sdl_test::text_input("After"), quit);
    sticky_gui_handle_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);

    check(sticky_gui_focused_note(gui).title == "BeforeAfter", "title edit appends then commits");
}

void test_sticky_gui_help_and_delete_modal() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(43, "Modal", ""), 40.0f, 40.0f);

    bool quit = false;
    sticky_gui_handle_event(gui, sdl_test::key_down(SDL_SCANCODE_H), quit);
    check(gui.show_help, "help opens");
    sticky_gui_handle_event(gui, sdl_test::text_input("nope"), quit);
    check(sticky_gui_focused_note(gui).text[0].empty(), "help blocks typing");

    sticky_gui_handle_event(gui, sdl_test::key_down(SDL_SCANCODE_H), quit);
    sticky_gui_handle_event(gui, sdl_test::ctrl_shift(SDL_SCANCODE_W), quit);
    check(gui.pending_delete, "delete confirm opens");
    sticky_gui_handle_event(gui, sdl_test::text_input("nope"), quit);
    check(std::filesystem::exists("notes/note_43.txt"), "delete confirm blocks stray keys");
}

void test_textbox_sdl_text_and_arrows() {
    EditorSession session{};
    textbox_init_session(session);
    bool quit = false;

    textbox_handle_sdl_event(session, sdl_test::text_input("abc"), quit);
    check(textbox_line_text(session) == "abc", "sdl text input inserts");

    textbox_handle_sdl_event(session, sdl_test::key_down(SDL_SCANCODE_LEFT), quit);
    textbox_handle_sdl_event(session, sdl_test::key_down(SDL_SCANCODE_LEFT), quit);
    check(textbox_cursor_column(session) == 1, "sdl left moves cursor");

    textbox_handle_sdl_event(session, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);
    check(textbox_line_count(session) == 2, "sdl enter splits line");
}

void test_textbox_sdl_delete_key() {
    EditorSession session{};
    textbox_init_session(session);
    bool quit = false;
    textbox_handle_sdl_event(session, sdl_test::text_input("ab"), quit);
    textbox_handle_sdl_event(session, sdl_test::key_down(SDL_SCANCODE_HOME), quit);
    textbox_handle_sdl_event(session, sdl_test::key_down(SDL_SCANCODE_DELETE), quit);
    check(textbox_line_text(session) == "b", "sdl delete removes at cursor");
}

void test_sticky_panel_chrome_sizes() {
    check(sticky_panel_title_bar_height() > 0.0f, "title bar height positive");
    check(sticky_panel_close_button_width() > 0.0f, "close button width positive");
}

void test_gui_theme_persistence() {
    test_notes::TempNotesDir dir;
    check(gui_theme_load_persisted() == GuiThemeId::Minimal, "missing theme file defaults minimal");

    gui_theme_save_persisted(GuiThemeId::Cyberpunk);
    check(gui_theme_load_persisted() == GuiThemeId::Cyberpunk, "theme save/load roundtrip");

    std::ofstream out("notes/gui_theme.txt");
    out << "  RETRO  \n";
    out.close();
    check(gui_theme_load_persisted() == GuiThemeId::Retro, "theme load trims and ignores case");

    std::ofstream bad("notes/gui_theme.txt");
    bad << "unknown\n";
    bad.close();
    check(gui_theme_load_persisted() == GuiThemeId::Minimal, "invalid theme slug defaults minimal");
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
    test_textbox_wrap_long_line();
    test_textbox_reflow_on_widen();
    test_textbox_word_boundary_wrap();
    test_textbox_storage_lines_collapse_soft_wraps();
    test_textbox_repair_persisted_soft_wraps();
    test_textbox_viewport_resets_when_content_fits();
    test_textbox_resize_pins_viewport_to_start();
    test_move_up_down();
    test_delete_note_file();
    test_sticky_gui_focus_raises_z_order();
    test_sticky_gui_delete_confirm_y_no_resave();
    test_sticky_gui_panel_hit_targets();
    test_sticky_gui_title_edit_commit();
    test_sticky_gui_help_and_delete_modal();
    test_textbox_sdl_text_and_arrows();
    test_textbox_sdl_delete_key();
    test_sticky_panel_chrome_sizes();
    test_gui_theme_persistence();

    if (failures == 0) {
	std::cout << "All tests passed.\n";
	return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
