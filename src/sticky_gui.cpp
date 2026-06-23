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
constexpr std::size_t kMaxPanels = 8;
constexpr std::size_t kMaxTitleLen = 64;

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
    out_h = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
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

void insert_title_char(StickyGui& gui, char ch) {
    if (!gui.editing_title) {
	return;
    }
    if (gui.title_edit_buffer.size() >= kMaxTitleLen) {
	return;
    }
    gui.title_edit_buffer.insert(gui.title_cursor, 1, ch);
    ++gui.title_cursor;
}

void backspace_title(StickyGui& gui) {
    if (!gui.editing_title || gui.title_cursor == 0) {
	return;
    }
    gui.title_edit_buffer.erase(gui.title_cursor - 1, 1);
    --gui.title_cursor;
}

bool handle_title_keys(StickyGui& gui, const SDL_Event& event) {
    if (!gui.editing_title) {
	return false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	switch (event.key.scancode) {
	case SDL_SCANCODE_ESCAPE:
	    cancel_title_edit(gui);
	    return true;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
	    commit_title_edit(gui);
	    return true;
	case SDL_SCANCODE_BACKSPACE:
	    backspace_title(gui);
	    return true;
	case SDL_SCANCODE_LEFT:
	    if (gui.title_cursor > 0) {
		--gui.title_cursor;
	    }
	    return true;
	case SDL_SCANCODE_RIGHT:
	    if (gui.title_cursor < gui.title_edit_buffer.size()) {
		++gui.title_cursor;
	    }
	    return true;
	case SDL_SCANCODE_HOME:
	    gui.title_cursor = 0;
	    return true;
	case SDL_SCANCODE_END:
	    gui.title_cursor = gui.title_edit_buffer.size();
	    return true;
	default:
	    break;
	}
    }

    if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text != nullptr) {
	for (const char* p = event.text.text; *p != '\0'; ++p) {
	    const unsigned char byte = static_cast<unsigned char>(*p);
	    if (byte >= 32 && byte <= 126) {
		insert_title_char(gui, static_cast<char>(byte));
	    }
	}
	return true;
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

bool handle_overlay_keys(StickyGui& gui, const SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
	return false;
    }

    if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
	if (gui.show_help) {
	    gui.show_help = false;
	    return true;
	}
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
	    }
	    return true;
	default:
	    break;
	}
    }

    return false;
}

bool handle_mouse(StickyGui& gui, const SDL_Event& event) {
    if (gui.pending_delete || gui.show_help) {
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
    if (gui.focused == gui.panels.size() - 1
	&& point_in_rect(mx, my, close_x, close_y, close_w, close_h)) {
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

void render_help_overlay(SDL_Renderer* renderer) {
    SDL_FRect bar{16.0f, 520.0f, 928.0f, 100.0f};
    SDL_SetRenderDrawColor(renderer, 20, 20, 28, 230);
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
    SDL_RenderRect(renderer, &bar);

    SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
    SDL_RenderDebugText(renderer, 24.0f, 528.0f, "Ctrl+N new  Ctrl+W close  Ctrl+Shift+W delete file  Ctrl+S save");
    SDL_RenderDebugText(renderer, 24.0f, 544.0f, "F2 rename title  H help  Esc quit  Drag title move  Grip resize");
    SDL_RenderDebugText(renderer, 24.0f, 560.0f, "Enter newline  Arrows move  Backspace merge lines at column 0");
    SDL_RenderDebugText(renderer, 24.0f, 576.0f, "Click x on panel to close (saves first)");
}

void render_delete_confirm(SDL_Renderer* renderer) {
    SDL_FRect bar{200.0f, 280.0f, 560.0f, 48.0f};
    SDL_SetRenderDrawColor(renderer, 60, 20, 20, 240);
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, 255, 120, 120, 255);
    SDL_RenderRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, 255, 230, 230, 255);
    SDL_RenderDebugText(renderer, 216.0f, 292.0f, "Delete note file from disk?  Y / N   (Esc cancel)");
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

void sticky_gui_render(SDL_Renderer* renderer, StickyGui& gui) {
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
			     panel.viewport, focused, chrome);
    }

    if (gui.show_help) {
	render_help_overlay(renderer);
    }
    if (gui.pending_delete) {
	render_delete_confirm(renderer);
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

    if (gui.editing_title || gui.show_help || gui.pending_delete) {
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
