#include "gui_theme.h"

namespace {
StickyGuiTheme make_minimal() {
    StickyGuiTheme t{};
    t.name = "Minimal";
    t.desk = {28, 28, 32, 255};
    t.panel_fill = {50, 48, 42, 255};
    t.panel_border = {100, 95, 85, 255};
    t.panel_border_focus = {255, 220, 80, 255};
    t.title_bar = {70, 65, 55, 255};
    t.title_text = {255, 248, 220, 255};
    t.body = {40, 40, 40, 255};
    t.body_text = {230, 230, 230, 255};
    t.caret = {255, 220, 80, 255};
    t.close_btn = {120, 50, 50, 255};
    t.close_text = {255, 200, 200, 255};
    t.grip = {180, 170, 150, 255};
    t.overlay_bg = {20, 20, 28, 230};
    t.overlay_border = {120, 120, 140, 255};
    t.overlay_text = {220, 220, 230, 255};
    t.overlay_muted = {150, 150, 165, 255};
    t.status_ok = {120, 200, 140, 255};
    t.status_err = {255, 120, 120, 255};
    return t;
}

StickyGuiTheme make_retro() {
    StickyGuiTheme t{};
    t.name = "Retro";
    t.desk = {192, 192, 192, 255};
    t.panel_fill = {212, 208, 200, 255};
    t.panel_border = {64, 64, 64, 255};
    t.panel_border_focus = {0, 0, 128, 255};
    t.title_bar = {0, 0, 128, 255};
    t.title_text = {255, 255, 255, 255};
    t.body = {255, 255, 255, 255};
    t.body_text = {0, 0, 0, 255};
    t.caret = {0, 0, 128, 255};
    t.close_btn = {192, 192, 192, 255};
    t.close_text = {0, 0, 0, 255};
    t.grip = {128, 128, 128, 255};
    t.overlay_bg = {212, 208, 200, 240};
    t.overlay_border = {64, 64, 64, 255};
    t.overlay_text = {0, 0, 0, 255};
    t.overlay_muted = {64, 64, 64, 255};
    t.status_ok = {0, 100, 0, 255};
    t.status_err = {128, 0, 0, 255};
    t.retro_bevel = true;
    return t;
}

StickyGuiTheme make_cyberpunk() {
    StickyGuiTheme t{};
    t.name = "Cyberpunk";
    t.desk = {4, 8, 12, 255};
    t.panel_fill = {8, 20, 16, 255};
    t.panel_border = {0, 80, 60, 255};
    t.panel_border_focus = {0, 255, 130, 255};
    t.title_bar = {0, 40, 30, 255};
    t.title_text = {0, 255, 130, 255};
    t.body = {2, 12, 10, 255};
    t.body_text = {0, 255, 65, 255};
    t.caret = {255, 0, 180, 255};
    t.close_btn = {80, 0, 60, 255};
    t.close_text = {255, 120, 220, 255};
    t.grip = {0, 180, 100, 255};
    t.overlay_bg = {0, 20, 14, 235};
    t.overlay_border = {0, 255, 130, 255};
    t.overlay_text = {0, 255, 130, 255};
    t.overlay_muted = {0, 140, 90, 255};
    t.status_ok = {0, 255, 130, 255};
    t.status_err = {255, 40, 120, 255};
    return t;
}
} // namespace

StickyGuiTheme gui_theme_get(GuiThemeId id) {
    switch (id) {
    case GuiThemeId::Retro:
	return make_retro();
    case GuiThemeId::Cyberpunk:
	return make_cyberpunk();
    case GuiThemeId::Minimal:
    default:
	return make_minimal();
    }
}

const char* gui_theme_name(GuiThemeId id) {
    return gui_theme_get(id).name;
}

GuiThemeId gui_theme_cycle(GuiThemeId current) {
    const int next = (static_cast<int>(current) + 1) % static_cast<int>(GuiThemeId::Count);
    return static_cast<GuiThemeId>(next);
}

void gui_set_render_color(SDL_Renderer* renderer, const GuiColor& color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}
