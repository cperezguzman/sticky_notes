#include "sticky_gui.h"

#include "note_store.h"
#include "sticky_note.h"
#include "textbox_sdl.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
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

bool point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

bool ctrl_down(const SDL_KeyboardEvent& key) {
    return (key.mod & SDL_KMOD_CTRL) != 0;
}

bool shift_down(const SDL_KeyboardEvent& key) {
    return (key.mod & SDL_KMOD_SHIFT) != 0;
}

void close_button_rect(const StickyPanel& panel, float& out_x, float& out_y, float& out_w, float& out_h) {
    out_w = sticky_panel_close_button_width();
    out_h = kLineH;
    out_x = panel.x + panel.width - out_w - kPadding * 0.5f;
    out_y = panel.y + kPadding * 0.5f;
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
    editor_reset_cursor(panel.session);
    editor_clear_history(panel.session);
    return panel;
}

StickyPanel make_blank_panel(float x, float y) {
    StickyPanel panel{};
    panel.x = x;
    panel.y = y;
    textbox_init_session(panel.session);
    return panel;
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
    if (panel.session.note.note_path.empty()) {
	const std::string title = panel.session.note.title.empty()
	    ? "Untitled"
	    : panel.session.note.title;
	sticky_note created = create_note_silent(title);
	created.text = panel.session.note.text;
	panel.session.note = created;
    }
    save_note(panel.session.note);
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
	return;
    }
    if (gui.focused >= gui.panels.size()) {
	gui.focused = gui.panels.size() - 1;
    }
}

void focus_panel(StickyGui& gui, std::size_t index);
void add_panel(StickyGui& gui, StickyPanel panel);
void cancel_title_edit(StickyGui& gui);
void cancel_find_edit(StickyGui& gui);

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
    if (gui.panels.size() >= kMaxPanels) {
	cancel_open_picker(gui);
	return;
    }
    sticky_note note{};
    if (!load_note_from_path(note, pick.path)) {
	cancel_open_picker(gui);
	return;
    }
    const float offset = static_cast<float>(gui.panels.size()) * 24.0f;
    add_panel(gui, make_panel_from_note(note, 48.0f + offset, 48.0f + offset));
    cancel_open_picker(gui);
}

void begin_title_edit(StickyGui& gui) {
    if (gui.panels.empty()) {
	return;
    }
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
	save_note(panel.session.note);
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
	if (gui.show_open_picker) {
	    cancel_open_picker(gui);
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
	    if (gui.panels.size() < kMaxPanels) {
		const float offset = static_cast<float>(gui.panels.size()) * 24.0f;
		add_panel(gui, make_panel_from_note(create_note_silent("Untitled"), 48.0f + offset,
						    48.0f + offset));
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
	default:
	    break;
	}
    }

    return false;
}

bool handle_mouse(StickyGui& gui, const SDL_Event& event) {
    if (gui.pending_delete || gui.show_help || gui.show_open_picker || gui.editing_find) {
	return false;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	if (gui.interaction == StickyGui::Interaction::None) {
	    return false;
	}
	StickyPanel& panel = gui.panels[gui.active_panel];
	const float mx = event.motion.x;
	const float my = event.motion.y;
	if (gui.interaction == StickyGui::Interaction::Drag) {
	    panel.x = mx - gui.drag_offset_x;
	    panel.y = my - gui.drag_offset_y;
	    return true;
	}
	if (gui.interaction == StickyGui::Interaction::Resize) {
	    panel.width = std::max(kMinPanelW, mx - panel.x);
	    panel.height = std::max(kMinPanelH, my - panel.y);
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

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
	if (gui.interaction != StickyGui::Interaction::None) {
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
			"Ctrl+Z undo  Ctrl+Y redo  Ctrl+F find  F3 find next  F2 rename");
    SDL_RenderDebugText(renderer, 24.0f, 540.0f,
			"Ctrl+T theme cycle  Ctrl+1/2/3 Minimal/Retro/Cyberpunk  H help  Esc quit");
    SDL_RenderDebugText(renderer, 24.0f, 556.0f,
			"Drag title move  Grip resize  Click x close (saves first)");
    SDL_RenderDebugText(renderer, 24.0f, 572.0f, "Enter newline in body  Arrows move cursor");
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
	std::string line = std::to_string(entry.id) + " : " + entry.title;
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

void render_theme_badge(SDL_Renderer* renderer, const StickyGuiTheme& theme) {
    render_overlay_bar(renderer, 16.0f, 16.0f, 200.0f, 28.0f, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    std::string label = std::string("Theme: ") + theme.name;
    SDL_RenderDebugText(renderer, 24.0f, 22.0f, label.c_str());
}
} // namespace

void sticky_gui_init(StickyGui& gui) {
    gui = StickyGui{};
    ensure_notes_data_dir();

    const NoteIndex idx = build_note_index();
    if (idx.empty()) {
	add_panel(gui, make_blank_panel(40.0f, 40.0f));
	return;
    }

    std::size_t n = 0;
    for (const auto& entry : idx) {
	if (n >= kMaxPanels) {
	    break;
	}
	sticky_note note{};
	if (!load_note_from_path(note, entry.second.second)) {
	    continue;
	}
	const float offset = static_cast<float>(n) * 28.0f;
	add_panel(gui, make_panel_from_note(note, 40.0f + offset, 40.0f + offset));
	++n;
    }

    if (gui.panels.empty()) {
	add_panel(gui, make_blank_panel(40.0f, 40.0f));
    }
}

void sticky_gui_save_all(const StickyGui& gui) {
    for (const StickyPanel& panel : gui.panels) {
	if (!panel.session.note.note_path.empty()) {
	    save_note(panel.session.note);
	}
    }
}

StickyGuiTheme sticky_gui_active_theme(const StickyGui& gui) {
    return gui_theme_get(gui.theme_id);
}

GuiColor sticky_gui_desk_color(const StickyGui& gui) {
    return sticky_gui_active_theme(gui).desk;
}

bool sticky_gui_modal_blocks_body(const StickyGui& gui) {
    return gui.editing_title || gui.show_help || gui.pending_delete || gui.show_open_picker
	|| gui.editing_find;
}

void sticky_gui_render(SDL_Renderer* renderer, StickyGui& gui) {
    const StickyGuiTheme theme = sticky_gui_active_theme(gui);

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

    render_theme_badge(renderer, theme);
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

bool sticky_gui_handle_event(StickyGui& gui, const SDL_Event& event, bool& quit_requested) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
	quit_requested = true;
	return true;
    }

    if (handle_delete_confirm(gui, event)) {
	return true;
    }

    if (handle_open_picker_keys(gui, event)) {
	return true;
    }

    if (handle_find_keys(gui, event)) {
	return true;
    }

    if (handle_title_keys(gui, event)) {
	return true;
    }

    if (handle_overlay_keys(gui, event)) {
	return true;
    }

    if (handle_mouse(gui, event)) {
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

    return textbox_handle_sdl_event(gui.panels[gui.focused].session, event, quit_requested);
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
