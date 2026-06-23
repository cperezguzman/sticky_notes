#pragma once

#include <SDL3/SDL.h>

#include <cstddef>

struct GuiColor {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 255;
};

struct StickyGuiTheme {
    const char* name = "Minimal";
    GuiColor desk{};
    GuiColor panel_fill{};
    GuiColor panel_border{};
    GuiColor panel_border_focus{};
    GuiColor title_bar{};
    GuiColor title_text{};
    GuiColor body{};
    GuiColor body_text{};
    GuiColor caret{};
    GuiColor close_btn{};
    GuiColor close_text{};
    GuiColor grip{};
    GuiColor overlay_bg{};
    GuiColor overlay_border{};
    GuiColor overlay_text{};
    GuiColor overlay_muted{};
    GuiColor status_ok{};
    GuiColor status_err{};
    bool retro_bevel = false;
};

enum class GuiThemeId { Minimal = 0, Retro, Cyberpunk, Count };

StickyGuiTheme gui_theme_get(GuiThemeId id);

const char* gui_theme_name(GuiThemeId id);

GuiThemeId gui_theme_cycle(GuiThemeId current);

void gui_set_render_color(SDL_Renderer* renderer, const GuiColor& color);
