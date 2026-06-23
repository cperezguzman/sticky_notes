#pragma once

#include "gui_theme.h"
#include "sticky_gui.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <vector>

struct StickyPopupEntry {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_WindowID window_id = 0;
    StickyPanel panel;
    bool dragging = false;
    bool resizing = false;
    float drag_grab_offset_x = 0.0f;
    float drag_grab_offset_y = 0.0f;
    float drag_global_start_x = 0.0f;
    float drag_global_start_y = 0.0f;
    Uint32 last_title_click_ms = 0;
};

struct StickyPopupManager {
    std::vector<StickyPopupEntry> entries;
    GuiThemeId theme_id = GuiThemeId::Minimal;
    std::size_t dock_request_index = static_cast<std::size_t>(-1);
};

bool sticky_popup_open(StickyPopupManager& mgr, StickyPanel panel, GuiThemeId theme, int screen_x,
		       int screen_y);

void sticky_popup_close_all(StickyPopupManager& mgr);

void sticky_popup_save_all(const StickyPopupManager& mgr);

void sticky_popup_render_all(StickyPopupManager& mgr);

bool sticky_popup_handle_event(StickyPopupManager& mgr, StickyGui& desk, const SDL_Event& event,
			       SDL_WindowID desk_window_id, bool& quit_app);

std::size_t sticky_popup_count(const StickyPopupManager& mgr);

bool sticky_popup_any_dragging(const StickyPopupManager& mgr);

bool sticky_popup_consume_dock_request(StickyPopupManager& mgr, StickyPanel& out_panel);
