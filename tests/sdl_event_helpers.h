#pragma once

#include "sticky_gui.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace sdl_test {

inline SDL_Event key_down(SDL_Scancode scancode, SDL_Keymod mod = SDL_KMOD_NONE, bool repeat = false) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = scancode;
    event.key.mod = mod;
    event.key.repeat = repeat;
    event.key.key = SDL_GetKeyFromScancode(scancode, mod, false);
    return event;
}

inline SDL_Event ctrl(SDL_Scancode scancode) {
    return key_down(scancode, SDL_KMOD_CTRL);
}

inline SDL_Event ctrl_shift(SDL_Scancode scancode) {
    return key_down(scancode, static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT));
}

inline SDL_Event text_input(const char* text) {
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    static thread_local char buffer[64];
    std::strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    event.text.text = buffer;
    return event;
}

inline SDL_Event mouse_button_down(float x, float y, Uint8 button = SDL_BUTTON_LEFT) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.x = x;
    event.button.y = y;
    event.button.button = button;
    return event;
}

inline SDL_Event mouse_button_up(float x, float y, Uint8 button = SDL_BUTTON_LEFT) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.x = x;
    event.button.y = y;
    event.button.button = button;
    return event;
}

inline SDL_Event mouse_motion(float x, float y) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = x;
    event.motion.y = y;
    return event;
}

inline SDL_Event mouse_wheel(float x, float y, float wheel_y) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.x = 0.0f;
    event.wheel.y = wheel_y;
    event.wheel.mouse_x = x;
    event.wheel.mouse_y = y;
    return event;
}

inline bool dispatch(StickyGui& gui, const SDL_Event& event, bool& quit_requested) {
    return sticky_gui_handle_event(gui, event, quit_requested);
}

} // namespace sdl_test
