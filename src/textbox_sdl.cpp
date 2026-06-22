#include "textbox_sdl.h"

#include "textbox_input.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <string>

namespace {
constexpr float kPadding = 8.0f;
constexpr float kCharW = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
constexpr float kLineH = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);

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
    case SDL_SCANCODE_UP:
	return {TextboxKeyKind::Up, 0};
    case SDL_SCANCODE_DOWN:
	return {TextboxKeyKind::Down, 0};
    case SDL_SCANCODE_HOME:
	return {TextboxKeyKind::Home, 0};
    case SDL_SCANCODE_END:
	return {TextboxKeyKind::End, 0};
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
	return {TextboxKeyKind::Newline, 0};
    default:
	return {TextboxKeyKind::Character, 0};
    }
}

std::size_t visible_body_lines(float body_height) {
    const float inner = std::max(body_height - kPadding, kLineH);
    return std::max<std::size_t>(1, static_cast<std::size_t>(inner / kLineH));
}

std::string panel_title(const EditorSession& session) {
    if (session.note.title.empty()) {
	return "Untitled";
    }
    return session.note.title;
}
} // namespace

float sticky_panel_title_bar_height() {
    return kLineH + 2.0f * kPadding;
}

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

void textbox_render_panel(SDL_Renderer* renderer, float x, float y, float width, float height,
			  const EditorSession& session, TextboxViewport& viewport, bool focused) {
    SDL_FRect panel{x, y, width, height};
    SDL_SetRenderDrawColor(renderer, 50, 48, 42, 255);
    SDL_RenderFillRect(renderer, &panel);
    if (focused) {
	SDL_SetRenderDrawColor(renderer, 255, 220, 80, 255);
    } else {
	SDL_SetRenderDrawColor(renderer, 100, 95, 85, 255);
    }
    SDL_RenderRect(renderer, &panel);

    const float title_bar_h = kLineH + 2.0f * kPadding;
    SDL_FRect title_bar{x, y, width, title_bar_h};
    SDL_SetRenderDrawColor(renderer, 70, 65, 55, 255);
    SDL_RenderFillRect(renderer, &title_bar);

    SDL_SetRenderDrawColor(renderer, 255, 248, 220, 255);
    SDL_RenderDebugText(renderer, x + kPadding, y + kPadding, panel_title(session).c_str());

    const float body_y = y + title_bar_h;
    const float body_h = height - title_bar_h;
    SDL_FRect body{x, body_y, width, body_h};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &body);

    const std::size_t visible = visible_body_lines(body_h);
    textbox_scroll_to_cursor(viewport, session, visible);

    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    const std::size_t line_count = textbox_line_count(session);
    const std::size_t first = viewport.first_visible_line;
    const std::size_t last = std::min(first + visible, line_count);

    for (std::size_t line_idx = first; line_idx < last; ++line_idx) {
	const float row = static_cast<float>(line_idx - first);
	const float text_y = body_y + kPadding + row * kLineH;
	SDL_RenderDebugText(renderer, x + kPadding, text_y, textbox_line_at(session, line_idx).c_str());
    }

    const std::size_t cursor_line = textbox_cursor_line(session);
    if (cursor_line >= first && cursor_line < last) {
	const float row = static_cast<float>(cursor_line - first);
	const float caret_y = body_y + kPadding + row * kLineH;
	const float caret_x = x + kPadding + static_cast<float>(textbox_cursor_column(session)) * kCharW;
	SDL_FRect caret{caret_x, caret_y, 2.0f, kLineH};
	SDL_SetRenderDrawColor(renderer, 255, 220, 80, 255);
	SDL_RenderFillRect(renderer, &caret);
    }

    if (focused) {
	const float grip = 10.0f;
	SDL_FRect resize_grip{width - grip, y + height - grip, grip, grip};
	SDL_SetRenderDrawColor(renderer, 180, 170, 150, 255);
	SDL_RenderFillRect(renderer, &resize_grip);
    }
}
