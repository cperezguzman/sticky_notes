#include "textbox_sdl.h"

#include "textbox_input.h"

#include <SDL3/SDL.h>

#include <string>

namespace {
TextboxKeyEvent key_from_scancode(SDL_Scancode scancode) {
    switch (scancode) {
    case SDL_SCANCODE_BACKSPACE:
	return {TextboxKeyKind::Backspace, 0};
    case SDL_SCANCODE_DELETE:
	return {TextboxKeyKind::Delete, 0};
    case SDL_SCANCODE_LEFT:
	return {TextboxKeyKind::Left, 0};
    case SDL_SCANCODE_RIGHT:
	return {TextboxKeyKind::Right, 0};
    case SDL_SCANCODE_HOME:
	return {TextboxKeyKind::Home, 0};
    case SDL_SCANCODE_END:
	return {TextboxKeyKind::End, 0};
    default:
	return {TextboxKeyKind::Character, 0};
    }
}
} // namespace

bool textbox_handle_sdl_event(EditorSession& session, const SDL_Event& event, bool& quit_requested) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
	quit_requested = true;
	return true;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
	const bool is_escape = event.key.scancode == SDL_SCANCODE_ESCAPE
	    || event.key.key == SDLK_ESCAPE;
	if (is_escape && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	    quit_requested = true;
	    return true;
	}
	if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
	    return false;
	}
	const TextboxKeyEvent mapped = key_from_scancode(event.key.scancode);
	if (mapped.kind != TextboxKeyKind::Character) {
	    return textbox_apply_key(session, mapped);
	}
	return false;
    }

    case SDL_EVENT_TEXT_INPUT:
	if (event.text.text == nullptr) {
	    return false;
	}
	for (const char* p = event.text.text; *p != '\0'; ++p) {
	    const unsigned char byte = static_cast<unsigned char>(*p);
	    if (byte >= 32 && byte <= 126) {
		textbox_apply_key(session, {TextboxKeyKind::Character, static_cast<char32_t>(byte)});
	    }
	}
	return true;

    default:
	return false;
    }
}

void textbox_render(SDL_Renderer* renderer, float x, float y, float width, float height,
		    const EditorSession& session) {
    constexpr float kPadding = 8.0f;
    constexpr float kCharW = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);

    SDL_FRect box{x, y, width, height};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    SDL_RenderRect(renderer, &box);

    const std::string line = textbox_line_text(session);
    const std::size_t cursor = textbox_cursor_column(session);

    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderDebugText(renderer, x + kPadding, y + kPadding, line.c_str());

    const float caret_x = x + kPadding + static_cast<float>(cursor) * kCharW;
    SDL_FRect caret{caret_x, y + kPadding, 2.0f, kCharW};
    SDL_SetRenderDrawColor(renderer, 255, 220, 80, 255);
    SDL_RenderFillRect(renderer, &caret);
}
