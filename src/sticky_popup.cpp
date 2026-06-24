#include "sticky_popup.h"

#include "note_store.h"
#include "textbox_sdl.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr float kMinPanelW = 160.0f;
constexpr float kMinPanelH = 120.0f;
constexpr float kResizeGrip = 10.0f;
constexpr float kPadding = 8.0f;
constexpr Uint32 kDoubleClickMs = 400;

bool point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

void close_button_rect(float panel_w, float& out_x, float& out_y, float& out_w, float& out_h) {
    sticky_panel_close_button_rect(0.0f, 0.0f, panel_w, out_x, out_y, out_w, out_h);
}

void dock_button_rect(float panel_w, float& out_x, float& out_y, float& out_w, float& out_h) {
    sticky_panel_dock_button_rect(0.0f, 0.0f, panel_w, true, out_x, out_y, out_w, out_h);
}

void persist_panel(StickyPanel& panel) {
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

void sync_panel_size_from_window(StickyPopupEntry& entry) {
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(entry.window, &w, &h);
    entry.panel.width = static_cast<float>(std::max(w, static_cast<int>(kMinPanelW)));
    entry.panel.height = static_cast<float>(std::max(h, static_cast<int>(kMinPanelH)));
}

void destroy_entry(StickyPopupEntry& entry) {
    if (entry.window != nullptr) {
	SDL_DestroyRenderer(entry.renderer);
	SDL_DestroyWindow(entry.window);
	entry.window = nullptr;
	entry.renderer = nullptr;
    }
}

StickyPopupEntry* find_entry(StickyPopupManager& mgr, SDL_WindowID window_id) {
    for (StickyPopupEntry& entry : mgr.entries) {
	if (entry.window_id == window_id) {
	    return &entry;
	}
    }
    return nullptr;
}

void request_dock(StickyPopupManager& mgr, std::size_t index) {
    if (index < mgr.entries.size()) {
	mgr.dock_request_index = index;
    }
}

void close_popup_at(StickyPopupManager& mgr, std::size_t index) {
    if (index >= mgr.entries.size()) {
	return;
    }
    persist_panel(mgr.entries[index].panel);
    destroy_entry(mgr.entries[index]);
    mgr.entries.erase(mgr.entries.begin() + static_cast<std::ptrdiff_t>(index));
}

bool handle_title_double_click(StickyPopupEntry& entry, Uint32 now) {
    if (entry.last_title_click_ms != 0 && now - entry.last_title_click_ms <= kDoubleClickMs) {
	entry.last_title_click_ms = 0;
	return true;
    }
    entry.last_title_click_ms = now;
    return false;
}

bool handle_popup_mouse(StickyPopupManager& mgr, std::size_t index, const SDL_Event& event) {
    if (index >= mgr.entries.size()) {
	return false;
    }

    StickyPopupEntry& entry = mgr.entries[index];
    StickyPanel& panel = entry.panel;
    const float panel_w = panel.width;
    const float panel_h = panel.height;
    const float title_h = sticky_panel_title_bar_height();

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	const float mx = event.motion.x;
	const float my = event.motion.y;
	(void)mx;
	(void)my;
	if (entry.dragging) {
	    float gx = 0.0f;
	    float gy = 0.0f;
	    SDL_GetGlobalMouseState(&gx, &gy);
	    SDL_SetWindowPosition(
		entry.window, static_cast<int>(std::lround(gx - entry.drag_grab_offset_x)),
		static_cast<int>(std::lround(gy - entry.drag_grab_offset_y)));
	    return true;
	}
	if (entry.resizing) {
	    const float new_w = std::max(kMinPanelW, event.motion.x);
	    const float new_h = std::max(kMinPanelH, event.motion.y);
	    SDL_SetWindowSize(entry.window, static_cast<int>(new_w), static_cast<int>(new_h));
	    panel.width = new_w;
	    panel.height = new_h;
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
	if (entry.dragging || entry.resizing) {
	    if (entry.dragging) {
		float gx = 0.0f;
		float gy = 0.0f;
		SDL_GetGlobalMouseState(&gx, &gy);
		const float dx = gx - entry.drag_global_start_x;
		const float dy = gy - entry.drag_global_start_y;
		if (dx * dx + dy * dy <= 16.0f && point_in_rect(mx, my, 0.0f, 0.0f, panel_w, title_h)
		    && handle_title_double_click(entry, SDL_GetTicks())) {
		    request_dock(mgr, index);
		}
	    }
	    entry.dragging = false;
	    entry.resizing = false;
	    return true;
	}
	return false;
    }

    float close_x = 0.0f;
    float close_y = 0.0f;
    float close_w = 0.0f;
    float close_h = 0.0f;
    close_button_rect(panel_w, close_x, close_y, close_w, close_h);
    if (point_in_rect(mx, my, close_x, close_y, close_w, close_h)) {
	close_popup_at(mgr, index);
	return true;
    }

    float dock_x = 0.0f;
    float dock_y = 0.0f;
    float dock_w = 0.0f;
    float dock_h = 0.0f;
    dock_button_rect(panel_w, dock_x, dock_y, dock_w, dock_h);
    if (point_in_rect(mx, my, dock_x, dock_y, dock_w, dock_h)) {
	request_dock(mgr, index);
	return true;
    }

    const float grip_x = panel_w - kResizeGrip;
    const float grip_y = panel_h - kResizeGrip;
    if (point_in_rect(mx, my, grip_x, grip_y, kResizeGrip, kResizeGrip)) {
	entry.resizing = true;
	return true;
    }

    if (point_in_rect(mx, my, 0.0f, 0.0f, panel_w, title_h)) {
	entry.dragging = true;
	float gx = 0.0f;
	float gy = 0.0f;
	SDL_GetGlobalMouseState(&gx, &gy);
	int window_x = 0;
	int window_y = 0;
	SDL_GetWindowPosition(entry.window, &window_x, &window_y);
	entry.drag_grab_offset_x = gx - static_cast<float>(window_x);
	entry.drag_grab_offset_y = gy - static_cast<float>(window_y);
	entry.drag_global_start_x = gx;
	entry.drag_global_start_y = gy;
	return true;
    }

    return true;
}

bool handle_popup_keyboard(StickyPopupManager& mgr, std::size_t index, const SDL_Event& event) {
    if (index >= mgr.entries.size()) {
	return false;
    }
    StickyPopupEntry& entry = mgr.entries[index];
    bool quit = false;
    return textbox_handle_sdl_event(entry.panel.session, event, quit,
				    textbox_body_max_columns(entry.panel.width), false);
}
} // namespace

bool sticky_popup_open(StickyPopupManager& mgr, StickyPanel panel, GuiThemeId theme, int screen_x,
		       int screen_y) {
    panel.x = 0.0f;
    panel.y = 0.0f;

    const std::string title = panel.session.note.title.empty() ? "Untitled" : panel.session.note.title;
    const int w = static_cast<int>(std::max(panel.width, kMinPanelW));
    const int h = static_cast<int>(std::max(panel.height, kMinPanelH));

    SDL_Window* window = SDL_CreateWindow(
	title.c_str(), w, h,
	SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS);
    if (window == nullptr) {
	return false;
    }

    SDL_SetWindowPosition(window, screen_x, screen_y);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
	SDL_DestroyWindow(window);
	return false;
    }
    SDL_SetRenderVSync(renderer, 1);

    StickyPopupEntry entry{};
    entry.window = window;
    entry.renderer = renderer;
    entry.window_id = SDL_GetWindowID(window);
    entry.panel = std::move(panel);
    entry.panel.width = static_cast<float>(w);
    entry.panel.height = static_cast<float>(h);
    mgr.theme_id = theme;
    mgr.entries.push_back(std::move(entry));
    SDL_StartTextInput(window);
    return true;
}

void sticky_popup_close_all(StickyPopupManager& mgr) {
    for (StickyPopupEntry& entry : mgr.entries) {
	persist_panel(entry.panel);
	destroy_entry(entry);
    }
    mgr.entries.clear();
}

void sticky_popup_save_all(const StickyPopupManager& mgr) {
    for (const StickyPopupEntry& entry : mgr.entries) {
	StickyPanel panel = entry.panel;
	persist_panel(panel);
    }
}

void sticky_popup_render_all(StickyPopupManager& mgr) {
    const StickyGuiTheme theme = gui_theme_get(mgr.theme_id);
    for (StickyPopupEntry& entry : mgr.entries) {
	// Repainting while SDL_SetWindowPosition runs leaves compositor trails on
	// borderless windows; the last frame moves with the window until drag ends.
	if (entry.dragging) {
	    continue;
	}

	SDL_SetRenderDrawColor(entry.renderer, theme.desk.r, theme.desk.g, theme.desk.b, theme.desk.a);
	SDL_RenderClear(entry.renderer);

	PanelChrome chrome{};
	chrome.show_close_button = true;
	chrome.show_dock_button = true;

	textbox_render_panel(entry.renderer, 0.0f, 0.0f, entry.panel.width, entry.panel.height,
			     entry.panel.session, entry.panel.viewport, true, chrome, theme);
	SDL_RenderPresent(entry.renderer);
    }
}

bool sticky_popup_handle_event(StickyPopupManager& mgr, StickyGui& desk, const SDL_Event& event,
			       SDL_WindowID desk_window_id, bool& quit_app) {
    (void)desk;
    (void)desk_window_id;
    (void)quit_app;

    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
	SDL_Window* window = SDL_GetWindowFromID(event.window.windowID);
	if (window != nullptr) {
	    SDL_StartTextInput(window);
	}
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
	StickyPopupEntry* entry = find_entry(mgr, event.window.windowID);
	if (entry != nullptr) {
	    sync_panel_size_from_window(*entry);
	    return true;
	}
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
	for (std::size_t i = 0; i < mgr.entries.size(); ++i) {
	    if (mgr.entries[i].window_id == event.window.windowID) {
		close_popup_at(mgr, i);
		return true;
	    }
	}
    }

    SDL_WindowID target_id = 0;
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	target_id = event.motion.windowID;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
	target_id = event.button.windowID;
    } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
	target_id = event.key.windowID;
    } else if (event.type == SDL_EVENT_TEXT_INPUT) {
	target_id = event.text.windowID;
    }

    if (target_id != 0) {
	StickyPopupEntry* entry = find_entry(mgr, target_id);
	if (entry != nullptr) {
	    const std::size_t index = static_cast<std::size_t>(entry - mgr.entries.data());
	    if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
		|| event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		return handle_popup_mouse(mgr, index, event);
	    }
	    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP
		|| event.type == SDL_EVENT_TEXT_INPUT) {
		return handle_popup_keyboard(mgr, index, event);
	    }
	}
    }

    return false;
}

std::size_t sticky_popup_count(const StickyPopupManager& mgr) {
    return mgr.entries.size();
}

bool sticky_popup_any_dragging(const StickyPopupManager& mgr) {
    for (const StickyPopupEntry& entry : mgr.entries) {
	if (entry.dragging) {
	    return true;
	}
    }
    return false;
}

bool sticky_popup_consume_dock_request(StickyPopupManager& mgr, StickyPanel& out_panel) {
    if (mgr.dock_request_index >= mgr.entries.size()) {
	return false;
    }

    const std::size_t index = mgr.dock_request_index;
    mgr.dock_request_index = static_cast<std::size_t>(-1);

    persist_panel(mgr.entries[index].panel);
    out_panel = std::move(mgr.entries[index].panel);
    out_panel.x = 0.0f;
    out_panel.y = 0.0f;
    destroy_entry(mgr.entries[index]);
    mgr.entries.erase(mgr.entries.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}
