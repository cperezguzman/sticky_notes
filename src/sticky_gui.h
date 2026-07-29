#pragma once

// sticky_gui — SDL desk of sticky panels (sidebar, theme, format bar).
//
// On the desk, the focused note fills the content area (not user-draggable/resizable);
// size tracks the window via desk_w/desk_h. Pop-outs keep floating drag/resize.
// Each StickyPanel embeds an EditorSession + TextboxViewport. StickyGui holds the
// panel list, focus, theme, and modal UI state (open picker, theme picker, etc.).
// Input/rendering are driven from sticky_gui.cpp + textbox_sdl.

#include "gui_theme.h"
#include "note_editor.h"
#include "note_store.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

struct StickyPanel {
    EditorSession session;
    float x = 40.0f;
    float y = 40.0f;
    float width = 280.0f;
    float height = 200.0f;
    TextboxViewport viewport{};
};

struct OpenPickerEntry {
    int id = 0;
    std::string title;
    std::string path;
};

struct SidebarEntry {
    int id = 0;
    std::string title;
    std::string path;
    std::chrono::system_clock::time_point last_edited{};
    bool on_desk = false;
};

struct StickyGui {
    std::vector<StickyPanel> panels;
    std::size_t focused = 0;

    // Live desk client size (updated from the SDL window on resize/render).
    float desk_w = 960.0f;
    float desk_h = 640.0f;

    enum class Interaction { None, Drag, Resize } interaction = Interaction::None;
    std::size_t active_panel = 0;
    float drag_offset_x = 0.0f;
    float drag_offset_y = 0.0f;

    GuiThemeId theme_id = GuiThemeId::Minimal;

    bool show_theme_picker = false;
    std::size_t theme_picker_cursor = 0;

    bool show_help = false;
    bool editing_title = false;
    std::string title_edit_buffer;
    std::size_t title_cursor = 0;

    bool pending_delete = false;
    std::size_t pending_delete_panel = 0;

    bool show_open_picker = false;
    std::vector<OpenPickerEntry> open_picker_entries;
    std::size_t open_picker_cursor = 0;

    bool editing_find = false;
    std::string find_buffer;
    std::size_t find_cursor = 0;
    std::string find_status;

    bool editing_sidebar_search = false;
    std::string sidebar_search_buffer;
    std::size_t sidebar_search_cursor = 0;

    Uint32 save_toast_until_ms = 0;
    std::string toast_message = "Saved";

    // Format bar (font / size / B I U S)
    enum class FormatPicker { None, Font, Size } format_picker = FormatPicker::None;
    std::size_t format_picker_cursor = 0;
    bool body_selecting = false;
    bool body_scrollbar_dragging = false;
    float body_scrollbar_drag_offset_y = 0.0f;

    bool pop_out_requested = false;
    std::size_t pop_out_panel_index = 0;
    float title_drag_start_x = 0.0f;
    float title_drag_start_y = 0.0f;
    Uint32 last_title_click_ms = 0;
    std::size_t last_title_click_panel = 0;

    bool sidebar_visible = true;
    std::vector<SidebarEntry> sidebar_entries;
    std::size_t sidebar_scroll = 0;
    bool sidebar_scrollbar_dragging = false;
    float sidebar_scrollbar_drag_offset_y = 0.0f;
    bool sidebar_drag_active = false;
    bool sidebar_drag_pop_out_done = false;
    std::size_t sidebar_drag_index = 0;
    float sidebar_drag_start_mx = 0.0f;
    float sidebar_drag_start_my = 0.0f;
    bool sidebar_pop_out_pending = false;
    StickyPanel sidebar_pop_out_panel{};
    int sidebar_pop_out_screen_x = 0;
    int sidebar_pop_out_screen_y = 0;
};

void sticky_gui_init(StickyGui& gui);

void sticky_gui_save_all(const StickyGui& gui);

void sticky_gui_render(SDL_Renderer* renderer, StickyGui& gui, SDL_Window* desk_window = nullptr);

bool sticky_gui_handle_event(StickyGui& gui, const SDL_Event& event, bool& quit_requested,
			     SDL_WindowID desk_window_id = 0);

void sticky_gui_install_desk_hit_test(SDL_Window* window);

bool sticky_gui_consume_pop_out_request(StickyGui& gui, StickyPanel& out_panel);

bool sticky_gui_consume_sidebar_pop_out_request(StickyGui& gui, StickyPanel& out_panel, int& out_screen_x,
						int& out_screen_y);

void sticky_gui_attach_panel(StickyGui& gui, StickyPanel panel);

bool sticky_gui_pop_out_screen_position(SDL_Window* desk_window, const StickyPanel& panel, int& out_x,
					int& out_y);

StickyGuiTheme sticky_gui_active_theme(const StickyGui& gui);

GuiColor sticky_gui_desk_color(const StickyGui& gui);

void sticky_gui_set_theme(StickyGui& gui, GuiThemeId id);

// Test / harness helpers (no SDL init required for event simulation).
void sticky_gui_reset(StickyGui& gui);

void sticky_gui_add_panel_from_note(StickyGui& gui, const sticky_note& note, float x, float y);

std::size_t sticky_gui_panel_count(const StickyGui& gui);

std::size_t sticky_gui_focused_index(const StickyGui& gui);

const sticky_note& sticky_gui_focused_note(const StickyGui& gui);

const StickyPanel& sticky_gui_panel_at(const StickyGui& gui, std::size_t index);

struct StickyPanelHitTargets {
    float close_x = 0.0f;
    float close_y = 0.0f;
    float title_x = 0.0f;
    float title_y = 0.0f;
    float grip_x = 0.0f;
    float grip_y = 0.0f;
};

void sticky_gui_panel_hit_targets(const StickyPanel& panel, StickyPanelHitTargets& out);

bool sticky_gui_modal_blocks_body(const StickyGui& gui);

bool sticky_gui_sidebar_visible(const StickyGui& gui);

std::size_t sticky_gui_sidebar_entry_count(const StickyGui& gui);

void sticky_gui_reflow_panel(StickyGui& gui, std::size_t panel_index);
