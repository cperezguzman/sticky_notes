// Deep integration harness for Phase 4 sticky GUI — simulates SDL events without a window.
// Run via: ./tests/textbox_smoke.sh  or  make textbox-smoke

#include "note_store.h"
#include "sticky_gui.h"
#include "textbox_input.h"
#include "textbox_sdl.h"

#include "sdl_event_helpers.h"
#include "test_notes_dir.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int failures = 0;
int passes = 0;

void pass(const char* label) {
    std::cout << "PASS: " << label << '\n';
    ++passes;
}

void fail(const char* label) {
    std::cerr << "FAIL: " << label << '\n';
    ++failures;
}

void check(bool ok, const char* label) {
    if (ok) {
	pass(label);
    } else {
	fail(label);
    }
}

void gui_event(StickyGui& gui, const SDL_Event& event, bool& quit) {
    sticky_gui_handle_event(gui, event, quit);
}

void gui_type(StickyGui& gui, const char* text, bool& quit) {
    gui_event(gui, sdl_test::text_input(text), quit);
}

void sdl_event(EditorSession& session, const SDL_Event& event, bool& quit) {
    textbox_handle_sdl_event(session, event, quit);
}

void sdl_type(EditorSession& session, const char* text, bool& quit) {
    sdl_event(session, sdl_test::text_input(text), quit);
}

sticky_note note_from_file(const std::string& path) {
    sticky_note note{};
    load_note_from_path(note, path);
    return note;
}

std::string body_first_line(const std::string& path) {
    const sticky_note note = note_from_file(path);
    if (note.text.empty()) {
	return "";
    }
    return note.text[0];
}

void clear_title_buffer(StickyGui& gui, bool& quit) {
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_END), quit);
    while (!gui.title_edit_buffer.empty()) {
	gui_event(gui, sdl_test::key_down(SDL_SCANCODE_BACKSPACE), quit);
    }
}

void scenario_init_loads_notes_from_disk() {
    test_notes::TempNotesDir dir;
    test_notes::write_note_file(0, "Alpha", "alpha body\n");
    test_notes::write_note_file(1, "Beta", "beta body\n");

    StickyGui gui{};
    sticky_gui_init(gui);

    check(sticky_gui_panel_count(gui) == 1, "init loads one desk panel");
    check(sticky_gui_sidebar_entry_count(gui) == 2, "sidebar lists all notes");
    check(sticky_gui_focused_note(gui).title == "Beta", "init desk shows most recent note");
}

void scenario_init_blank_when_no_notes() {
    test_notes::TempNotesDir dir;

    StickyGui gui{};
    sticky_gui_init(gui);

    check(sticky_gui_panel_count(gui) == 1, "init creates blank panel when notes empty");
    check(sticky_gui_sidebar_entry_count(gui) == 0, "sidebar empty when no notes");
    check(sticky_gui_focused_note(gui).title.empty(), "blank panel has empty title");
}

void scenario_init_caps_at_eight_notes() {
    test_notes::TempNotesDir dir;
    for (int i = 0; i < 10; ++i) {
	test_notes::write_note_file(i, "N" + std::to_string(i), "body\n");
    }

    StickyGui gui{};
    sticky_gui_init(gui);
    check(sticky_gui_panel_count(gui) == 1, "init keeps one note on desk");
    check(sticky_gui_sidebar_entry_count(gui) == 10, "sidebar lists every saved note");
}

void scenario_init_spreads_panels_on_grid() {
    test_notes::TempNotesDir dir;
    test_notes::write_note_file(0, "Only", "body\n");

    StickyGui gui{};
    sticky_gui_init(gui);

    const StickyPanel& panel = sticky_gui_panel_at(gui, 0);
    check(panel.x >= 200.0f, "desk panel sits right of open sidebar");
}

void scenario_sidebar_toggle() {
    test_notes::TempNotesDir dir;
    test_notes::write_note_file(0, "A", "a\n");
    StickyGui gui{};
    sticky_gui_init(gui);
    check(sticky_gui_sidebar_visible(gui), "sidebar open by default");

    bool quit = false;
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_B), quit);
    check(!sticky_gui_sidebar_visible(gui), "Ctrl+B hides sidebar");
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_B), quit);
    check(sticky_gui_sidebar_visible(gui), "Ctrl+B shows sidebar again");
}

void scenario_theme_badge_dropdown() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_init(gui);
    check(gui.theme_id == GuiThemeId::Minimal, "starts on minimal theme");

    bool quit = false;
    // Theme badge sits at the bottom of the sidebar (220px wide, 640px tall window).
    constexpr float kThemeBadgeX = 110.0f;
    constexpr float kThemeBadgeY = 618.0f;
    constexpr float kRetroRowY = 576.0f;

    gui_event(gui, sdl_test::mouse_button_down(kThemeBadgeX, kThemeBadgeY), quit);
    gui_event(gui, sdl_test::mouse_button_up(kThemeBadgeX, kThemeBadgeY), quit);
    check(gui.show_theme_picker, "click theme badge opens dropdown");

    gui_event(gui, sdl_test::mouse_button_down(kThemeBadgeX, kRetroRowY), quit);
    gui_event(gui, sdl_test::mouse_button_up(kThemeBadgeX, kRetroRowY), quit);
    check(!gui.show_theme_picker, "click retro row applies and closes dropdown");
    check(gui.theme_id == GuiThemeId::Retro, "retro selected from dropdown");

    gui_event(gui, sdl_test::mouse_button_down(kThemeBadgeX, kThemeBadgeY), quit);
    gui_event(gui, sdl_test::mouse_button_up(kThemeBadgeX, kThemeBadgeY), quit);
    check(gui.show_theme_picker, "badge click reopens dropdown");
    gui_event(gui, sdl_test::mouse_button_down(kThemeBadgeX, kThemeBadgeY), quit);
    gui_event(gui, sdl_test::mouse_button_up(kThemeBadgeX, kThemeBadgeY), quit);
    check(!gui.show_theme_picker, "badge click again closes dropdown");
}

void scenario_hover_focuses_panel() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(60, "Back", "a"), 260.0f, 40.0f);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(61, "Front", "b"), 400.0f, 200.0f);

    check(sticky_gui_focused_note(gui).title == "Front", "top panel focused after add");

    bool quit = false;
    gui_event(gui, sdl_test::mouse_motion(280.0f, 60.0f), quit);
    check(sticky_gui_focused_note(gui).title == "Back", "hover focuses panel under cursor");
}

void scenario_body_reflows_when_panel_widened() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(71, "Reflow", ""), 40.0f, 40.0f);

    bool quit = false;
    for (int i = 0; i < 100; ++i) {
	gui_type(gui, "d", quit);
    }

    const float start_w = sticky_gui_panel_at(gui, 0).width;
    const std::size_t narrow_cols = textbox_body_max_columns(start_w);
    const std::size_t lines_before = sticky_gui_focused_note(gui).text.size();
    check(lines_before >= 2, "long line wraps across multiple rows when narrow");

    // Desk notes are not grip-resized; widen the panel and reflow like a window expand.
    gui.panels[0].width = start_w + 240.0f;
    sticky_gui_reflow_panel(gui, 0);

    check(sticky_gui_panel_at(gui, 0).width > start_w + 100.0f, "wider desk panel width applied");

    const std::size_t lines_after = sticky_gui_focused_note(gui).text.size();
    check(lines_after < lines_before, "widened panel merges soft-wrapped lines");
    std::size_t max_line = 0;
    for (const std::string& line : sticky_gui_focused_note(gui).text) {
	max_line = std::max(max_line, line.size());
	check(line.size() <= textbox_body_max_columns(sticky_gui_panel_at(gui, 0).width),
	      "each line fits widened panel");
    }
    check(max_line > narrow_cols, "widened panel allows longer line segments");
}

void scenario_focus_panel_by_click() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(0, "Back", "a"), 240.0f, 80.0f);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(1, "Front", "b"), 400.0f, 200.0f);

    bool quit = false;
    // Click body of Back (right of sidebar / below title bar).
    gui_event(gui, sdl_test::mouse_button_down(260.0f, 120.0f), quit);
    gui_event(gui, sdl_test::mouse_button_up(260.0f, 120.0f), quit);

    check(sticky_gui_focused_note(gui).title == "Back", "click back panel focuses it");
    check(!quit, "focus click does not quit");
}

void scenario_body_typing_and_save() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(5, "Typed", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_type(gui, "hello", quit);
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_S), quit);

    check(body_first_line("notes/note_5.txt") == "hello", "ctrl+s persists typed body");
}

void scenario_multiline_body_via_enter() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(6, "Lines", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_type(gui, "a", quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);
    gui_type(gui, "b", quit);
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_S), quit);

    const sticky_note saved = note_from_file("notes/note_6.txt");
    check(saved.text.size() == 2, "enter creates second line in saved note");
    check(saved.text[0] == "a" && saved.text[1] == "b", "multiline body saved correctly");
}

void scenario_close_panel_saves_then_removes() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(7, "Close Me", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_type(gui, "new", quit);
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_W), quit);

    check(sticky_gui_panel_count(gui) == 0, "ctrl+w removes panel from desk");
    check(std::filesystem::exists("notes/note_7.txt"), "ctrl+w keeps note file on disk");
    check(body_first_line("notes/note_7.txt") == "new", "ctrl+w saves body before close");
}

void scenario_delete_from_disk_y_removes_file() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(8, "Trash", "bye"), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl_shift(SDL_SCANCODE_W), quit);
    check(gui.pending_delete, "ctrl+shift+w opens delete confirm");
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_Y), quit);

    check(sticky_gui_panel_count(gui) == 0, "delete confirm y removes panel");
    check(!std::filesystem::exists("notes/note_8.txt"), "delete confirm y removes file from disk");
}

void scenario_delete_from_disk_n_keeps_file() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(9, "Keep", "stay"), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl_shift(SDL_SCANCODE_W), quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_N), quit);

    check(sticky_gui_panel_count(gui) == 1, "delete confirm n keeps panel");
    check(std::filesystem::exists("notes/note_9.txt"), "delete confirm n keeps file");
    check(!gui.pending_delete, "delete confirm n clears pending state");
}

void scenario_delete_cancel_esc() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(10, "Esc", "x"), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl_shift(SDL_SCANCODE_W), quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_ESCAPE), quit);

    check(sticky_gui_panel_count(gui) == 1, "esc cancels delete and keeps panel");
    check(std::filesystem::exists("notes/note_10.txt"), "esc cancel keeps file");
}

void scenario_delete_does_not_resurrect_file_on_reinit() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(11, "Gone", "z"), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl_shift(SDL_SCANCODE_W), quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_Y), quit);

    StickyGui reloaded{};
    sticky_gui_init(reloaded);
    check(!std::filesystem::exists("notes/note_11.txt"), "file stays deleted after re-init");
    check(sticky_gui_panel_count(reloaded) == 1, "re-init shows blank desk when no notes remain");
}

void scenario_title_rename_f2() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(12, "Old Name", "body"), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_F2), quit);
    check(gui.editing_title, "f2 enters title edit mode");

    clear_title_buffer(gui, quit);
    gui_type(gui, "Renamed", quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);

    check(!gui.editing_title, "enter commits title edit");
    check(sticky_gui_focused_note(gui).title == "Renamed", "title updated in session");
    check(note_from_file("notes/note_12.txt").title == "Renamed", "title persisted to disk");
}

void scenario_title_edit_escape_cancels() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(13, "Stable", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_F2), quit);
    gui_type(gui, "XXX", quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_ESCAPE), quit);

    check(sticky_gui_focused_note(gui).title == "Stable", "esc cancels title edit");
}

void scenario_help_overlay_blocks_body_input() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(14, "Help", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_H), quit);
    check(gui.show_help, "h toggles help overlay on");
    gui_type(gui, "blocked", quit);
    check(sticky_gui_focused_note(gui).text[0].empty(), "help overlay blocks body typing");

    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_H), quit);
    check(!gui.show_help, "h toggles help overlay off");
}

void scenario_ctrl_n_creates_note_file() {
    test_notes::TempNotesDir dir;
    std::ofstream("notes/next_note_id.txt") << "20";

    StickyGui gui{};
    sticky_gui_init(gui);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_N), quit);
    gui_type(gui, "fresh", quit);
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_S), quit);

    check(sticky_gui_panel_count(gui) == 1, "ctrl+n keeps single desk panel");
    check(std::filesystem::exists("notes/note_20.txt"), "ctrl+n creates note file on save");
    check(body_first_line("notes/note_20.txt") == "fresh", "new note body saved");
}

void scenario_max_eight_panels() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(
	gui, test_notes::make_note_on_disk(100, "First", "one"), 100.0f, 100.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_N), quit);
    check(sticky_gui_panel_count(gui) == 1, "ctrl+n replaces desk with new blank panel");
}

void scenario_desk_title_does_not_drag_panel() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(15, "Drag", ""), 100.0f, 100.0f);

    StickyPanelHitTargets hit{};
    sticky_gui_panel_hit_targets(sticky_gui_panel_at(gui, 0), hit);
    const float start_x = sticky_gui_panel_at(gui, 0).x;
    const float start_y = sticky_gui_panel_at(gui, 0).y;

    bool quit = false;
    gui_event(gui, sdl_test::mouse_button_down(hit.title_x, hit.title_y), quit);
    gui_event(gui, sdl_test::mouse_motion(hit.title_x + 80.0f, hit.title_y + 60.0f), quit);
    gui_event(gui, sdl_test::mouse_button_up(hit.title_x + 80.0f, hit.title_y + 60.0f), quit);

    const StickyPanel& panel = sticky_gui_panel_at(gui, sticky_gui_focused_index(gui));
    check(panel.x == start_x && panel.y == start_y, "desk note title does not drag panel");
}

void scenario_desk_note_does_not_resize() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(16, "Resize", ""), 50.0f, 50.0f);

    const float start_w = sticky_gui_panel_at(gui, 0).width;
    StickyPanelHitTargets hit{};
    sticky_gui_panel_hit_targets(sticky_gui_panel_at(gui, 0), hit);

    bool quit = false;
    gui_event(gui, sdl_test::mouse_button_down(hit.grip_x, hit.grip_y), quit);
    gui_event(gui, sdl_test::mouse_motion(hit.grip_x + 40.0f, hit.grip_y + 30.0f), quit);
    gui_event(gui, sdl_test::mouse_button_up(hit.grip_x + 40.0f, hit.grip_y + 30.0f), quit);

    check(sticky_gui_panel_at(gui, 0).width == start_w, "desk note grip does not resize");
}

void scenario_sidebar_wheel_scrolls_list() {
    test_notes::TempNotesDir dir;
    for (int i = 0; i < 12; ++i) {
	test_notes::write_note_file(i, "N" + std::to_string(i), "body\n");
    }

    StickyGui gui{};
    sticky_gui_init(gui);
    gui.desk_h = 200.0f; // short window so the note list overflows
    check(sticky_gui_sidebar_entry_count(gui) == 12, "sidebar has many notes");
    check(gui.sidebar_scroll == 0, "sidebar starts at top");

    bool quit = false;
    gui_event(gui, sdl_test::mouse_wheel(40.0f, 100.0f, -1.0f), quit);
    check(gui.sidebar_scroll > 0, "wheel over sidebar scrolls note list");

    const std::size_t mid = gui.sidebar_scroll;
    gui_event(gui, sdl_test::mouse_wheel(40.0f, 100.0f, 1.0f), quit);
    check(gui.sidebar_scroll < mid, "wheel up scrolls sidebar toward top");
}

void scenario_note_body_scrollbar_scrolls() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(72, "Tall", ""), 240.0f, 40.0f);
    gui.panels[0].width = 280.0f;
    gui.panels[0].height = 120.0f;

    bool quit = false;
    for (int i = 0; i < 40; ++i) {
	gui_type(gui, "a", quit);
	gui_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);
    }
    check(textbox_body_scrollbar_needed(gui.panels[0].session, gui.panels[0].height),
	  "long note shows body scrollbar");

    // Cursor follow leaves the view near the bottom; jump toward the top via the track.
    textbox_pin_viewport_to_start(gui.panels[0].viewport);
    float track_x = 0.0f;
    float track_y = 0.0f;
    float track_w = 0.0f;
    float track_h = 0.0f;
    const StickyPanel& panel = gui.panels[0];
    textbox_body_scrollbar_track_rect(panel.x, panel.y, panel.width, panel.height, track_x, track_y,
				      track_w, track_h);
    const float click_x = track_x + track_w * 0.5f;
    const float click_y = track_y + track_h - 4.0f;
    gui_event(gui, sdl_test::mouse_button_down(click_x, click_y), quit);
    gui_event(gui, sdl_test::mouse_button_up(click_x, click_y), quit);
    check(gui.panels[0].viewport.first_visible_line > 0, "body scrollbar click scrolls note");
}

void scenario_close_button_saves_and_removes() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(17, "X Close", ""), 80.0f, 80.0f);

    StickyPanelHitTargets hit{};
    sticky_gui_panel_hit_targets(sticky_gui_panel_at(gui, 0), hit);

    bool quit = false;
    gui_type(gui, "saved", quit);
    gui_event(gui, sdl_test::mouse_button_down(hit.close_x, hit.close_y), quit);

    check(sticky_gui_panel_count(gui) == 0, "close button removes panel");
    check(body_first_line("notes/note_17.txt") == "saved", "close button saves before remove");
}

void scenario_save_all_on_quit_path() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(18, "Quit Save", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_type(gui, "persist", quit);
    sticky_gui_save_all(gui);

    check(body_first_line("notes/note_18.txt") == "persist", "save_all writes focused body");
}

void scenario_esc_requests_quit() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(19, "Quit", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_ESCAPE), quit);
    check(quit, "esc sets quit requested on focused body");
}

void scenario_textbox_sdl_esc_quit() {
    EditorSession session{};
    textbox_init_session(session);
    bool quit = false;
    sdl_event(session, sdl_test::key_down(SDL_SCANCODE_ESCAPE), quit);
    check(quit, "textbox_handle_sdl_event esc quits");
}

void scenario_backspace_merge_via_sdl() {
    EditorSession session{};
    textbox_init_session(session);
    bool quit = false;
    sdl_type(session, "hi", quit);
    sdl_event(session, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);
    sdl_type(session, "!", quit);
    sdl_event(session, sdl_test::key_down(SDL_SCANCODE_HOME), quit);
    sdl_event(session, sdl_test::key_down(SDL_SCANCODE_BACKSPACE), quit);

    check(textbox_line_count(session) == 1, "sdl backspace at line start merges");
    check(textbox_line_text(session) == "hi!", "sdl merge preserves text");
}

void scenario_delete_note_file_helper() {
    test_notes::TempNotesDir dir;
    test_notes::write_note_file(30, "Delete API", "x\n");
    check(std::filesystem::exists("notes/note_30.txt"), "fixture exists before delete");
    check(delete_note_file("notes/note_30.txt"), "delete_note_file succeeds");
    check(!std::filesystem::exists("notes/note_30.txt"), "delete_note_file removes path");
    check(!delete_note_file(""), "delete_note_file rejects empty path");
}

void scenario_title_whitespace_trims_to_untitled() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(31, "Has Title", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_F2), quit);
    clear_title_buffer(gui, quit);
    gui_type(gui, "   ", quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);

    check(sticky_gui_focused_note(gui).title == "Untitled", "whitespace-only title becomes Untitled");
}

void scenario_f1_toggles_help() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(32, "F1", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_F1), quit);
    check(gui.show_help, "f1 toggles help overlay");
}

void scenario_title_edit_blocks_body_keys() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(33, "Title Mode", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_F2), quit);
    gui_type(gui, "Z", quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);

    check(sticky_gui_focused_note(gui).text[0].empty(), "title edit does not insert into body");
    check(sticky_gui_focused_note(gui).title == "Title ModeZ", "title edit appends to title buffer");
}

void scenario_theme_cycle() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(50, "Theme", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_T), quit);
    check(gui.theme_id == GuiThemeId::Retro, "ctrl+t cycles to retro theme");

    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_3), quit);
    check(gui.theme_id == GuiThemeId::Cyberpunk, "ctrl+3 sets cyberpunk theme");
}

void scenario_theme_persists_on_reinit() {
    test_notes::TempNotesDir dir;
    {
	StickyGui gui{};
	sticky_gui_init(gui);
	bool quit = false;
	gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_2), quit);
	check(gui.theme_id == GuiThemeId::Retro, "ctrl+2 sets retro theme");
    }

    StickyGui reloaded{};
    sticky_gui_init(reloaded);
    check(reloaded.theme_id == GuiThemeId::Retro, "theme restored on next init");
}

void scenario_undo_in_gui() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(51, "Undo", ""), 40.0f, 40.0f);

    bool quit = false;
    gui_type(gui, "a", quit);
    gui_type(gui, "b", quit);
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_Z), quit);
    check(sticky_gui_focused_note(gui).text[0] == "a", "ctrl+z undoes last typed char");
}

void scenario_find_in_gui() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(52, "Find", "needle"), 40.0f, 40.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_F), quit);
    check(gui.editing_find, "ctrl+f opens find bar");
    gui_type(gui, "needle", quit);
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);
    check(gui.find_status == "Match found", "find locates needle in body");
}

void scenario_sidebar_search_filters_notes() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(60, "Recipes", "pasta"), 40.0f,
				   40.0f);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(61, "Travel", "japan trip"),
				   200.0f, 40.0f);
    sticky_gui_init(gui);

    check(sticky_gui_sidebar_entry_count(gui) == 2, "sidebar lists both notes before search");

    bool quit = false;
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_K), quit);
    check(gui.editing_sidebar_search, "ctrl+k opens sidebar search");
    gui_type(gui, "japan", quit);
    check(sticky_gui_sidebar_entry_count(gui) == 1, "search filters to body match");
    check(gui.sidebar_entries[0].title == "Travel", "matching note is Travel");

    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_ESCAPE), quit);
    check(!gui.editing_sidebar_search, "esc exits sidebar search");
    check(gui.sidebar_search_buffer.empty(), "esc clears search query");
    check(sticky_gui_sidebar_entry_count(gui) == 2, "esc restores full sidebar list");
}

void scenario_open_picker_focuses_existing() {
    test_notes::TempNotesDir dir;
    StickyGui gui{};
    sticky_gui_reset(gui);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(53, "Open Me", "hi"), 40.0f, 40.0f);
    sticky_gui_add_panel_from_note(gui, test_notes::make_note_on_disk(54, "Other", ""), 200.0f, 200.0f);

    bool quit = false;
    gui_event(gui, sdl_test::ctrl(SDL_SCANCODE_O), quit);
    check(gui.show_open_picker, "ctrl+o opens note picker");
    gui_event(gui, sdl_test::key_down(SDL_SCANCODE_RETURN), quit);
    check(sticky_gui_focused_note(gui).title == "Open Me", "open picker focuses existing panel");
    check(sticky_gui_panel_count(gui) == 2, "open existing does not duplicate panel");
}

} // namespace

int main() {
    scenario_init_loads_notes_from_disk();
    scenario_init_blank_when_no_notes();
    scenario_init_caps_at_eight_notes();
    scenario_init_spreads_panels_on_grid();
    scenario_sidebar_toggle();
    scenario_theme_badge_dropdown();
    scenario_focus_panel_by_click();
    scenario_body_typing_and_save();
    scenario_multiline_body_via_enter();
    scenario_close_panel_saves_then_removes();
    scenario_delete_from_disk_y_removes_file();
    scenario_delete_from_disk_n_keeps_file();
    scenario_delete_cancel_esc();
    scenario_delete_does_not_resurrect_file_on_reinit();
    scenario_title_rename_f2();
    scenario_title_edit_escape_cancels();
    scenario_help_overlay_blocks_body_input();
    scenario_ctrl_n_creates_note_file();
    scenario_max_eight_panels();
    scenario_desk_title_does_not_drag_panel();
    scenario_desk_note_does_not_resize();
    scenario_sidebar_wheel_scrolls_list();
    scenario_note_body_scrollbar_scrolls();
    scenario_close_button_saves_and_removes();
    scenario_save_all_on_quit_path();
    scenario_esc_requests_quit();
    scenario_textbox_sdl_esc_quit();
    scenario_backspace_merge_via_sdl();
    scenario_delete_note_file_helper();
    scenario_title_whitespace_trims_to_untitled();
    scenario_f1_toggles_help();
    scenario_title_edit_blocks_body_keys();
    scenario_theme_cycle();
    scenario_theme_persists_on_reinit();
    scenario_undo_in_gui();
    scenario_find_in_gui();
    scenario_sidebar_search_filters_notes();
    scenario_open_picker_focuses_existing();
    scenario_hover_focuses_panel();
    scenario_body_reflows_when_panel_widened();

    std::cout << "\nResults: " << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
