#include "sticky_gui.h"

#include "note_store.h"
#include "textbox_sdl.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

namespace {
constexpr float kMinPanelW = 160.0f;
constexpr float kMinPanelH = 120.0f;
constexpr float kResizeGrip = 10.0f;
constexpr std::size_t kMaxPanels = 8;

bool point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

bool ctrl_down(const SDL_KeyboardEvent& key) {
    return (key.mod & SDL_KMOD_CTRL) != 0;
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

int panel_at_point(StickyGui& gui, float px, float py) {
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
    if (index != gui.focused) {
	gui.focused = index;
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

void close_focused_panel(StickyGui& gui) {
    if (gui.panels.empty()) {
	return;
    }
    const StickyPanel& panel = gui.panels[gui.focused];
    if (!panel.session.note.note_path.empty()) {
	save_note(panel.session.note);
    }
    gui.panels.erase(gui.panels.begin() + static_cast<std::ptrdiff_t>(gui.focused));
    if (gui.panels.empty()) {
	gui.focused = 0;
	return;
    }
    if (gui.focused >= gui.panels.size()) {
	gui.focused = gui.panels.size() - 1;
    }
}

bool handle_shortcuts(StickyGui& gui, const SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
	return false;
    }
    if (!ctrl_down(event.key)) {
	return false;
    }

    switch (event.key.scancode) {
    case SDL_SCANCODE_N:
	if (gui.panels.size() < kMaxPanels) {
	    const float offset = static_cast<float>(gui.panels.size()) * 24.0f;
	    add_panel(gui, make_panel_from_note(create_note_silent("Untitled"), 48.0f + offset,
						48.0f + offset));
	}
	return true;
    case SDL_SCANCODE_W:
	close_focused_panel(gui);
	return true;
    case SDL_SCANCODE_S:
	if (!gui.panels.empty()) {
	    StickyPanel& panel = gui.panels[gui.focused];
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
	return true;
    default:
	return false;
    }
}

bool handle_mouse(StickyGui& gui, const SDL_Event& event) {
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

    return true;
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
	textbox_render_panel(renderer, panel.x, panel.y, panel.width, panel.height, panel.session,
			     panel.viewport, focused);
    }
}

bool sticky_gui_handle_event(StickyGui& gui, const SDL_Event& event, bool& quit_requested) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
	quit_requested = true;
	return true;
    }

    if (handle_mouse(gui, event)) {
	return true;
    }

    if (gui.interaction != StickyGui::Interaction::None) {
	return false;
    }

    if (handle_shortcuts(gui, event)) {
	return true;
    }

    if (gui.panels.empty()) {
	return false;
    }

    return textbox_handle_sdl_event(gui.panels[gui.focused].session, event, quit_requested);
}
