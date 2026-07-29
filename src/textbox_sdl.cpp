#include "textbox_sdl.h"

#include "text_font.h"
#include "text_font_render.h"
#include "text_style.h"
#include "textbox_input.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr float kPadding = 8.0f;
constexpr float kChromeCharW = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
constexpr float kChromeLineH = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
constexpr float kBodyScrollBarW = 8.0f;
constexpr float kBodyScrollBarPad = 2.0f;
constexpr float kBodyScrollBarMinThumb = 18.0f;

bool point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && py >= y && px < x + w && py < y + h;
}

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

std::size_t visible_body_lines(float body_height, float line_h) {
    const float inner = std::max(body_height - kPadding, line_h);
    return std::max<std::size_t>(1, static_cast<std::size_t>(inner / line_h));
}

std::size_t max_body_columns(float panel_width, float char_w) {
    const float inner = std::max(panel_width - 2.0f * kPadding, char_w);
    return std::max<std::size_t>(1, static_cast<std::size_t>(inner / char_w));
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
    return kChromeLineH + 2.0f * kPadding;
}

float sticky_panel_close_button_width() {
    return kChromeLineH;
}

float sticky_panel_dock_button_width() {
    return kChromeLineH;
}

void sticky_panel_close_button_rect(float panel_x, float panel_y, float panel_width, float& out_x,
				    float& out_y, float& out_w, float& out_h) {
    const float title_bar_h = sticky_panel_title_bar_height();
    out_w = kChromeLineH;
    out_h = kChromeLineH;
    out_x = panel_x + panel_width - out_w - kPadding;
    out_y = panel_y + (title_bar_h - out_h) * 0.5f;
}

void sticky_panel_dock_button_rect(float panel_x, float panel_y, float panel_width,
				   bool has_close_button, float& out_x, float& out_y, float& out_w,
				   float& out_h) {
    const float title_bar_h = sticky_panel_title_bar_height();
    out_w = kChromeLineH;
    out_h = kChromeLineH;
    out_y = panel_y + (title_bar_h - out_h) * 0.5f;
    const float close_offset = has_close_button ? out_w + kPadding : 0.0f;
    out_x = panel_x + panel_width - out_w - kPadding - close_offset;
}

std::size_t textbox_body_max_columns(float panel_width) {
    return max_body_columns(panel_width, kChromeCharW);
}

std::size_t textbox_body_max_columns_for(float panel_width, NoteTypography typo) {
    const TextFontMetrics metrics = text_font_metrics(typo);
    return max_body_columns(panel_width, metrics.char_w);
}

float textbox_body_wrap_width(float panel_width) {
    const float gutter = kBodyScrollBarW + kBodyScrollBarPad;
    return std::max(kChromeCharW, panel_width - 2.0f * kPadding - gutter);
}

float textbox_body_wrap_width_for(float panel_width, NoteTypography typo) {
    const TextFontMetrics metrics = text_font_metrics(typo);
    const float gutter = kBodyScrollBarW + kBodyScrollBarPad;
    return std::max(metrics.char_w, panel_width - 2.0f * kPadding - gutter);
}

std::size_t textbox_visible_body_lines(float panel_height) {
    const float title_bar_h = sticky_panel_title_bar_height();
    const float body_h = std::max(panel_height - title_bar_h, kChromeLineH);
    return visible_body_lines(body_h, kChromeLineH);
}

std::size_t textbox_visible_body_lines_for(float panel_height, NoteTypography typo) {
    const TextFontMetrics metrics = text_font_metrics(typo);
    const float title_bar_h = sticky_panel_title_bar_height();
    const float body_h = std::max(panel_height - title_bar_h, metrics.line_h);
    return visible_body_lines(body_h, metrics.line_h);
}

bool textbox_handle_sdl_event_width(EditorSession& session, const SDL_Event& event,
				    bool& quit_requested, float max_body_width_px,
				    NoteTypography typo, bool quit_on_escape) {
    typo = note_typography_normalize(typo);
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
	quit_requested = true;
	return true;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
	const bool is_escape = event.key.scancode == SDL_SCANCODE_ESCAPE
	    || event.key.key == SDLK_ESCAPE;
	if (is_escape) {
	    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && quit_on_escape) {
		quit_requested = true;
	    }
	    return event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat;
	}
	if (event.type != SDL_EVENT_KEY_DOWN) {
	    return false;
	}

	const SDL_Keymod mods = event.key.mod;
	const bool ctrl = (mods & SDL_KMOD_CTRL) != 0;
	const bool shift = (mods & SDL_KMOD_SHIFT) != 0;
	if (ctrl && !event.key.repeat) {
	    switch (event.key.scancode) {
	    case SDL_SCANCODE_B:
		if (shift) {
		    editor_toggle_style_flag(session, TextStyleBold);
		    return true;
		}
		break;
	    case SDL_SCANCODE_I:
		editor_toggle_style_flag(session, TextStyleItalic);
		return true;
	    case SDL_SCANCODE_U:
		editor_toggle_style_flag(session, TextStyleUnderline);
		return true;
	    case SDL_SCANCODE_S:
		if (shift) {
		    editor_toggle_style_flag(session, TextStyleStrike);
		    return true;
		}
		break;
	    default:
		break;
	    }
	}

	const TextboxKeyEvent mapped = key_from_scancode(event.key.scancode);
	if (mapped.kind != TextboxKeyKind::Character) {
	    const bool nav = mapped.kind == TextboxKeyKind::Left || mapped.kind == TextboxKeyKind::Right
		|| mapped.kind == TextboxKeyKind::Up || mapped.kind == TextboxKeyKind::Down
		|| mapped.kind == TextboxKeyKind::Home || mapped.kind == TextboxKeyKind::End;
	    if (nav) {
		if (shift) {
		    if (!session.has_selection) {
			editor_set_selection_anchor_to_cursor(session);
		    }
		} else {
		    editor_clear_selection(session);
		}
	    }
	    return textbox_apply_key_width(session, mapped, max_body_width_px, typo);
	}

	// Hold-to-type: TEXT_INPUT often only fires once; KEY_DOWN repeat carries the rest.
	if (!event.key.repeat) {
	    return false;
	}
	const bool chord = (mods & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0;
	if (chord) {
	    return false;
	}
	const SDL_Keycode key = event.key.key;
	if (key >= 32 && key <= 126) {
	    return textbox_apply_key_width(
		session, {TextboxKeyKind::Character, static_cast<char32_t>(key)}, max_body_width_px,
		typo);
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
		textbox_apply_key_width(session,
					{TextboxKeyKind::Character, static_cast<char32_t>(byte)},
					max_body_width_px, typo);
	    }
	}
	return true;

    default:
	return false;
    }
}

bool textbox_handle_sdl_event(EditorSession& session, const SDL_Event& event, bool& quit_requested,
			      std::size_t max_body_columns, bool quit_on_escape) {
    const NoteTypography typo = note_typography_normalize(session.note.typography);
    const float max_width = max_body_columns == 0
	? 0.0f
	: static_cast<float>(max_body_columns) * text_font_metrics(typo).char_w;
    return textbox_handle_sdl_event_width(session, event, quit_requested, max_width, typo,
					  quit_on_escape);
}

bool textbox_handle_body_click(EditorSession& session, TextboxViewport& viewport, float panel_x,
			       float panel_y, float panel_width, float panel_height, float mx,
			       float my, bool extend_selection) {
    const float title_h = sticky_panel_title_bar_height();
    const float body_y = panel_y + title_h;
    const float body_h = panel_height - title_h;
    if (mx < panel_x || my < body_y || mx >= panel_x + panel_width || my >= body_y + body_h) {
	return false;
    }
    if (textbox_point_in_body_scrollbar(session, panel_x, panel_y, panel_width, panel_height, mx,
					my)) {
	return false;
    }
    const TextFontMetrics metrics = text_font_metrics(session.note.typography);
    const std::size_t visible = visible_body_lines(body_h, metrics.line_h);
    if (extend_selection) {
	if (!session.has_selection) {
	    editor_set_selection_anchor_to_cursor(session);
	}
    } else {
	editor_clear_selection(session);
    }

    const NoteTypography typo = note_typography_normalize(session.note.typography);
    if (typo.font == NoteFontId::Debug) {
	textbox_click_body(session, viewport, visible, mx - panel_x, my - body_y, metrics.char_w,
			   metrics.line_h, kPadding);
    } else {
	const float local_x = mx - panel_x - kPadding;
	const float local_y = my - body_y - kPadding;
	int row_in_view = static_cast<int>(std::floor(local_y / metrics.line_h));
	if (row_in_view < 0) {
	    row_in_view = 0;
	}
	const std::size_t line_count = textbox_line_count(session);
	if (line_count == 0) {
	    textbox_set_cursor(session, 0, 0);
	} else {
	    std::size_t line = viewport.first_visible_line + static_cast<std::size_t>(row_in_view);
	    const std::size_t last_visible =
		std::min(viewport.first_visible_line + visible, line_count) - 1;
	    if (line > last_visible) {
		line = last_visible;
	    }
	    std::string line_text = textbox_line_at(session, line);
	    const uint8_t* flags = nullptr;
	    std::vector<uint8_t> style_row;
	    note_styles_ensure(session.note.styles, session.note.text);
	    if (line < session.note.styles.lines.size()) {
		style_row = session.note.styles.lines[line];
		if (style_row.size() < line_text.size()) {
		    style_row.resize(line_text.size(), TextStyleNone);
		}
		flags = style_row.data();
	    }
	    const std::size_t col = text_font_column_at_x(
		nullptr, typo, line_text.c_str(), line_text.size(), flags, local_x);
	    textbox_set_cursor(session, line, col);
	}
    }
    if (extend_selection) {
	session.has_selection = true;
    }
    return true;
}

bool textbox_handle_mouse_wheel(EditorSession& session, TextboxViewport& viewport, float panel_height,
				float wheel_y) {
    if (wheel_y == 0.0f) {
	return false;
    }
    const int delta = wheel_y > 0.0f ? -3 : 3;
    return textbox_scroll_lines(viewport, session,
				textbox_visible_body_lines_for(panel_height, session.note.typography),
				delta);
}

bool textbox_body_scrollbar_needed(const EditorSession& session, float panel_height) {
    const std::size_t visible =
	textbox_visible_body_lines_for(panel_height, session.note.typography);
    return textbox_max_first_visible(session, visible) > 0;
}

void textbox_body_scrollbar_track_rect(float panel_x, float panel_y, float panel_width,
				       float panel_height, float& out_x, float& out_y, float& out_w,
				       float& out_h) {
    const float title_h = sticky_panel_title_bar_height();
    out_x = panel_x + panel_width - kBodyScrollBarPad - kBodyScrollBarW;
    out_y = panel_y + title_h + kBodyScrollBarPad;
    out_w = kBodyScrollBarW;
    out_h = std::max(0.0f, panel_height - title_h - 2.0f * kBodyScrollBarPad);
}

void textbox_body_scrollbar_thumb_rect(const EditorSession& session, const TextboxViewport& viewport,
				       float panel_x, float panel_y, float panel_width,
				       float panel_height, float& out_x, float& out_y, float& out_w,
				       float& out_h) {
    float track_x = 0.0f;
    float track_y = 0.0f;
    float track_w = 0.0f;
    float track_h = 0.0f;
    textbox_body_scrollbar_track_rect(panel_x, panel_y, panel_width, panel_height, track_x, track_y,
				      track_w, track_h);
    out_x = track_x;
    out_w = track_w;

    const std::size_t visible =
	textbox_visible_body_lines_for(panel_height, session.note.typography);
    const std::size_t total = textbox_line_count(session);
    const std::size_t max_first = textbox_max_first_visible(session, visible);
    if (total == 0 || visible == 0 || track_h <= 0.0f) {
	out_y = track_y;
	out_h = track_h;
	return;
    }

    out_h = std::max(kBodyScrollBarMinThumb,
		     track_h * static_cast<float>(visible) / static_cast<float>(total));
    if (out_h > track_h) {
	out_h = track_h;
    }
    const float travel = track_h - out_h;
    if (max_first == 0 || travel <= 0.0f) {
	out_y = track_y;
	return;
    }
    out_y = track_y
	    + travel * static_cast<float>(viewport.first_visible_line)
		  / static_cast<float>(max_first);
}

bool textbox_point_in_body_scrollbar(const EditorSession& session, float panel_x, float panel_y,
				     float panel_width, float panel_height, float mx, float my) {
    if (!textbox_body_scrollbar_needed(session, panel_height)) {
	return false;
    }
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    textbox_body_scrollbar_track_rect(panel_x, panel_y, panel_width, panel_height, x, y, w, h);
    return point_in_rect(mx, my, x, y, w, h);
}

void textbox_scroll_body_from_thumb_y(EditorSession& session, TextboxViewport& viewport,
				      float panel_x, float panel_y, float panel_width,
				      float panel_height, float my, float drag_offset_y) {
    float track_x = 0.0f;
    float track_y = 0.0f;
    float track_w = 0.0f;
    float track_h = 0.0f;
    textbox_body_scrollbar_track_rect(panel_x, panel_y, panel_width, panel_height, track_x, track_y,
				      track_w, track_h);
    float thumb_x = 0.0f;
    float thumb_y = 0.0f;
    float thumb_w = 0.0f;
    float thumb_h = 0.0f;
    textbox_body_scrollbar_thumb_rect(session, viewport, panel_x, panel_y, panel_width, panel_height,
				      thumb_x, thumb_y, thumb_w, thumb_h);
    const std::size_t visible =
	textbox_visible_body_lines_for(panel_height, session.note.typography);
    const std::size_t max_first = textbox_max_first_visible(session, visible);
    const float travel = track_h - thumb_h;
    if (travel <= 0.0f || max_first == 0) {
	textbox_set_first_visible(viewport, session, visible, 0);
	return;
    }
    float rel = (my - drag_offset_y) - track_y;
    if (rel < 0.0f) {
	rel = 0.0f;
    }
    if (rel > travel) {
	rel = travel;
    }
    const std::size_t first = static_cast<std::size_t>(
	std::lround(rel / travel * static_cast<float>(max_first)));
    textbox_set_first_visible(viewport, session, visible, first);
}

void textbox_render_panel(SDL_Renderer* renderer, float x, float y, float width, float height,
			  EditorSession& session, TextboxViewport& viewport, bool focused,
			  const PanelChrome& chrome, const StickyGuiTheme& theme) {
    SDL_FRect panel{x, y, width, height};
    gui_set_render_color(renderer, theme.panel_fill);
    SDL_RenderFillRect(renderer, &panel);
    gui_set_render_color(renderer, focused ? theme.panel_border_focus : theme.panel_border);
    SDL_RenderRect(renderer, &panel);
    if (theme.retro_bevel) {
	draw_retro_bevel(renderer, panel, true);
    }

    const float title_bar_h = sticky_panel_title_bar_height();
    SDL_FRect title_bar{x, y, width, title_bar_h};
    gui_set_render_color(renderer, theme.title_bar);
    SDL_RenderFillRect(renderer, &title_bar);

    gui_set_render_color(renderer, theme.title_text);
    const std::string title =
	chrome.title_text != nullptr ? chrome.title_text : panel_title(session);
    SDL_RenderDebugText(renderer, std::floor(x + kPadding), std::floor(y + kPadding), title.c_str());

    if (chrome.show_close_button) {
	float btn_x = 0.0f;
	float btn_y = 0.0f;
	float btn_w = 0.0f;
	float btn_h = 0.0f;
	sticky_panel_close_button_rect(x, y, width, btn_x, btn_y, btn_w, btn_h);
	SDL_FRect close_btn{btn_x, btn_y, btn_w, btn_h};
	gui_set_render_color(renderer, theme.close_btn);
	SDL_RenderFillRect(renderer, &close_btn);
	gui_set_render_color(renderer, theme.close_text);
	const float glyph_x = std::floor(btn_x + (btn_w - kChromeCharW) * 0.5f);
	SDL_RenderDebugText(renderer, glyph_x, std::floor(btn_y), "x");
    }

    if (chrome.show_dock_button) {
	float btn_x = 0.0f;
	float btn_y = 0.0f;
	float btn_w = 0.0f;
	float btn_h = 0.0f;
	sticky_panel_dock_button_rect(x, y, width, chrome.show_close_button, btn_x, btn_y, btn_w,
				      btn_h);
	SDL_FRect dock_btn{btn_x, btn_y, btn_w, btn_h};
	gui_set_render_color(renderer, theme.close_btn);
	SDL_RenderFillRect(renderer, &dock_btn);
	gui_set_render_color(renderer, theme.close_text);
	const float glyph_x = std::floor(btn_x + (btn_w - kChromeCharW) * 0.5f);
	SDL_RenderDebugText(renderer, glyph_x, std::floor(btn_y), "v");
    }

    if (chrome.title_caret_visible) {
	const float caret_x =
	    x + kPadding + static_cast<float>(chrome.title_caret_col) * kChromeCharW;
	SDL_FRect title_caret{caret_x, y + kPadding, 2.0f, kChromeLineH};
	gui_set_render_color(renderer, theme.caret);
	SDL_RenderFillRect(renderer, &title_caret);
    }

    const float body_y = y + title_bar_h;
    const float body_h = height - title_bar_h;
    SDL_FRect body{x, body_y, width, body_h};
    gui_set_render_color(renderer, theme.body);
    SDL_RenderFillRect(renderer, &body);

    const NoteTypography typo = note_typography_normalize(session.note.typography);
    const TextFontMetrics metrics = text_font_metrics(typo);
    // Soft wrap is applied by callers on edit/resize/font change — not every frame.

    const std::size_t visible = visible_body_lines(body_h, metrics.line_h);
    // Resize pins to the start; do not chase the cursor here (that caused mid-note views).
    textbox_clamp_viewport(viewport, session, visible);

    gui_set_render_color(renderer, theme.body_text);
    const std::size_t line_count = textbox_line_count(session);
    const std::size_t first = viewport.first_visible_line;
    const std::size_t last = std::min(first + visible, line_count);

    std::size_t sel_a_line = 0;
    std::size_t sel_a_col = 0;
    std::size_t sel_b_line = 0;
    std::size_t sel_b_col = 0;
    const bool show_sel = editor_selection_active(session);
    if (show_sel) {
	editor_get_normalized_selection(session, sel_a_line, sel_a_col, sel_b_line, sel_b_col);
    }

    note_styles_ensure(session.note.styles, session.note.text);

    for (std::size_t line_idx = first; line_idx < last; ++line_idx) {
	const float row = static_cast<float>(line_idx - first);
	const float text_x = std::floor(x + kPadding);
	const float text_y = std::floor(body_y + kPadding + row * metrics.line_h);
	std::string line = textbox_line_at(session, line_idx);

	if (show_sel && line_idx >= sel_a_line && line_idx <= sel_b_line) {
	    const std::size_t start = (line_idx == sel_a_line) ? sel_a_col : 0;
	    const std::size_t end = (line_idx == sel_b_line) ? sel_b_col : line.size();
	    if (end > start) {
		const uint8_t* sel_flags = nullptr;
		std::vector<uint8_t> sel_styles;
		if (line_idx < session.note.styles.lines.size()) {
		    sel_styles = session.note.styles.lines[line_idx];
		    if (sel_styles.size() < line.size()) {
			sel_styles.resize(line.size(), TextStyleNone);
		    }
		    sel_flags = sel_styles.data();
		}
		const float hi_x =
		    text_x
		    + text_font_prefix_width(renderer, typo, line.c_str(), start, sel_flags,
					     line.size());
		const float hi_w = text_font_prefix_width(renderer, typo, line.c_str(), end, sel_flags,
							  line.size())
				   - (hi_x - text_x);
		SDL_FRect hi{hi_x, text_y, std::max(2.0f, hi_w), metrics.line_h};
		gui_set_render_color(renderer, theme.panel_border_focus);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, theme.panel_border_focus.r,
				       theme.panel_border_focus.g, theme.panel_border_focus.b, 80);
		SDL_RenderFillRect(renderer, &hi);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	    }
	}

	gui_set_render_color(renderer, theme.body_text);
	const uint8_t* flags = nullptr;
	std::vector<uint8_t> style_row;
	if (line_idx < session.note.styles.lines.size()) {
	    style_row = session.note.styles.lines[line_idx];
	    if (style_row.size() < line.size()) {
		style_row.resize(line.size(), TextStyleNone);
	    }
	    flags = style_row.data();
	}
	text_font_draw_styled(renderer, typo, text_x, text_y, line.c_str(), flags, line.size());
    }

    const std::size_t cursor_line = textbox_cursor_line(session);
    if (cursor_line >= first && cursor_line < last) {
	const float row = static_cast<float>(cursor_line - first);
	const float caret_y = std::floor(body_y + kPadding + row * metrics.line_h);
	std::string caret_line = textbox_line_at(session, cursor_line);
	const uint8_t* caret_flags = nullptr;
	std::vector<uint8_t> caret_styles;
	if (cursor_line < session.note.styles.lines.size()) {
	    caret_styles = session.note.styles.lines[cursor_line];
	    if (caret_styles.size() < caret_line.size()) {
		caret_styles.resize(caret_line.size(), TextStyleNone);
	    }
	    caret_flags = caret_styles.data();
	}
	const float caret_x = std::floor(
	    x + kPadding
	    + text_font_prefix_width(renderer, typo, caret_line.c_str(), textbox_cursor_column(session),
				     caret_flags, caret_line.size()));
	SDL_FRect caret{caret_x, caret_y, 2.0f, metrics.line_h};
	gui_set_render_color(renderer, theme.caret);
	SDL_RenderFillRect(renderer, &caret);
    }

    if (textbox_body_scrollbar_needed(session, height)) {
	float track_x = 0.0f;
	float track_y = 0.0f;
	float track_w = 0.0f;
	float track_h = 0.0f;
	textbox_body_scrollbar_track_rect(x, y, width, height, track_x, track_y, track_w, track_h);
	SDL_FRect track{track_x, track_y, track_w, track_h};
	gui_set_render_color(renderer, theme.overlay_border);
	SDL_RenderFillRect(renderer, &track);

	float thumb_x = 0.0f;
	float thumb_y = 0.0f;
	float thumb_w = 0.0f;
	float thumb_h = 0.0f;
	textbox_body_scrollbar_thumb_rect(session, viewport, x, y, width, height, thumb_x, thumb_y,
					  thumb_w, thumb_h);
	SDL_FRect thumb{thumb_x, thumb_y, thumb_w, thumb_h};
	gui_set_render_color(renderer, focused ? theme.panel_border_focus : theme.grip);
	SDL_RenderFillRect(renderer, &thumb);
    }

    if (focused && chrome.show_resize_grip) {
	const float grip = 10.0f;
	SDL_FRect resize_grip{x + width - grip, y + height - grip, grip, grip};
	gui_set_render_color(renderer, theme.grip);
	SDL_RenderFillRect(renderer, &resize_grip);
    }
}
