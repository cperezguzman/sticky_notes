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

void draw_retro_bevel(SDL_Renderer* renderer, const SDL_FRect& rect, bool raised) {
    const Uint8 hi = raised ? 255 : 64;
    const Uint8 lo = raised ? 64 : 255;
    SDL_FRect top{rect.x, rect.y, rect.w, 2.0f};
    SDL_FRect left{rect.x, rect.y, 2.0f, rect.h};
    SDL_FRect bottom{rect.x, rect.y + rect.h - 2.0f, rect.w, 2.0f};
    SDL_FRect right{rect.x + rect.w - 2.0f, rect.y, 2.0f, rect.h};
    SDL_SetRenderDrawColor(renderer, hi, hi, hi, 255);
    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &left);
    SDL_SetRenderDrawColor(renderer, lo, lo, lo, 255);
    SDL_RenderFillRect(renderer, &bottom);
    SDL_RenderFillRect(renderer, &right);
}
} // namespace

float sticky_panel_title_bar_height() {
    return kLineH + 2.0f * kPadding;
}

float sticky_panel_close_button_width() {
    return kLineH + kPadding;
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
			  const EditorSession& session, TextboxViewport& viewport, bool focused,
			  const PanelChrome& chrome, const StickyGuiTheme& theme) {
    SDL_FRect panel{x, y, width, height};
    gui_set_render_color(renderer, theme.panel_fill);
    SDL_RenderFillRect(renderer, &panel);
    gui_set_render_color(renderer, focused ? theme.panel_border_focus : theme.panel_border);
    SDL_RenderRect(renderer, &panel);
    if (theme.retro_bevel) {
	draw_retro_bevel(renderer, panel, true);
    }

    const float title_bar_h = kLineH + 2.0f * kPadding;
    SDL_FRect title_bar{x, y, width, title_bar_h};
    gui_set_render_color(renderer, theme.title_bar);
    SDL_RenderFillRect(renderer, &title_bar);

    gui_set_render_color(renderer, theme.title_text);
    const char* title = chrome.title_text != nullptr ? chrome.title_text : panel_title(session).c_str();
    SDL_RenderDebugText(renderer, x + kPadding, y + kPadding, title);

    if (chrome.show_close_button) {
	const float btn_w = sticky_panel_close_button_width();
	const float btn_x = x + width - btn_w - kPadding * 0.5f;
	SDL_FRect close_btn{btn_x, y + kPadding * 0.5f, btn_w, kLineH};
	gui_set_render_color(renderer, theme.close_btn);
	SDL_RenderFillRect(renderer, &close_btn);
	gui_set_render_color(renderer, theme.close_text);
	SDL_RenderDebugText(renderer, btn_x + 4.0f, y + kPadding, "x");
    }

    if (chrome.title_caret_visible) {
	const float caret_x = x + kPadding + static_cast<float>(chrome.title_caret_col) * kCharW;
	SDL_FRect title_caret{caret_x, y + kPadding, 2.0f, kLineH};
	gui_set_render_color(renderer, theme.caret);
	SDL_RenderFillRect(renderer, &title_caret);
    }

    const float body_y = y + title_bar_h;
    const float body_h = height - title_bar_h;
    SDL_FRect body{x, body_y, width, body_h};
    gui_set_render_color(renderer, theme.body);
    SDL_RenderFillRect(renderer, &body);

    const std::size_t visible = visible_body_lines(body_h);
    textbox_scroll_to_cursor(viewport, session, visible);

    gui_set_render_color(renderer, theme.body_text);
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
	gui_set_render_color(renderer, theme.caret);
	SDL_RenderFillRect(renderer, &caret);
    }

    if (focused) {
	const float grip = 10.0f;
	SDL_FRect resize_grip{x + width - grip, y + height - grip, grip, grip};
	gui_set_render_color(renderer, theme.grip);
	SDL_RenderFillRect(renderer, &resize_grip);
    }
}
