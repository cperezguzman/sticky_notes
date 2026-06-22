#pragma once

#include "note_editor.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

#include <cstddef>
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
};

void sticky_gui_init(StickyGui& gui);

void sticky_gui_save_all(const StickyGui& gui);

void sticky_gui_render(SDL_Renderer* renderer, StickyGui& gui);

bool sticky_gui_handle_event(StickyGui& gui, const SDL_Event& event, bool& quit_requested);
