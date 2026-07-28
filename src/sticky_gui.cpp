#include "sticky_gui.h"

#include "note_store.h"
#include "sticky_note.h"
#include "textbox_sdl.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <utility>

namespace {
constexpr float kMinPanelW = 160.0f;
constexpr float kMinPanelH = 120.0f;
constexpr float kResizeGrip = 10.0f;
constexpr float kPadding = 8.0f;
constexpr float kLineH = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
constexpr std::size_t kMaxPanels = 8;
constexpr std::size_t kMaxTitleLen = 64;
constexpr std::size_t kMaxFindLen = 64;
constexpr Uint32 kSaveToastMs = 1800;
constexpr Uint32 kTitleDoubleClickMs = 400;
constexpr float kDefaultDeskW = 960.0f;
constexpr float kDefaultDeskH = 640.0f;
constexpr float kSidebarWidth = 220.0f;
constexpr float kSidebarTabWidth = 22.0f;
constexpr float kSidebarRowH = 18.0f;
constexpr float kSidebarHeaderH = 28.0f;
constexpr float kSidebarSearchH = 18.0f;
constexpr float kSidebarPad = 8.0f;
constexpr int kSidebarDragThresholdPx = 8;
constexpr std::size_t kMaxSidebarSearchLen = 48;
constexpr const char* kDeskStatePath = "notes/desk_state.txt";

// Matches sandbox window (textbox_main.cpp); used for startup grid layout.
constexpr float kDeskMargin = 28.0f;
constexpr float kPopOutPanelW = 280.0f;
constexpr float kPopOutPanelH = 180.0f;
constexpr float kThemeBadgeH = 28.0f;
constexpr std::size_t kThemePickerCount = static_cast<std::size_t>(GuiThemeId::Count);

bool point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

int panel_index_for_path(const StickyGui& gui, const std::string& path);
void close_panel_at(StickyGui& gui, std::size_t index, bool save);
void add_panel(StickyGui& gui, StickyPanel panel);
void focus_panel(StickyGui& gui, std::size_t index);
bool extract_panel_at(StickyGui& gui, std::size_t index, StickyPanel& out);
void cancel_theme_picker(StickyGui& gui);
void render_theme_badge(SDL_Renderer* renderer, const StickyGui& gui, const StickyGuiTheme& theme);
void render_theme_picker(SDL_Renderer* renderer, const StickyGui& gui, const StickyGuiTheme& theme);
void begin_sidebar_search(StickyGui& gui);
void clear_sidebar_search(StickyGui& gui);

float sidebar_theme_top_y() {
    return kDefaultDeskH - kSidebarPad - kThemeBadgeH - 4.0f;
}

float sidebar_list_top_y() {
    return kSidebarHeaderH + kSidebarSearchH;
}

std::string ascii_lower(std::string s) {
    for (char& ch : s) {
	ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

bool note_matches_sidebar_query(const sticky_note& note, const std::string& query) {
    const std::string q = ascii_lower(query);
    if (q.empty()) {
	return true;
    }
    if (ascii_lower(note.title).find(q) != std::string::npos) {
	return true;
    }
    for (const std::string& line : note.text) {
	if (ascii_lower(line).find(q) != std::string::npos) {
	    return true;
	}
    }
    return false;
}

bool ctrl_down(const SDL_KeyboardEvent& key) {
    return (key.mod & SDL_KMOD_CTRL) != 0;
}

bool shift_down(const SDL_KeyboardEvent& key) {
    return (key.mod & SDL_KMOD_SHIFT) != 0;
}

void close_button_rect(const StickyPanel& panel, float& out_x, float& out_y, float& out_w, float& out_h) {
    sticky_panel_close_button_rect(panel.x, panel.y, panel.width, out_x, out_y, out_w, out_h);
}

StickyPanel make_panel_from_note(const sticky_note& note, float x, float y) {
    StickyPanel panel{};
    panel.x = x;
    panel.y = y;
    panel.session.note = note;
    panel.session.current_line = 0;
    panel.session.current_column = 0;
    if (panel.session.note.text.empty()) {
	panel.session.note.text.push_back("");
    }
    textbox_repair_persisted_soft_wraps(panel.session.note.text);
    editor_reset_cursor(panel.session);
    editor_clear_history(panel.session);
    textbox_init_hard_breaks_for_loaded_note(panel.session);
    return panel;
}

StickyPanel make_blank_panel(float x, float y) {
    StickyPanel panel{};
    panel.x = x;
    panel.y = y;
    textbox_init_session(panel.session);
    return panel;
}

float sidebar_occupies_width(const StickyGui& gui) {
    return gui.sidebar_visible ? kSidebarWidth : kSidebarTabWidth;
}

void desk_content_rect(const StickyGui& gui, float& out_x, float& out_y, float& out_w, float& out_h) {
    out_x = sidebar_occupies_width(gui) + kDeskMargin;
    out_y = kDeskMargin;
    out_w = kDefaultDeskW - out_x - kDeskMargin;
    out_h = kDefaultDeskH - 2.0f * kDeskMargin;
}

void size_panel_for_desk(const StickyGui& gui, StickyPanel& panel) {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    desk_content_rect(gui, x, y, w, h);
    panel.x = x;
    panel.y = y;
    panel.width = std::max(kMinPanelW, w);
    panel.height = std::max(kMinPanelH, h);
    textbox_enforce_wrap(panel.session, textbox_body_max_columns(panel.width));
}

void size_panel_for_pop_out(StickyPanel& panel) {
    panel.width = kPopOutPanelW;
    panel.height = kPopOutPanelH;
    textbox_enforce_wrap(panel.session, textbox_body_max_columns(panel.width));
}

void place_panel_on_desk(const StickyGui& gui, StickyPanel& panel) {
    size_panel_for_desk(gui, panel);
}

void desk_state_save(const StickyGui& gui) {
    ensure_notes_data_dir();
    std::ofstream out(kDeskStatePath);
    if (!out) {
	return;
    }
    out << (gui.sidebar_visible ? "1" : "0") << '\n';
    if (!gui.panels.empty()) {
	out << gui.panels[gui.focused].session.note.note_path << '\n';
    } else {
	out << '\n';
    }
}

bool desk_state_load(bool& sidebar_open, std::string& desk_note_path) {
    std::ifstream in(kDeskStatePath);
    if (!in) {
	return false;
    }
    std::string flag;
    if (!std::getline(in, flag)) {
	return false;
    }
    sidebar_open = (flag == "1");
    if (!std::getline(in, desk_note_path)) {
	return false;
    }
    return true;
}

std::string find_most_recently_edited_note_path() {
    const NoteIndex idx = build_note_index();
    std::string best_path;
    std::chrono::system_clock::time_point best_time{};
    int best_id = -1;
    bool any = false;

    for (const auto& entry : idx) {
	sticky_note note{};
	if (!load_note_from_path(note, entry.second.second)) {
	    continue;
	}
	if (!any || note.last_edited > best_time
	    || (note.last_edited == best_time && note.id > best_id)) {
	    best_time = note.last_edited;
	    best_id = note.id;
	    best_path = entry.second.second;
	    any = true;
	}
    }
    return best_path;
}

void refresh_sidebar_entries(StickyGui& gui) {
    gui.sidebar_entries.clear();
    const NoteIndex idx = build_note_index();
    for (const auto& entry : idx) {
	sticky_note note{};
	if (!load_note_from_path(note, entry.second.second)) {
	    continue;
	}
	if (!note_matches_sidebar_query(note, gui.sidebar_search_buffer)) {
	    continue;
	}
	SidebarEntry row{};
	row.id = entry.first;
	row.title = entry.second.first.empty() ? "Untitled" : entry.second.first;
	row.path = entry.second.second;
	row.last_edited = note.last_edited;
	row.on_desk = panel_index_for_path(gui, row.path) >= 0;
	gui.sidebar_entries.push_back(std::move(row));
    }
    std::sort(gui.sidebar_entries.begin(), gui.sidebar_entries.end(),
	      [](const SidebarEntry& a, const SidebarEntry& b) {
		  if (a.last_edited != b.last_edited) {
		      return a.last_edited > b.last_edited;
		  }
		  return a.id > b.id;
	      });
    if (gui.sidebar_scroll >= gui.sidebar_entries.size()) {
	gui.sidebar_scroll = 0;
    }
}

void clear_desk_panels(StickyGui& gui, bool save) {
    while (!gui.panels.empty()) {
	close_panel_at(gui, 0, save);
    }
}

void set_desk_panel(StickyGui& gui, StickyPanel panel) {
    clear_desk_panels(gui, true);
    place_panel_on_desk(gui, panel);
    add_panel(gui, std::move(panel));
    refresh_sidebar_entries(gui);
}

bool load_desk_panel_from_path(StickyGui& gui, const std::string& path) {
    if (path.empty()) {
	return false;
    }
    sticky_note note{};
    if (!load_note_from_path(note, path)) {
	return false;
    }
    StickyPanel panel = make_panel_from_note(note, 0.0f, 0.0f);
    set_desk_panel(gui, std::move(panel));
    return true;
}

void begin_sidebar_pop_out(StickyGui& gui, const SidebarEntry& entry, SDL_Window* desk_window,
			   float desk_mx, float desk_my) {
    const int desk_idx = panel_index_for_path(gui, entry.path);
    if (desk_idx >= 0) {
	extract_panel_at(gui, static_cast<std::size_t>(desk_idx), gui.sidebar_pop_out_panel);
    } else {
	sticky_note note{};
	if (!load_note_from_path(note, entry.path)) {
	    return;
	}
	gui.sidebar_pop_out_panel = make_panel_from_note(note, 0.0f, 0.0f);
	size_panel_for_pop_out(gui.sidebar_pop_out_panel);
    }

    int desk_x = 0;
    int desk_y = 0;
    if (desk_window != nullptr) {
	SDL_GetWindowPosition(desk_window, &desk_x, &desk_y);
    }
    gui.sidebar_pop_out_screen_x = desk_x + static_cast<int>(desk_mx);
    gui.sidebar_pop_out_screen_y = desk_y + static_cast<int>(desk_my);
    gui.sidebar_pop_out_pending = true;
    refresh_sidebar_entries(gui);
}

void show_sidebar_note_on_desk(StickyGui& gui, const SidebarEntry& entry) {
    const int existing = panel_index_for_path(gui, entry.path);
    if (existing >= 0) {
	focus_panel(gui, static_cast<std::size_t>(existing));
	return;
    }
    load_desk_panel_from_path(gui, entry.path);
}

float sidebar_toggle_y() {
    return 300.0f;
}

bool point_in_sidebar_toggle(float px, float py) {
    const float y = sidebar_toggle_y();
    return point_in_rect(px, py, 0.0f, y, kSidebarTabWidth, 40.0f);
}

bool sidebar_row_rect(const StickyGui& gui, std::size_t index, float& out_x, float& out_y, float& out_w,
		      float& out_h) {
    if (!gui.sidebar_visible || index < gui.sidebar_scroll
	|| index >= gui.sidebar_entries.size()) {
	return false;
    }
    const std::size_t visible_index = index - gui.sidebar_scroll;
    const float list_top = sidebar_list_top_y();
    const float theme_top = sidebar_theme_top_y();
    const std::size_t max_visible_rows =
	static_cast<std::size_t>((theme_top - list_top) / kSidebarRowH);
    if (visible_index >= max_visible_rows) {
	return false;
    }
    out_x = kSidebarPad;
    out_y = list_top + static_cast<float>(visible_index) * kSidebarRowH;
    out_w = kSidebarWidth - 2.0f * kSidebarPad;
    out_h = kSidebarRowH;
    return true;
}

bool point_in_sidebar_search(float px, float py) {
    return point_in_rect(px, py, kSidebarPad, kSidebarHeaderH, kSidebarWidth - 2.0f * kSidebarPad,
			 kSidebarSearchH);
}

int sidebar_row_at_point(const StickyGui& gui, float px, float py) {
    if (!gui.sidebar_visible || px < 0.0f || px >= kSidebarWidth || py < sidebar_list_top_y()
	|| py >= sidebar_theme_top_y()) {
	return -1;
    }
    const std::size_t visible_row =
	static_cast<std::size_t>((py - sidebar_list_top_y()) / kSidebarRowH);
    const std::size_t index = gui.sidebar_scroll + visible_row;
    if (index >= gui.sidebar_entries.size()) {
	return -1;
    }
    float rx = 0.0f;
    float ry = 0.0f;
    float rw = 0.0f;
    float rh = 0.0f;
    if (!sidebar_row_rect(gui, index, rx, ry, rw, rh)) {
	return -1;
    }
    if (!point_in_rect(px, py, rx, ry, rw, rh)) {
	return -1;
    }
    return static_cast<int>(index);
}

void toggle_sidebar(StickyGui& gui) {
    gui.sidebar_visible = !gui.sidebar_visible;
    if (!gui.sidebar_visible) {
	cancel_theme_picker(gui);
	gui.editing_sidebar_search = false;
    }
    if (!gui.panels.empty()) {
	place_panel_on_desk(gui, gui.panels[gui.focused]);
    }
}

bool handle_sidebar_mouse(StickyGui& gui, const SDL_Event& event, SDL_Window* desk_window) {
    if (gui.pending_delete || gui.show_help || gui.show_open_picker || gui.editing_find) {
	return false;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	const float mx = event.motion.x;
	const float my = event.motion.y;

	if (gui.sidebar_drag_active && !gui.sidebar_drag_pop_out_done) {
	    const float dx = mx - gui.sidebar_drag_start_mx;
	    const float dy = my - gui.sidebar_drag_start_my;
	    if (dx * dx + dy * dy
		>= static_cast<float>(kSidebarDragThresholdPx * kSidebarDragThresholdPx)) {
		if (gui.sidebar_drag_index < gui.sidebar_entries.size()) {
		    begin_sidebar_pop_out(gui, gui.sidebar_entries[gui.sidebar_drag_index],
					  desk_window, mx, my);
		    gui.sidebar_drag_pop_out_done = true;
		}
	    }
	    return true;
	}
	return false;
    }

    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP) {
	return false;
    }
    if (event.button.button != SDL_BUTTON_LEFT) {
	return false;
    }

    const float mx = event.button.x;
    const float my = event.button.y;

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
	if (point_in_sidebar_toggle(mx, my)) {
	    toggle_sidebar(gui);
	    return true;
	}
	if (!gui.sidebar_visible) {
	    return mx < kSidebarTabWidth;
	}
	if (point_in_sidebar_search(mx, my)) {
	    begin_sidebar_search(gui);
	    return true;
	}
	const int row = sidebar_row_at_point(gui, mx, my);
	if (row >= 0) {
	    if (gui.editing_sidebar_search) {
		gui.editing_sidebar_search = false;
	    }
	    gui.sidebar_drag_active = true;
	    gui.sidebar_drag_pop_out_done = false;
	    gui.sidebar_drag_index = static_cast<std::size_t>(row);
	    gui.sidebar_drag_start_mx = mx;
	    gui.sidebar_drag_start_my = my;
	    return true;
	}
	if (gui.editing_sidebar_search) {
	    gui.editing_sidebar_search = false;
	    return true;
	}
	return false;
    }

    if (gui.sidebar_drag_active) {
	const bool was_drag = gui.sidebar_drag_pop_out_done;
	gui.sidebar_drag_active = false;
	if (!was_drag && gui.sidebar_drag_index < gui.sidebar_entries.size()) {
	    show_sidebar_note_on_desk(gui, gui.sidebar_entries[gui.sidebar_drag_index]);
	}
	gui.sidebar_drag_pop_out_done = false;
	return true;
    }

    return false;
}

void render_sidebar(SDL_Renderer* renderer, StickyGui& gui, const StickyGuiTheme& theme) {
    if (gui.sidebar_visible) {
	SDL_FRect panel{0.0f, 0.0f, kSidebarWidth, kDefaultDeskH};
	gui_set_render_color(renderer, theme.overlay_bg);
	SDL_RenderFillRect(renderer, &panel);
	gui_set_render_color(renderer, theme.overlay_border);
	SDL_RenderRect(renderer, &panel);

	gui_set_render_color(renderer, theme.overlay_text);
	SDL_RenderDebugText(renderer, kSidebarPad, kSidebarPad, "Notes");

	const float search_y = kSidebarHeaderH;
	if (gui.editing_sidebar_search) {
	    gui_set_render_color(renderer, theme.overlay_text);
	    std::string shown = gui.sidebar_search_buffer + "|";
	    if (shown.size() > 24) {
		shown = shown.substr(shown.size() - 24);
	    }
	    SDL_RenderDebugText(renderer, kSidebarPad, search_y, shown.c_str());
	} else if (!gui.sidebar_search_buffer.empty()) {
	    gui_set_render_color(renderer, theme.overlay_text);
	    std::string shown = gui.sidebar_search_buffer;
	    if (shown.size() > 24) {
		shown = shown.substr(0, 21) + "...";
	    }
	    SDL_RenderDebugText(renderer, kSidebarPad, search_y, shown.c_str());
	} else {
	    gui_set_render_color(renderer, theme.overlay_muted);
	    SDL_RenderDebugText(renderer, kSidebarPad, search_y, "Ctrl+K search");
	}

	float row_y = sidebar_list_top_y();
	const float theme_top = sidebar_theme_top_y();
	const std::size_t max_visible_rows =
	    static_cast<std::size_t>((theme_top - sidebar_list_top_y()) / kSidebarRowH);
	const std::size_t max_row = std::min(gui.sidebar_entries.size(),
					     gui.sidebar_scroll + max_visible_rows);
	for (std::size_t i = gui.sidebar_scroll; i < max_row; ++i) {
	    const SidebarEntry& entry = gui.sidebar_entries[i];
	    if (entry.on_desk) {
		SDL_FRect row{kSidebarPad, row_y - 1.0f, kSidebarWidth - 2.0f * kSidebarPad,
			      kSidebarRowH};
		gui_set_render_color(renderer, theme.panel_border_focus);
		SDL_RenderFillRect(renderer, &row);
	    }
	    gui_set_render_color(renderer, entry.on_desk ? theme.desk : theme.overlay_text);
	    std::string line = entry.title;
	    if (line.size() > 24) {
		line = line.substr(0, 21) + "...";
	    }
	    SDL_RenderDebugText(renderer, kSidebarPad + 4.0f, row_y, line.c_str());
	    row_y += kSidebarRowH;
	}

	if (gui.sidebar_entries.empty()) {
	    gui_set_render_color(renderer, theme.overlay_muted);
	    const char* empty_msg =
		gui.sidebar_search_buffer.empty() ? "(no notes)" : "(no matches)";
	    SDL_RenderDebugText(renderer, kSidebarPad, sidebar_list_top_y(), empty_msg);
	}

	render_theme_badge(renderer, gui, theme);
	if (gui.show_theme_picker) {
	    render_theme_picker(renderer, gui, theme);
	}
    }

    const float tab_y = sidebar_toggle_y();
    SDL_FRect tab{0.0f, tab_y, kSidebarTabWidth, 40.0f};
    gui_set_render_color(renderer, theme.title_bar);
    SDL_RenderFillRect(renderer, &tab);
    gui_set_render_color(renderer, theme.title_text);
    SDL_RenderDebugText(renderer, 6.0f, tab_y + 12.0f, gui.sidebar_visible ? "<" : ">");
}

int panel_at_point(const StickyGui& gui, float px, float py) {
    for (int i = static_cast<int>(gui.panels.size()) - 1; i >= 0; --i) {
	const StickyPanel& p = gui.panels[static_cast<std::size_t>(i)];
	if (point_in_rect(px, py, p.x, p.y, p.width, p.height)) {
	    return i;
	}
    }
    return -1;
}

int panel_index_for_path(const StickyGui& gui, const std::string& path) {
    if (path.empty()) {
	return -1;
    }
    for (std::size_t i = 0; i < gui.panels.size(); ++i) {
	if (gui.panels[i].session.note.note_path == path) {
	    return static_cast<int>(i);
	}
    }
    return -1;
}

void cancel_title_edit(StickyGui& gui);
void cancel_find_edit(StickyGui& gui);
void cancel_open_picker(StickyGui& gui);

void focus_panel(StickyGui& gui, std::size_t index) {
    if (index >= gui.panels.size()) {
	return;
    }
    StickyPanel panel = std::move(gui.panels[index]);
    gui.panels.erase(gui.panels.begin() + static_cast<std::ptrdiff_t>(index));
    gui.panels.push_back(std::move(panel));
    gui.focused = gui.panels.size() - 1;
}

void add_panel(StickyGui& gui, StickyPanel panel) {
    gui.panels.push_back(std::move(panel));
    gui.focused = gui.panels.size() - 1;
}

void persist_panel_if_possible(StickyPanel& panel) {
    const bool has_title = !panel.session.note.title.empty();
    bool has_body = false;
    for (const std::string& line : panel.session.note.text) {
	if (!line.empty()) {
	    has_body = true;
	    break;
	}
    }
    if (panel.session.note.note_path.empty() && !has_title && !has_body) {
	return;
    }
    if (panel.session.note.note_path.empty()) {
	const std::string title = panel.session.note.title.empty()
	    ? "Untitled"
	    : panel.session.note.title;
	sticky_note created = create_note_silent(title);
	created.text = textbox_storage_lines(panel.session);
	panel.session.note = created;
	textbox_init_hard_breaks_for_loaded_note(panel.session);
	save_note(panel.session.note);
	return;
    }
    sticky_note to_save = panel.session.note;
    to_save.text = textbox_storage_lines(panel.session);
    save_note(to_save);
}

void trigger_save_toast(StickyGui& gui) {
    gui.save_toast_until_ms = SDL_GetTicks() + kSaveToastMs;
}

void close_panel_at(StickyGui& gui, std::size_t index, bool save = true) {
    if (index >= gui.panels.size()) {
	return;
    }
    if (save) {
	persist_panel_if_possible(gui.panels[index]);
    }
    gui.panels.erase(gui.panels.begin() + static_cast<std::ptrdiff_t>(index));
    if (gui.panels.empty()) {
	gui.focused = 0;
	gui.editing_title = false;
	refresh_sidebar_entries(gui);
	return;
    }
    if (gui.focused >= gui.panels.size()) {
	gui.focused = gui.panels.size() - 1;
    }
    refresh_sidebar_entries(gui);
}

bool extract_panel_at(StickyGui& gui, std::size_t index, StickyPanel& out) {
    if (index >= gui.panels.size()) {
	return false;
    }
    if (gui.editing_title) {
	cancel_title_edit(gui);
    }
    if (gui.editing_find) {
	cancel_find_edit(gui);
    }
    if (gui.show_open_picker) {
	cancel_open_picker(gui);
    }
    persist_panel_if_possible(gui.panels[index]);
    out = std::move(gui.panels[index]);
    gui.panels.erase(gui.panels.begin() + static_cast<std::ptrdiff_t>(index));
    if (gui.panels.empty()) {
	gui.focused = 0;
    } else if (gui.focused >= gui.panels.size()) {
	gui.focused = gui.panels.size() - 1;
    }
    gui.pop_out_requested = false;
    refresh_sidebar_entries(gui);
    return true;
}

void request_pop_out(StickyGui& gui, std::size_t index) {
    if (index < gui.panels.size()) {
	gui.pop_out_requested = true;
	gui.pop_out_panel_index = index;
    }
}

void refresh_open_picker(StickyGui& gui) {
    gui.open_picker_entries.clear();
    const NoteIndex idx = build_note_index();
    for (const auto& entry : idx) {
	gui.open_picker_entries.push_back(
	    OpenPickerEntry{entry.first, entry.second.first, entry.second.second});
    }
    if (gui.open_picker_cursor >= gui.open_picker_entries.size()) {
	gui.open_picker_cursor = gui.open_picker_entries.empty() ? 0 : gui.open_picker_entries.size() - 1;
    }
}

void begin_open_picker(StickyGui& gui) {
    cancel_title_edit(gui);
    cancel_find_edit(gui);
    gui.editing_sidebar_search = false;
    refresh_open_picker(gui);
    gui.show_open_picker = true;
    gui.open_picker_cursor = 0;
}

void cancel_open_picker(StickyGui& gui) {
    gui.show_open_picker = false;
    gui.open_picker_entries.clear();
    gui.open_picker_cursor = 0;
}

void open_picker_selection(StickyGui& gui) {
    if (gui.open_picker_entries.empty()) {
	cancel_open_picker(gui);
	return;
    }
    const OpenPickerEntry& pick = gui.open_picker_entries[gui.open_picker_cursor];
    const int existing = panel_index_for_path(gui, pick.path);
    if (existing >= 0) {
	focus_panel(gui, static_cast<std::size_t>(existing));
	cancel_open_picker(gui);
	return;
    }
    sticky_note note{};
    if (!load_note_from_path(note, pick.path)) {
	cancel_open_picker(gui);
	return;
    }
    StickyPanel panel = make_panel_from_note(note, 0.0f, 0.0f);
    set_desk_panel(gui, std::move(panel));
    cancel_open_picker(gui);
}

void begin_title_edit(StickyGui& gui) {
    if (gui.panels.empty()) {
	return;
    }
    gui.editing_sidebar_search = false;
    gui.editing_title = true;
    gui.title_edit_buffer = gui.panels[gui.focused].session.note.title;
    if (gui.title_edit_buffer.empty()) {
	gui.title_edit_buffer = "Untitled";
    }
    gui.title_cursor = gui.title_edit_buffer.size();
}

void cancel_title_edit(StickyGui& gui) {
    gui.editing_title = false;
    gui.title_edit_buffer.clear();
    gui.title_cursor = 0;
}

void commit_title_edit(StickyGui& gui) {
    if (!gui.editing_title || gui.panels.empty()) {
	return;
    }
    std::string title = gui.title_edit_buffer;
    while (!title.empty() && std::isspace(static_cast<unsigned char>(title.front()))) {
	title.erase(title.begin());
    }
    while (!title.empty() && std::isspace(static_cast<unsigned char>(title.back()))) {
	title.pop_back();
    }
    if (title.empty()) {
	title = "Untitled";
    }
    if (title.size() > kMaxTitleLen) {
	title.resize(kMaxTitleLen);
    }

    StickyPanel& panel = gui.panels[gui.focused];
    set_title(panel.session.note, title);
    if (!panel.session.note.note_path.empty()) {
	sticky_note to_save = panel.session.note;
	to_save.text = textbox_storage_lines(panel.session);
	save_note(to_save);
    }
    cancel_title_edit(gui);
}

void insert_buffer_char(std::string& buffer, std::size_t& cursor, char ch, std::size_t max_len) {
    if (buffer.size() >= max_len) {
	return;
    }
    buffer.insert(cursor, 1, ch);
    ++cursor;
}

void backspace_buffer(std::string& buffer, std::size_t& cursor) {
    if (cursor == 0) {
	return;
    }
    buffer.erase(cursor - 1, 1);
    --cursor;
}

bool handle_text_buffer_keys(const SDL_Event& event, std::string& buffer, std::size_t& cursor,
			     std::size_t max_len, bool& cancel_flag, bool& commit_flag) {
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	switch (event.key.scancode) {
	case SDL_SCANCODE_ESCAPE:
	    cancel_flag = true;
	    return true;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
	    commit_flag = true;
	    return true;
	case SDL_SCANCODE_BACKSPACE:
	    backspace_buffer(buffer, cursor);
	    return true;
	case SDL_SCANCODE_LEFT:
	    if (cursor > 0) {
		--cursor;
	    }
	    return true;
	case SDL_SCANCODE_RIGHT:
	    if (cursor < buffer.size()) {
		++cursor;
	    }
	    return true;
	case SDL_SCANCODE_HOME:
	    cursor = 0;
	    return true;
	case SDL_SCANCODE_END:
	    cursor = buffer.size();
	    return true;
	default:
	    break;
	}
    }
    if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text != nullptr) {
	for (const char* p = event.text.text; *p != '\0'; ++p) {
	    const unsigned char byte = static_cast<unsigned char>(*p);
	    if (byte >= 32 && byte <= 126) {
		insert_buffer_char(buffer, cursor, static_cast<char>(byte), max_len);
	    }
	}
	return true;
    }
    return false;
}

bool handle_title_keys(StickyGui& gui, const SDL_Event& event) {
    if (!gui.editing_title) {
	return false;
    }
    bool cancel = false;
    bool commit = false;
    if (handle_text_buffer_keys(event, gui.title_edit_buffer, gui.title_cursor, kMaxTitleLen, cancel,
				commit)) {
	if (cancel) {
	    cancel_title_edit(gui);
	} else if (commit) {
	    commit_title_edit(gui);
	}
	return true;
    }
    return true;
}

void begin_find_edit(StickyGui& gui) {
    if (gui.panels.empty()) {
	return;
    }
    gui.editing_sidebar_search = false;
    gui.editing_find = true;
    gui.find_buffer = gui.panels[gui.focused].session.find_needle;
    gui.find_cursor = gui.find_buffer.size();
    gui.find_status.clear();
}

void cancel_find_edit(StickyGui& gui) {
    gui.editing_find = false;
    gui.find_buffer.clear();
    gui.find_cursor = 0;
}

void clear_sidebar_search(StickyGui& gui) {
    gui.editing_sidebar_search = false;
    gui.sidebar_search_buffer.clear();
    gui.sidebar_search_cursor = 0;
    refresh_sidebar_entries(gui);
}

void begin_sidebar_search(StickyGui& gui) {
    cancel_find_edit(gui);
    cancel_open_picker(gui);
    cancel_theme_picker(gui);
    cancel_title_edit(gui);
    if (!gui.sidebar_visible) {
	gui.sidebar_visible = true;
	if (!gui.panels.empty()) {
	    place_panel_on_desk(gui, gui.panels[gui.focused]);
	}
    }
    gui.editing_sidebar_search = true;
    gui.sidebar_search_cursor = gui.sidebar_search_buffer.size();
}

bool handle_sidebar_search_keys(StickyGui& gui, const SDL_Event& event) {
    if (!gui.editing_sidebar_search) {
	return false;
    }
    bool cancel = false;
    bool commit = false;
    const std::string before = gui.sidebar_search_buffer;
    if (handle_text_buffer_keys(event, gui.sidebar_search_buffer, gui.sidebar_search_cursor,
				kMaxSidebarSearchLen, cancel, commit)) {
	if (cancel) {
	    clear_sidebar_search(gui);
	} else if (commit) {
	    gui.editing_sidebar_search = false;
	} else if (gui.sidebar_search_buffer != before) {
	    refresh_sidebar_entries(gui);
	}
	return true;
    }
    return true;
}

void run_find(StickyGui& gui) {
    if (gui.panels.empty() || gui.find_buffer.empty()) {
	gui.find_status = "Empty needle";
	return;
    }
    EditorSession& session = gui.panels[gui.focused].session;
    if (find_text(session, gui.find_buffer)) {
	gui.find_status = "Match found";
    } else {
	gui.find_status = "Not found";
    }
}

void run_find_next(StickyGui& gui) {
    if (gui.panels.empty()) {
	return;
    }
    EditorSession& session = gui.panels[gui.focused].session;
    if (find_next(session)) {
	gui.find_status = "Next match";
    } else {
	gui.find_status = "No more matches";
    }
}

bool handle_find_keys(StickyGui& gui, const SDL_Event& event) {
    if (!gui.editing_find) {
	return false;
    }
    bool cancel = false;
    bool commit = false;
    if (handle_text_buffer_keys(event, gui.find_buffer, gui.find_cursor, kMaxFindLen, cancel, commit)) {
	if (cancel) {
	    cancel_find_edit(gui);
	} else if (commit) {
	    run_find(gui);
	}
	return true;
    }
    return true;
}

bool handle_open_picker_keys(StickyGui& gui, const SDL_Event& event) {
    if (!gui.show_open_picker) {
	return false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	switch (event.key.scancode) {
	case SDL_SCANCODE_ESCAPE:
	    cancel_open_picker(gui);
	    return true;
	case SDL_SCANCODE_UP:
	    if (gui.open_picker_cursor > 0) {
		--gui.open_picker_cursor;
	    }
	    return true;
	case SDL_SCANCODE_DOWN:
	    if (!gui.open_picker_entries.empty()
		&& gui.open_picker_cursor + 1 < gui.open_picker_entries.size()) {
		++gui.open_picker_cursor;
	    }
	    return true;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
	    open_picker_selection(gui);
	    return true;
	default:
	    break;
	}
    }
    return true;
}

bool handle_delete_confirm(StickyGui& gui, const SDL_Event& event) {
    if (!gui.pending_delete) {
	return false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
	    gui.pending_delete = false;
	    return true;
	}
	if (event.key.scancode == SDL_SCANCODE_Y) {
	    StickyPanel& panel = gui.panels[gui.pending_delete_panel];
	    if (!panel.session.note.note_path.empty()) {
		delete_note_file(panel.session.note.note_path);
	    }
	    const std::size_t idx = gui.pending_delete_panel;
	    gui.pending_delete = false;
	    close_panel_at(gui, idx, false);
	    return true;
	}
	if (event.key.scancode == SDL_SCANCODE_N) {
	    gui.pending_delete = false;
	    return true;
	}
    }

    return true;
}

void set_theme(StickyGui& gui, GuiThemeId id) {
    gui.theme_id = id;
    gui_theme_save_persisted(id);
}

void theme_badge_rect(const StickyGui& gui, float& out_x, float& out_y, float& out_w, float& out_h) {
    if (!gui.sidebar_visible) {
	out_x = 0.0f;
	out_y = 0.0f;
	out_w = 0.0f;
	out_h = 0.0f;
	return;
    }
    out_x = kSidebarPad;
    out_y = kDefaultDeskH - kThemeBadgeH - kSidebarPad;
    out_w = kSidebarWidth - 2.0f * kSidebarPad;
    out_h = kThemeBadgeH;
}

void theme_dropdown_rect(const StickyGui& gui, float& out_x, float& out_y, float& out_w, float& out_h) {
    float badge_y = 0.0f;
    float badge_h = 0.0f;
    theme_badge_rect(gui, out_x, badge_y, out_w, badge_h);
    if (out_w <= 0.0f) {
	out_y = 0.0f;
	out_h = 0.0f;
	return;
    }
    // Opens upward from the badge; must match render_theme_picker.
    const float drop_h = kLineH + 4.0f + static_cast<float>(kThemePickerCount) * (kLineH + 4.0f)
			 + 8.0f;
    out_y = badge_y - drop_h;
    out_h = drop_h;
}

int theme_picker_row_at_point(const StickyGui& gui, float mx, float my) {
    float drop_x = 0.0f;
    float drop_y = 0.0f;
    float drop_w = 0.0f;
    float drop_h = 0.0f;
    theme_dropdown_rect(gui, drop_x, drop_y, drop_w, drop_h);
    if (!point_in_rect(mx, my, drop_x, drop_y, drop_w, drop_h)) {
	return -1;
    }

    float row_y = drop_y + 4.0f + kLineH;
    for (std::size_t i = 0; i < kThemePickerCount; ++i) {
	const float row_h = kLineH + 4.0f;
	if (my >= row_y - 2.0f && my < row_y + row_h) {
	    return static_cast<int>(i);
	}
	row_y += row_h;
    }
    return -1;
}

bool theme_picker_point_in_dropdown(const StickyGui& gui, float mx, float my) {
    float drop_x = 0.0f;
    float drop_y = 0.0f;
    float drop_w = 0.0f;
    float drop_h = 0.0f;
    theme_dropdown_rect(gui, drop_x, drop_y, drop_w, drop_h);
    return point_in_rect(mx, my, drop_x, drop_y, drop_w, drop_h);
}

void cancel_theme_picker(StickyGui& gui) {
    gui.show_theme_picker = false;
}

void begin_theme_picker(StickyGui& gui) {
    cancel_title_edit(gui);
    cancel_find_edit(gui);
    cancel_open_picker(gui);
    gui.show_theme_picker = true;
    gui.theme_picker_cursor = static_cast<std::size_t>(gui.theme_id);
}

void toggle_theme_picker(StickyGui& gui) {
    if (gui.show_theme_picker) {
	cancel_theme_picker(gui);
    } else {
	begin_theme_picker(gui);
    }
}

void theme_picker_apply(StickyGui& gui) {
    if (gui.theme_picker_cursor >= kThemePickerCount) {
	cancel_theme_picker(gui);
	return;
    }
    set_theme(gui, static_cast<GuiThemeId>(gui.theme_picker_cursor));
    cancel_theme_picker(gui);
}

bool handle_theme_picker_keys(StickyGui& gui, const SDL_Event& event) {
    if (!gui.show_theme_picker) {
	return false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	switch (event.key.scancode) {
	case SDL_SCANCODE_ESCAPE:
	    cancel_theme_picker(gui);
	    return true;
	case SDL_SCANCODE_UP:
	    if (gui.theme_picker_cursor > 0) {
		--gui.theme_picker_cursor;
	    }
	    return true;
	case SDL_SCANCODE_DOWN:
	    if (gui.theme_picker_cursor + 1 < kThemePickerCount) {
		++gui.theme_picker_cursor;
	    }
	    return true;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
	    theme_picker_apply(gui);
	    return true;
	default:
	    break;
	}
    }
    return false;
}

bool handle_theme_picker_mouse(StickyGui& gui, const SDL_Event& event) {
    if (!gui.sidebar_visible) {
	if (gui.show_theme_picker) {
	    cancel_theme_picker(gui);
	}
	return false;
    }

    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP
	&& event.type != SDL_EVENT_MOUSE_MOTION) {
	return false;
    }
    if (event.type != SDL_EVENT_MOUSE_MOTION && event.button.button != SDL_BUTTON_LEFT) {
	return gui.show_theme_picker;
    }

    const float mx = event.type == SDL_EVENT_MOUSE_MOTION ? event.motion.x : event.button.x;
    const float my = event.type == SDL_EVENT_MOUSE_MOTION ? event.motion.y : event.button.y;

    float badge_x = 0.0f;
    float badge_y = 0.0f;
    float badge_w = 0.0f;
    float badge_h = 0.0f;
    theme_badge_rect(gui, badge_x, badge_y, badge_w, badge_h);
    const bool on_badge = point_in_rect(mx, my, badge_x, badge_y, badge_w, badge_h);

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	if (!gui.show_theme_picker) {
	    return false;
	}
	const int row = theme_picker_row_at_point(gui, mx, my);
	if (row >= 0) {
	    gui.theme_picker_cursor = static_cast<std::size_t>(row);
	}
	return true;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
	if (on_badge) {
	    return gui.show_theme_picker;
	}
	if (!gui.show_theme_picker) {
	    return false;
	}
	if (theme_picker_point_in_dropdown(gui, mx, my)) {
	    const int row = theme_picker_row_at_point(gui, mx, my);
	    if (row >= 0) {
		gui.theme_picker_cursor = static_cast<std::size_t>(row);
	    }
	    return true;
	}
	cancel_theme_picker(gui);
	return false;
    }

    // Mouse up — primary click action.
    if (on_badge) {
	toggle_theme_picker(gui);
	return true;
    }

    if (gui.show_theme_picker) {
	const int row = theme_picker_row_at_point(gui, mx, my);
	if (row >= 0) {
	    gui.theme_picker_cursor = static_cast<std::size_t>(row);
	    theme_picker_apply(gui);
	    return true;
	}
	if (theme_picker_point_in_dropdown(gui, mx, my)) {
	    return true;
	}
	cancel_theme_picker(gui);
	return true;
    }

    return false;
}

bool handle_overlay_keys(StickyGui& gui, const SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
	return false;
    }

    if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
	if (gui.show_help) {
	    gui.show_help = false;
	    return true;
	}
	if (gui.editing_find) {
	    cancel_find_edit(gui);
	    return true;
	}
	if (gui.editing_sidebar_search || !gui.sidebar_search_buffer.empty()) {
	    clear_sidebar_search(gui);
	    return true;
	}
	if (gui.show_open_picker) {
	    cancel_open_picker(gui);
	    return true;
	}
	if (gui.show_theme_picker) {
	    cancel_theme_picker(gui);
	    return true;
	}
    }

    if (event.key.scancode == SDL_SCANCODE_F3) {
	run_find_next(gui);
	return true;
    }

    if (event.key.scancode == SDL_SCANCODE_H || event.key.scancode == SDL_SCANCODE_F1) {
	gui.show_help = !gui.show_help;
	return true;
    }

    if (gui.show_help) {
	return true;
    }

    if (event.key.scancode == SDL_SCANCODE_F2) {
	begin_title_edit(gui);
	return true;
    }

    if (ctrl_down(event.key)) {
	switch (event.key.scancode) {
	case SDL_SCANCODE_O:
	    begin_open_picker(gui);
	    return true;
	case SDL_SCANCODE_F:
	    begin_find_edit(gui);
	    return true;
	case SDL_SCANCODE_K:
	    begin_sidebar_search(gui);
	    return true;
	case SDL_SCANCODE_Z:
	    if (!gui.panels.empty()) {
		if (shift_down(event.key)) {
		    editor_redo(gui.panels[gui.focused].session);
		} else {
		    editor_undo(gui.panels[gui.focused].session);
		}
	    }
	    return true;
	case SDL_SCANCODE_Y:
	    if (!gui.panels.empty()) {
		editor_redo(gui.panels[gui.focused].session);
	    }
	    return true;
	case SDL_SCANCODE_T:
	    set_theme(gui, gui_theme_cycle(gui.theme_id));
	    return true;
	case SDL_SCANCODE_1:
	    set_theme(gui, GuiThemeId::Minimal);
	    return true;
	case SDL_SCANCODE_2:
	    set_theme(gui, GuiThemeId::Retro);
	    return true;
	case SDL_SCANCODE_3:
	    set_theme(gui, GuiThemeId::Cyberpunk);
	    return true;
	case SDL_SCANCODE_N:
	    {
		StickyPanel panel = make_blank_panel(0.0f, 0.0f);
		textbox_init_session(panel.session);
		set_desk_panel(gui, std::move(panel));
	    }
	    return true;
	case SDL_SCANCODE_W:
	    if (shift_down(event.key)) {
		if (!gui.panels.empty()) {
		    gui.pending_delete = true;
		    gui.pending_delete_panel = gui.focused;
		}
		return true;
	    }
	    close_panel_at(gui, gui.focused);
	    return true;
	case SDL_SCANCODE_S:
	    if (!gui.panels.empty()) {
		persist_panel_if_possible(gui.panels[gui.focused]);
		trigger_save_toast(gui);
	    }
	    return true;
	case SDL_SCANCODE_B:
	    toggle_sidebar(gui);
	    return true;
	case SDL_SCANCODE_P:
	    if (shift_down(event.key) && !gui.panels.empty()) {
		request_pop_out(gui, gui.focused);
		return true;
	    }
	    break;
	default:
	    break;
	}
    }

    return false;
}

bool handle_mouse(StickyGui& gui, const SDL_Event& event, SDL_Window* desk_window) {
    if (gui.pending_delete || gui.show_help || gui.show_open_picker || gui.editing_find) {
	return false;
    }

    if (handle_theme_picker_mouse(gui, event)) {
	return true;
    }

    if (gui.show_theme_picker) {
	return true;
    }

    if (handle_sidebar_mouse(gui, event, desk_window)) {
	return true;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	const float mx = event.motion.x;
	const float my = event.motion.y;
	if (gui.interaction != StickyGui::Interaction::None) {
	    StickyPanel& panel = gui.panels[gui.active_panel];
	    if (gui.interaction == StickyGui::Interaction::Drag) {
		panel.x = mx - gui.drag_offset_x;
		panel.y = my - gui.drag_offset_y;
		return true;
	    }
	    if (gui.interaction == StickyGui::Interaction::Resize) {
		panel.width = std::max(kMinPanelW, mx - panel.x);
		panel.height = std::max(kMinPanelH, my - panel.y);
		sticky_gui_reflow_panel(gui, gui.active_panel);
		return true;
	    }
	    return false;
	}
	if (!gui.editing_title) {
	    const int hit = panel_at_point(gui, mx, my);
	    if (hit >= 0 && static_cast<std::size_t>(hit) != gui.focused) {
		focus_panel(gui, static_cast<std::size_t>(hit));
		return true;
	    }
	}
	return false;
    }

    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP) {
	return false;
    }
    if (event.button.button != SDL_BUTTON_LEFT) {
	return false;
    }

    const float mx = event.button.x;
    const float my = event.button.y;

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
	if (gui.interaction != StickyGui::Interaction::None) {
	    if (gui.interaction == StickyGui::Interaction::Resize) {
		sticky_gui_reflow_panel(gui, gui.active_panel);
	    }
	    if (gui.interaction == StickyGui::Interaction::Drag) {
		StickyPanel& dragged = gui.panels[gui.active_panel];
		const float dx = dragged.x - gui.title_drag_start_x;
		const float dy = dragged.y - gui.title_drag_start_y;
		const float title_h = sticky_panel_title_bar_height();
		if (dx * dx + dy * dy <= 16.0f
		    && point_in_rect(mx, my, dragged.x, dragged.y, dragged.width, title_h)) {
		    const Uint32 now = SDL_GetTicks();
		    if (gui.active_panel == gui.last_title_click_panel
			&& now - gui.last_title_click_ms <= kTitleDoubleClickMs) {
			request_pop_out(gui, gui.active_panel);
			gui.last_title_click_ms = 0;
		    } else {
			gui.last_title_click_ms = now;
			gui.last_title_click_panel = gui.active_panel;
		    }
		}
	    }
	    gui.interaction = StickyGui::Interaction::None;
	    return true;
	}
	return false;
    }

    const int hit = panel_at_point(gui, mx, my);
    if (hit < 0) {
	return false;
    }

    const std::size_t index = static_cast<std::size_t>(hit);
    focus_panel(gui, index);
    StickyPanel& panel = gui.panels[gui.focused];

    float close_x = 0.0f;
    float close_y = 0.0f;
    float close_w = 0.0f;
    float close_h = 0.0f;
    close_button_rect(panel, close_x, close_y, close_w, close_h);
    if (point_in_rect(mx, my, close_x, close_y, close_w, close_h)) {
	close_panel_at(gui, gui.focused);
	return true;
    }

    const float grip_x = panel.x + panel.width - kResizeGrip;
    const float grip_y = panel.y + panel.height - kResizeGrip;
    if (point_in_rect(mx, my, grip_x, grip_y, kResizeGrip, kResizeGrip)) {
	gui.interaction = StickyGui::Interaction::Resize;
	gui.active_panel = gui.focused;
	return true;
    }

    const float title_h = sticky_panel_title_bar_height();
    if (point_in_rect(mx, my, panel.x, panel.y, panel.width, title_h)) {
	gui.interaction = StickyGui::Interaction::Drag;
	gui.active_panel = gui.focused;
	gui.drag_offset_x = mx - panel.x;
	gui.drag_offset_y = my - panel.y;
	gui.title_drag_start_x = panel.x;
	gui.title_drag_start_y = panel.y;
	return true;
    }

    cancel_title_edit(gui);
    return true;
}

void render_overlay_bar(SDL_Renderer* renderer, float x, float y, float w, float h,
			const StickyGuiTheme& theme) {
    SDL_FRect bar{x, y, w, h};
    gui_set_render_color(renderer, theme.overlay_bg);
    SDL_RenderFillRect(renderer, &bar);
    gui_set_render_color(renderer, theme.overlay_border);
    SDL_RenderRect(renderer, &bar);
}

void render_help_overlay(SDL_Renderer* renderer, const StickyGuiTheme& theme) {
    render_overlay_bar(renderer, 16.0f, 500.0f, 928.0f, 120.0f, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    SDL_RenderDebugText(renderer, 24.0f, 508.0f,
			"Ctrl+O open  Ctrl+S save  Ctrl+N new  Ctrl+W close  Ctrl+Shift+W delete");
    SDL_RenderDebugText(renderer, 24.0f, 524.0f,
			"Ctrl+Z undo  Ctrl+Y redo  Ctrl+F find  F3 next  Ctrl+K search notes");
    SDL_RenderDebugText(renderer, 24.0f, 540.0f,
			"Ctrl+T theme  Ctrl+1/2/3 themes  Sidebar Theme badge  H help  Esc quit");
    SDL_RenderDebugText(renderer, 24.0f, 556.0f,
			"Ctrl+B sidebar  Drag row to pop out  Ctrl+Shift+P pop out  F2 rename");
    SDL_RenderDebugText(renderer, 24.0f, 572.0f,
			"Popup: v dock  Dbl-click title dock  Enter newline");
}

void render_delete_confirm(SDL_Renderer* renderer, const StickyGuiTheme& theme) {
    render_overlay_bar(renderer, 200.0f, 280.0f, 560.0f, 48.0f, theme);
    gui_set_render_color(renderer, theme.status_err);
    SDL_RenderDebugText(renderer, 216.0f, 292.0f, "Delete note file from disk?  Y / N   (Esc cancel)");
}

void render_open_picker(SDL_Renderer* renderer, const StickyGui& gui, const StickyGuiTheme& theme) {
    render_overlay_bar(renderer, 120.0f, 80.0f, 720.0f, 400.0f, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    SDL_RenderDebugText(renderer, 136.0f, 88.0f, "Open note  (Up/Down  Enter  Esc cancel)");
    gui_set_render_color(renderer, theme.overlay_muted);
    SDL_RenderDebugText(renderer, 136.0f, 104.0f, "Opens existing file or focuses panel if already open");

    float row_y = 128.0f;
    for (std::size_t i = 0; i < gui.open_picker_entries.size() && i < 14; ++i) {
	const OpenPickerEntry& entry = gui.open_picker_entries[i];
	const bool selected = (i == gui.open_picker_cursor);
	if (selected) {
	    SDL_FRect row{128.0f, row_y - 2.0f, 704.0f, kLineH + 4.0f};
	    gui_set_render_color(renderer, theme.panel_border_focus);
	    SDL_RenderFillRect(renderer, &row);
	}
	gui_set_render_color(renderer, selected ? theme.desk : theme.overlay_text);
	std::string line = std::to_string(i + 1) + ". " + entry.title;
	SDL_RenderDebugText(renderer, 136.0f, row_y, line.c_str());
	row_y += kLineH + 4.0f;
    }
    if (gui.open_picker_entries.empty()) {
	gui_set_render_color(renderer, theme.overlay_muted);
	SDL_RenderDebugText(renderer, 136.0f, 128.0f, "(no saved notes in notes/)");
    }
}

void render_find_bar(SDL_Renderer* renderer, const StickyGui& gui, const StickyGuiTheme& theme) {
    render_overlay_bar(renderer, 120.0f, 16.0f, 720.0f, 56.0f, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    SDL_RenderDebugText(renderer, 136.0f, 24.0f, "Find:");
    const std::string shown = gui.find_buffer + "|";
    SDL_RenderDebugText(renderer, 200.0f, 24.0f, shown.c_str());
    if (!gui.find_status.empty()) {
	const GuiColor& color = gui.find_status == "Not found" || gui.find_status == "No more matches"
	    ? theme.status_err
	    : theme.status_ok;
	gui_set_render_color(renderer, color);
	SDL_RenderDebugText(renderer, 136.0f, 44.0f, gui.find_status.c_str());
    }
}

void render_save_toast(SDL_Renderer* renderer, const StickyGui& gui, const StickyGuiTheme& theme) {
    if (SDL_GetTicks() >= gui.save_toast_until_ms) {
	return;
    }
    render_overlay_bar(renderer, 760.0f, 16.0f, 184.0f, 32.0f, theme);
    gui_set_render_color(renderer, theme.status_ok);
    SDL_RenderDebugText(renderer, 776.0f, 24.0f, "Saved");
}

void render_theme_badge(SDL_Renderer* renderer, const StickyGui& gui, const StickyGuiTheme& theme) {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    theme_badge_rect(gui, x, y, w, h);
    render_overlay_bar(renderer, x, y, w, h, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    std::string label = std::string("Theme: ") + theme.name;
    SDL_RenderDebugText(renderer, x + 8.0f, y + 10.0f, label.c_str());
    SDL_RenderDebugText(renderer, x + w - 16.0f, y + 10.0f, gui.show_theme_picker ? "^" : "v");
}

void render_theme_picker(SDL_Renderer* renderer, const StickyGui& gui, const StickyGuiTheme& theme) {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    theme_dropdown_rect(gui, x, y, w, h);
    render_overlay_bar(renderer, x, y, w, h, theme);
    gui_set_render_color(renderer, theme.overlay_muted);
    SDL_RenderDebugText(renderer, x + 8.0f, y + 2.0f, "Choose theme");

    float row_y = y + 4.0f + kLineH;
    for (std::size_t i = 0; i < kThemePickerCount; ++i) {
	const bool selected = (i == gui.theme_picker_cursor);
	const bool active = (static_cast<GuiThemeId>(i) == gui.theme_id);
	if (selected) {
	    SDL_FRect row{x + 4.0f, row_y - 2.0f, w - 8.0f, kLineH + 4.0f};
	    gui_set_render_color(renderer, theme.panel_border_focus);
	    SDL_RenderFillRect(renderer, &row);
	}
	gui_set_render_color(renderer, selected ? theme.desk : theme.overlay_text);
	const char* name = gui_theme_name(static_cast<GuiThemeId>(i));
	std::string line = std::string(active ? "* " : "  ") + name;
	SDL_RenderDebugText(renderer, x + 12.0f, row_y, line.c_str());
	row_y += kLineH + 4.0f;
    }
}
} // namespace

void sticky_gui_init(StickyGui& gui) {
    gui = StickyGui{};
    ensure_notes_data_dir();
    gui.theme_id = gui_theme_load_persisted();

    bool sidebar_open = true;
    std::string desk_path;
    if (desk_state_load(sidebar_open, desk_path)) {
	gui.sidebar_visible = sidebar_open;
    }

    if (desk_path.empty()) {
	desk_path = find_most_recently_edited_note_path();
    }

    if (!desk_path.empty() && load_desk_panel_from_path(gui, desk_path)) {
	return;
    }

    StickyPanel panel = make_blank_panel(0.0f, 0.0f);
    set_desk_panel(gui, std::move(panel));
}

void sticky_gui_save_all(const StickyGui& gui) {
    for (const StickyPanel& panel : gui.panels) {
	if (!panel.session.note.note_path.empty()) {
	    sticky_note to_save = panel.session.note;
	    to_save.text = textbox_storage_lines(panel.session);
	    save_note(to_save);
	}
    }
    desk_state_save(gui);
}

StickyGuiTheme sticky_gui_active_theme(const StickyGui& gui) {
    return gui_theme_get(gui.theme_id);
}

GuiColor sticky_gui_desk_color(const StickyGui& gui) {
    return sticky_gui_active_theme(gui).desk;
}

bool sticky_gui_modal_blocks_body(const StickyGui& gui) {
    return gui.editing_title || gui.show_help || gui.pending_delete || gui.show_open_picker
	|| gui.show_theme_picker || gui.editing_find || gui.editing_sidebar_search;
}

void sticky_gui_reflow_panel(StickyGui& gui, std::size_t panel_index) {
    if (panel_index >= gui.panels.size()) {
	return;
    }
    StickyPanel& panel = gui.panels[panel_index];
    textbox_enforce_wrap(panel.session, textbox_body_max_columns(panel.width));
    textbox_pin_viewport_to_start(panel.viewport);
}

void sticky_gui_render(SDL_Renderer* renderer, StickyGui& gui) {
    const StickyGuiTheme theme = sticky_gui_active_theme(gui);

    render_sidebar(renderer, gui, theme);

    for (std::size_t i = 0; i < gui.panels.size(); ++i) {
	const bool focused = (i == gui.focused);
	StickyPanel& panel = gui.panels[i];

	PanelChrome chrome{};
	if (focused && gui.editing_title) {
	    chrome.title_text = gui.title_edit_buffer.c_str();
	    chrome.title_caret_visible = true;
	    chrome.title_caret_col = gui.title_cursor;
	}
	if (focused) {
	    chrome.show_close_button = true;
	}

	textbox_render_panel(renderer, panel.x, panel.y, panel.width, panel.height, panel.session,
			     panel.viewport, focused, chrome, theme);
    }

    render_save_toast(renderer, gui, theme);

    if (gui.editing_find) {
	render_find_bar(renderer, gui, theme);
    }
    if (gui.show_open_picker) {
	render_open_picker(renderer, gui, theme);
    }
    if (gui.show_help) {
	render_help_overlay(renderer, theme);
    }
    if (gui.pending_delete) {
	render_delete_confirm(renderer, theme);
    }
}

bool sticky_gui_handle_event(StickyGui& gui, const SDL_Event& event, bool& quit_requested,
			     SDL_WindowID desk_window_id) {
    SDL_Window* desk_window = nullptr;
    if (desk_window_id != 0) {
	desk_window = SDL_GetWindowFromID(desk_window_id);
    }
    if (event.type == SDL_EVENT_QUIT) {
	quit_requested = true;
	return true;
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
	if (desk_window_id == 0 || event.window.windowID == desk_window_id) {
	    quit_requested = true;
	}
	return true;
    }

    if (desk_window_id != 0 && event.type == SDL_EVENT_MOUSE_MOTION) {
	if (event.motion.windowID != desk_window_id) {
	    return false;
	}
    } else if (desk_window_id != 0
	       && (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
		   || event.type == SDL_EVENT_MOUSE_BUTTON_UP)) {
	if (event.button.windowID != desk_window_id) {
	    return false;
	}
    } else if (desk_window_id != 0 && event.type == SDL_EVENT_TEXT_INPUT) {
	if (event.text.windowID != desk_window_id) {
	    return false;
	}
    } else if (desk_window_id != 0
	       && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)) {
	if (event.key.windowID != desk_window_id) {
	    return false;
	}
    }

    if (handle_delete_confirm(gui, event)) {
	return true;
    }

    if (handle_open_picker_keys(gui, event)) {
	return true;
    }

    if (handle_theme_picker_keys(gui, event)) {
	return true;
    }

    if (handle_find_keys(gui, event)) {
	return true;
    }

    if (handle_sidebar_search_keys(gui, event)) {
	return true;
    }

    if (handle_title_keys(gui, event)) {
	return true;
    }

    if (handle_overlay_keys(gui, event)) {
	return true;
    }

    if (handle_mouse(gui, event, desk_window)) {
	return true;
    }

    if (gui.interaction != StickyGui::Interaction::None) {
	return false;
    }

    if (sticky_gui_modal_blocks_body(gui)) {
	return false;
    }

    if (gui.panels.empty()) {
	return false;
    }

    StickyPanel& panel = gui.panels[gui.focused];
    const bool handled = textbox_handle_sdl_event(panel.session, event, quit_requested,
						  textbox_body_max_columns(panel.width));
    if (handled) {
	textbox_scroll_to_cursor(panel.viewport, panel.session,
				 textbox_visible_body_lines(panel.height));
    }
    return handled;
}

void sticky_gui_reset(StickyGui& gui) {
    gui = StickyGui{};
}

void sticky_gui_add_panel_from_note(StickyGui& gui, const sticky_note& note, float x, float y) {
    add_panel(gui, make_panel_from_note(note, x, y));
}

std::size_t sticky_gui_panel_count(const StickyGui& gui) {
    return gui.panels.size();
}

std::size_t sticky_gui_focused_index(const StickyGui& gui) {
    return gui.focused;
}

const sticky_note& sticky_gui_focused_note(const StickyGui& gui) {
    return gui.panels.at(gui.focused).session.note;
}

const StickyPanel& sticky_gui_panel_at(const StickyGui& gui, std::size_t index) {
    return gui.panels.at(index);
}

void sticky_gui_panel_hit_targets(const StickyPanel& panel, StickyPanelHitTargets& out) {
    float close_w = 0.0f;
    float close_h = 0.0f;
    close_button_rect(panel, out.close_x, out.close_y, close_w, close_h);
    out.close_x += close_w * 0.5f;
    out.close_y += close_h * 0.5f;

    const float title_h = sticky_panel_title_bar_height();
    out.title_x = panel.x + panel.width * 0.5f;
    out.title_y = panel.y + title_h * 0.5f;

    out.grip_x = panel.x + panel.width - kResizeGrip * 0.5f;
    out.grip_y = panel.y + panel.height - kResizeGrip * 0.5f;
}

bool sticky_gui_consume_pop_out_request(StickyGui& gui, StickyPanel& out_panel) {
    if (!gui.pop_out_requested) {
	return false;
    }
    const std::size_t index = gui.pop_out_panel_index;
    gui.pop_out_requested = false;
    if (!extract_panel_at(gui, index, out_panel)) {
	return false;
    }
    size_panel_for_pop_out(out_panel);
    return true;
}

void sticky_gui_attach_panel(StickyGui& gui, StickyPanel panel) {
    set_desk_panel(gui, std::move(panel));
}

bool sticky_gui_consume_sidebar_pop_out_request(StickyGui& gui, StickyPanel& out_panel,
						int& out_screen_x, int& out_screen_y) {
    if (!gui.sidebar_pop_out_pending) {
	return false;
    }
    gui.sidebar_pop_out_pending = false;
    out_panel = std::move(gui.sidebar_pop_out_panel);
    size_panel_for_pop_out(out_panel);
    out_screen_x = gui.sidebar_pop_out_screen_x;
    out_screen_y = gui.sidebar_pop_out_screen_y;
    return true;
}

bool sticky_gui_sidebar_visible(const StickyGui& gui) {
    return gui.sidebar_visible;
}

std::size_t sticky_gui_sidebar_entry_count(const StickyGui& gui) {
    return gui.sidebar_entries.size();
}

bool sticky_gui_pop_out_screen_position(SDL_Window* desk_window, const StickyPanel& panel, int& out_x,
					int& out_y) {
    if (desk_window == nullptr) {
	return false;
    }
    int desk_x = 0;
    int desk_y = 0;
    SDL_GetWindowPosition(desk_window, &desk_x, &desk_y);
    out_x = desk_x + static_cast<int>(panel.x);
    out_y = desk_y + static_cast<int>(panel.y);
    return true;
}
