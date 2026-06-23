#pragma once

#include "note_editor.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

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

struct StickyGui {
    std::vector<StickyPanel> panels;
    std::size_t focused = 0;

    enum class Interaction { None, Drag, Resize } interaction = Interaction::None;
    std::size_t active_panel = 0;
    float drag_offset_x = 0.0f;
    float drag_offset_y = 0.0f;

    bool show_help = false;
    bool editing_title = false;
    std::string title_edit_buffer;
    std::size_t title_cursor = 0;

    bool pending_delete = false;
    std::size_t pending_delete_panel = 0;
};

void sticky_gui_init(StickyGui& gui);

void sticky_gui_save_all(const StickyGui& gui);

void sticky_gui_render(SDL_Renderer* renderer, StickyGui& gui);

bool sticky_gui_handle_event(StickyGui& gui, const SDL_Event& event, bool& quit_requested);

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
