#include "sticky_popup.h"

#include "note_editor.h"
#include "note_store.h"
#include "text_font.h"
#include "text_style.h"
#include "textbox_sdl.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr float kMinPanelW = 280.0f;
constexpr float kMinPanelH = 160.0f;
constexpr float kResizeGrip = 10.0f;
constexpr float kPadding = 8.0f;
constexpr float kLineH = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
constexpr float kFormatBarH = 28.0f;
constexpr float kFormatBarGap = 6.0f;
constexpr float kFormatBarBottomPad = 6.0f;
constexpr std::size_t kThemeCount = static_cast<std::size_t>(GuiThemeId::Count);
constexpr Uint32 kDoubleClickMs = 400;

bool point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

void close_button_rect(float panel_w, float& out_x, float& out_y, float& out_w, float& out_h) {
    sticky_panel_close_button_rect(0.0f, 0.0f, panel_w, out_x, out_y, out_w, out_h);
}

void dock_button_rect(float panel_w, float& out_x, float& out_y, float& out_w, float& out_h) {
    sticky_panel_dock_button_rect(0.0f, 0.0f, panel_w, true, out_x, out_y, out_w, out_h);
}

float popup_format_reserve() {
    return kFormatBarGap + kFormatBarH + kFormatBarBottomPad;
}

float popup_note_height(float window_h) {
    return std::max(kMinPanelH * 0.5f, window_h - popup_format_reserve());
}

float popup_format_bar_y(float window_h) {
    return popup_note_height(window_h) + kFormatBarGap;
}

void persist_panel(StickyPanel& panel) {
    if (panel.session.note.note_path.empty()) {
	const std::string title = panel.session.note.title.empty()
	    ? "Untitled"
	    : panel.session.note.title;
	sticky_note created = create_note_silent(title);
	created.text = textbox_storage_lines(panel.session);
	created.styles = textbox_storage_styles(panel.session);
	created.typography = panel.session.note.typography;
	panel.session.note = created;
	textbox_init_hard_breaks_for_loaded_note(panel.session);
	save_note(panel.session.note);
	return;
    }
    sticky_note to_save = panel.session.note;
    to_save.text = textbox_storage_lines(panel.session);
    to_save.styles = textbox_storage_styles(panel.session);
    save_note(to_save);
}

bool read_popup_render_size(StickyPopupEntry& entry, float& out_w, float& out_h) {
    int w = 0;
    int h = 0;
    if (entry.renderer != nullptr && SDL_GetRenderOutputSize(entry.renderer, &w, &h)) {
	out_w = static_cast<float>(w);
	out_h = static_cast<float>(h);
	return true;
    }
    if (entry.window != nullptr && SDL_GetWindowSizeInPixels(entry.window, &w, &h)) {
	out_w = static_cast<float>(w);
	out_h = static_cast<float>(h);
	return true;
    }
    if (entry.window != nullptr && SDL_GetWindowSize(entry.window, &w, &h)) {
	out_w = static_cast<float>(w);
	out_h = static_cast<float>(h);
	return true;
    }
    return false;
}

void sync_panel_size_from_window(StickyPopupEntry& entry) {
    float new_w = entry.panel.width;
    float new_h = entry.panel.height;
    if (!read_popup_render_size(entry, new_w, new_h)) {
	return;
    }
    new_w = std::max(new_w, kMinPanelW);
    new_h = std::max(new_h, kMinPanelH);
    const bool size_changed = new_w != entry.panel.width || new_h != entry.panel.height;
    entry.panel.width = new_w;
    entry.panel.height = new_h;
    if (size_changed) {
	textbox_enforce_wrap_width(entry.panel.session, textbox_body_wrap_width_for(entry.panel.width, entry.panel.session.note.typography), entry.panel.session.note.typography);
	textbox_pin_viewport_to_start(entry.panel.viewport);
    }
}

void destroy_entry(StickyPopupEntry& entry) {
    if (entry.window != nullptr) {
	SDL_DestroyRenderer(entry.renderer);
	SDL_DestroyWindow(entry.window);
	entry.window = nullptr;
	entry.renderer = nullptr;
    }
}

StickyPopupEntry* find_entry(StickyPopupManager& mgr, SDL_WindowID window_id) {
    for (StickyPopupEntry& entry : mgr.entries) {
	if (entry.window_id == window_id) {
	    return &entry;
	}
    }
    return nullptr;
}

void request_dock(StickyPopupManager& mgr, std::size_t index) {
    if (index < mgr.entries.size()) {
	mgr.dock_request_index = index;
    }
}

void close_popup_at(StickyPopupManager& mgr, std::size_t index) {
    if (index >= mgr.entries.size()) {
	return;
    }
    persist_panel(mgr.entries[index].panel);
    destroy_entry(mgr.entries[index]);
    mgr.entries.erase(mgr.entries.begin() + static_cast<std::ptrdiff_t>(index));
}

bool handle_title_double_click(StickyPopupEntry& entry, Uint32 now) {
    if (entry.last_title_click_ms != 0 && now - entry.last_title_click_ms <= kDoubleClickMs) {
	entry.last_title_click_ms = 0;
	return true;
    }
    entry.last_title_click_ms = now;
    return false;
}

void render_overlay_bar(SDL_Renderer* renderer, float x, float y, float w, float h,
			const StickyGuiTheme& theme) {
    SDL_FRect bar{x, y, w, h};
    gui_set_render_color(renderer, theme.overlay_bg);
    SDL_RenderFillRect(renderer, &bar);
    gui_set_render_color(renderer, theme.overlay_border);
    SDL_RenderRect(renderer, &bar);
}

void format_font_rect(float bar_y, float& x, float& y, float& w, float& h) {
    x = kPadding;
    y = bar_y;
    w = 120.0f;
    h = kFormatBarH;
}

void format_size_rect(float bar_y, float& x, float& y, float& w, float& h) {
    x = kPadding + 128.0f;
    y = bar_y;
    w = 56.0f;
    h = kFormatBarH;
}

void format_style_rect(float bar_y, int index, float& x, float& y, float& w, float& h) {
    x = kPadding + 192.0f + static_cast<float>(index) * 26.0f;
    y = bar_y;
    w = 22.0f;
    h = kFormatBarH;
}

void format_theme_rect(float window_w, float bar_y, float& x, float& y, float& w, float& h) {
    w = 120.0f;
    h = kFormatBarH;
    x = std::max(kPadding + 300.0f, window_w - kPadding - w - kResizeGrip - 4.0f);
    y = bar_y;
}

void apply_format_picker(StickyPopupEntry& entry) {
    StickyPanel& panel = entry.panel;
    if (entry.format_picker == StickyPopupEntry::FormatPicker::Font) {
	panel.session.note.typography = note_typography_set_font(
	    panel.session.note.typography, note_font_at_index(entry.format_picker_cursor));
	textbox_enforce_wrap_width(panel.session, textbox_body_wrap_width_for(panel.width, panel.session.note.typography), panel.session.note.typography);
	textbox_pin_viewport_to_start(panel.viewport);
    } else if (entry.format_picker == StickyPopupEntry::FormatPicker::Size) {
	if (entry.format_picker_cursor < kTtfSizePresetCount) {
	    panel.session.note.typography = note_typography_set_size(
		panel.session.note.typography, kTtfSizePresets[entry.format_picker_cursor]);
	    textbox_enforce_wrap_width(panel.session, textbox_body_wrap_width_for(panel.width, panel.session.note.typography), panel.session.note.typography);
	}
    }
    entry.format_picker = StickyPopupEntry::FormatPicker::None;
}

void apply_theme(StickyPopupManager& mgr, StickyGui& desk, std::size_t theme_index) {
    if (theme_index >= kThemeCount) {
	return;
    }
    const GuiThemeId id = static_cast<GuiThemeId>(theme_index);
    mgr.theme_id = id;
    sticky_gui_set_theme(desk, id);
}

void render_format_bar(SDL_Renderer* renderer, StickyPopupEntry& entry, const StickyGuiTheme& theme,
		       GuiThemeId theme_id) {
    const float bar_y = popup_format_bar_y(entry.panel.height);
    const float window_w = entry.panel.width;
    StickyPanel& panel = entry.panel;
    const NoteTypography typo = note_typography_normalize(panel.session.note.typography);
    const uint8_t flags = editor_active_style_flags(panel.session);

    float fx = 0, fy = 0, fw = 0, fh = 0;
    format_font_rect(bar_y, fx, fy, fw, fh);
    render_overlay_bar(renderer, fx, fy, fw, fh, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    std::string font_label = std::string("Font:") + note_font_display_name(typo.font);
    if (font_label.size() > 14) {
	font_label = font_label.substr(0, 14);
    }
    SDL_RenderDebugText(renderer, fx + 4.0f, fy + 10.0f, font_label.c_str());

    format_size_rect(bar_y, fx, fy, fw, fh);
    render_overlay_bar(renderer, fx, fy, fw, fh, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    const std::string size_label =
	typo.font == NoteFontId::Debug ? "8px" : (std::to_string(typo.size_px) + "px");
    SDL_RenderDebugText(renderer, fx + 8.0f, fy + 10.0f, size_label.c_str());

    const char* style_glyphs[] = {"B", "I", "U", "S"};
    const uint8_t style_flags[] = {TextStyleBold, TextStyleItalic, TextStyleUnderline,
				   TextStyleStrike};
    for (int i = 0; i < 4; ++i) {
	format_style_rect(bar_y, i, fx, fy, fw, fh);
	render_overlay_bar(renderer, fx, fy, fw, fh, theme);
	const bool on = (flags & style_flags[i]) != 0;
	gui_set_render_color(renderer, on ? theme.status_ok : theme.overlay_text);
	SDL_RenderDebugText(renderer, fx + 7.0f, fy + 10.0f, style_glyphs[i]);
    }

    format_theme_rect(window_w, bar_y, fx, fy, fw, fh);
    render_overlay_bar(renderer, fx, fy, fw, fh, theme);
    gui_set_render_color(renderer, theme.overlay_text);
    std::string theme_label = std::string("Theme:") + gui_theme_name(theme_id);
    if (theme_label.size() > 14) {
	theme_label = theme_label.substr(0, 14);
    }
    SDL_RenderDebugText(renderer, fx + 4.0f, fy + 10.0f, theme_label.c_str());
    SDL_RenderDebugText(renderer, fx + fw - 14.0f, fy + 10.0f, entry.show_theme_picker ? "^" : "v");

    if (entry.format_picker == StickyPopupEntry::FormatPicker::Font) {
	const float dd_h = static_cast<float>(kNoteFontCount) * kLineH + 8.0f;
	format_font_rect(bar_y, fx, fy, fw, fh);
	render_overlay_bar(renderer, fx, fy - dd_h, fw, dd_h, theme);
	for (int i = 0; i < kNoteFontCount; ++i) {
	    const float row_y = fy - dd_h + 4.0f + static_cast<float>(i) * kLineH;
	    gui_set_render_color(renderer, static_cast<std::size_t>(i) == entry.format_picker_cursor
					       ? theme.status_ok
					       : theme.overlay_text);
	    SDL_RenderDebugText(renderer, fx + 6.0f, row_y,
				note_font_display_name(note_font_at_index(static_cast<std::size_t>(i))));
	}
    } else if (entry.format_picker == StickyPopupEntry::FormatPicker::Size
	       && typo.font != NoteFontId::Debug) {
	const float dd_h = static_cast<float>(kTtfSizePresetCount) * kLineH + 8.0f;
	format_size_rect(bar_y, fx, fy, fw, fh);
	render_overlay_bar(renderer, fx, fy - dd_h, fw, dd_h, theme);
	for (std::size_t i = 0; i < kTtfSizePresetCount; ++i) {
	    const float row_y = fy - dd_h + 4.0f + static_cast<float>(i) * kLineH;
	    gui_set_render_color(renderer,
				 i == entry.format_picker_cursor ? theme.status_ok : theme.overlay_text);
	    const std::string lab = std::to_string(kTtfSizePresets[i]);
	    SDL_RenderDebugText(renderer, fx + 12.0f, row_y, lab.c_str());
	}
    }

    if (entry.show_theme_picker) {
	format_theme_rect(window_w, bar_y, fx, fy, fw, fh);
	const float dd_h = kLineH + 4.0f + static_cast<float>(kThemeCount) * (kLineH + 4.0f) + 8.0f;
	render_overlay_bar(renderer, fx, fy - dd_h, fw, dd_h, theme);
	gui_set_render_color(renderer, theme.overlay_muted);
	SDL_RenderDebugText(renderer, fx + 6.0f, fy - dd_h + 2.0f, "Choose theme");
	float row_y = fy - dd_h + 4.0f + kLineH;
	for (std::size_t i = 0; i < kThemeCount; ++i) {
	    const bool selected = (i == entry.theme_picker_cursor);
	    const bool active = (static_cast<GuiThemeId>(i) == theme_id);
	    if (selected) {
		SDL_FRect row{fx + 4.0f, row_y - 2.0f, fw - 8.0f, kLineH + 4.0f};
		gui_set_render_color(renderer, theme.panel_border_focus);
		SDL_RenderFillRect(renderer, &row);
	    }
	    gui_set_render_color(renderer, selected ? theme.desk : theme.overlay_text);
	    std::string line = std::string(active ? "* " : "  ") + gui_theme_name(static_cast<GuiThemeId>(i));
	    SDL_RenderDebugText(renderer, fx + 10.0f, row_y, line.c_str());
	    row_y += kLineH + 4.0f;
	}
    }
}

bool handle_format_bar_mouse(StickyPopupManager& mgr, StickyPopupEntry& entry, StickyGui& desk,
			     float mx, float my) {
    const float bar_y = popup_format_bar_y(entry.panel.height);
    const float window_w = entry.panel.width;
    StickyPanel& panel = entry.panel;
    const NoteTypography typo = note_typography_normalize(panel.session.note.typography);

    if (entry.format_picker == StickyPopupEntry::FormatPicker::Font) {
	float fx, fy, fw, fh;
	format_font_rect(bar_y, fx, fy, fw, fh);
	const float dd_h = static_cast<float>(kNoteFontCount) * kLineH + 8.0f;
	if (point_in_rect(mx, my, fx, fy - dd_h, fw, dd_h)) {
	    const int row = static_cast<int>((my - (fy - dd_h + 4.0f)) / kLineH);
	    if (row >= 0 && row < kNoteFontCount) {
		entry.format_picker_cursor = static_cast<std::size_t>(row);
		apply_format_picker(entry);
	    }
	    return true;
	}
	entry.format_picker = StickyPopupEntry::FormatPicker::None;
    } else if (entry.format_picker == StickyPopupEntry::FormatPicker::Size) {
	float fx, fy, fw, fh;
	format_size_rect(bar_y, fx, fy, fw, fh);
	const float dd_h = static_cast<float>(kTtfSizePresetCount) * kLineH + 8.0f;
	if (point_in_rect(mx, my, fx, fy - dd_h, fw, dd_h)) {
	    const int row = static_cast<int>((my - (fy - dd_h + 4.0f)) / kLineH);
	    if (row >= 0 && row < static_cast<int>(kTtfSizePresetCount)) {
		entry.format_picker_cursor = static_cast<std::size_t>(row);
		apply_format_picker(entry);
	    }
	    return true;
	}
	entry.format_picker = StickyPopupEntry::FormatPicker::None;
    }

    if (entry.show_theme_picker) {
	float fx, fy, fw, fh;
	format_theme_rect(window_w, bar_y, fx, fy, fw, fh);
	const float dd_h = kLineH + 4.0f + static_cast<float>(kThemeCount) * (kLineH + 4.0f) + 8.0f;
	if (point_in_rect(mx, my, fx, fy - dd_h, fw, dd_h)) {
	    float row_y = fy - dd_h + 4.0f + kLineH;
	    for (std::size_t i = 0; i < kThemeCount; ++i) {
		if (point_in_rect(mx, my, fx + 4.0f, row_y - 2.0f, fw - 8.0f, kLineH + 4.0f)) {
		    entry.theme_picker_cursor = i;
		    apply_theme(mgr, desk, i);
		    entry.show_theme_picker = false;
		    return true;
		}
		row_y += kLineH + 4.0f;
	    }
	    return true;
	}
	entry.show_theme_picker = false;
    }

    float fx, fy, fw, fh;
    format_font_rect(bar_y, fx, fy, fw, fh);
    if (point_in_rect(mx, my, fx, fy, fw, fh)) {
	entry.show_theme_picker = false;
	entry.format_picker = StickyPopupEntry::FormatPicker::Font;
	entry.format_picker_cursor = note_font_index(typo.font);
	return true;
    }
    format_size_rect(bar_y, fx, fy, fw, fh);
    if (point_in_rect(mx, my, fx, fy, fw, fh)) {
	entry.show_theme_picker = false;
	if (typo.font == NoteFontId::Debug) {
	    return true;
	}
	entry.format_picker = StickyPopupEntry::FormatPicker::Size;
	entry.format_picker_cursor = 0;
	for (std::size_t i = 0; i < kTtfSizePresetCount; ++i) {
	    if (kTtfSizePresets[i] == typo.size_px) {
		entry.format_picker_cursor = i;
		break;
	    }
	}
	return true;
    }
    const uint8_t style_flags[] = {TextStyleBold, TextStyleItalic, TextStyleUnderline,
				   TextStyleStrike};
    for (int i = 0; i < 4; ++i) {
	format_style_rect(bar_y, i, fx, fy, fw, fh);
	if (point_in_rect(mx, my, fx, fy, fw, fh)) {
	    entry.format_picker = StickyPopupEntry::FormatPicker::None;
	    entry.show_theme_picker = false;
	    editor_toggle_style_flag(panel.session, style_flags[i]);
	    return true;
	}
    }
    format_theme_rect(window_w, bar_y, fx, fy, fw, fh);
    if (point_in_rect(mx, my, fx, fy, fw, fh)) {
	entry.format_picker = StickyPopupEntry::FormatPicker::None;
	entry.show_theme_picker = !entry.show_theme_picker;
	entry.theme_picker_cursor = static_cast<std::size_t>(mgr.theme_id);
	return true;
    }

    // Clicks in the format-bar strip (but not on a control) still consume the event.
    if (my >= bar_y && my < entry.panel.height - kFormatBarBottomPad + 1.0f) {
	entry.format_picker = StickyPopupEntry::FormatPicker::None;
	entry.show_theme_picker = false;
	return true;
    }
    return false;
}

bool handle_popup_mouse(StickyPopupManager& mgr, std::size_t index, StickyGui& desk,
			const SDL_Event& event) {
    if (index >= mgr.entries.size()) {
	return false;
    }

    StickyPopupEntry& entry = mgr.entries[index];
    StickyPanel& panel = entry.panel;
    const float panel_w = panel.width;
    const float panel_h = panel.height;
    const float note_h = popup_note_height(panel_h);
    const float title_h = sticky_panel_title_bar_height();

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	const float mx = event.motion.x;
	const float my = event.motion.y;
	(void)mx;
	if (entry.body_scrollbar_dragging) {
	    textbox_scroll_body_from_thumb_y(panel.session, panel.viewport, 0.0f, 0.0f, panel_w,
					     note_h, my, entry.body_scrollbar_drag_offset_y);
	    return true;
	}
	if (entry.dragging) {
	    float gx = 0.0f;
	    float gy = 0.0f;
	    SDL_GetGlobalMouseState(&gx, &gy);
	    SDL_SetWindowPosition(
		entry.window, static_cast<int>(std::lround(gx - entry.drag_grab_offset_x)),
		static_cast<int>(std::lround(gy - entry.drag_grab_offset_y)));
	    return true;
	}
	if (entry.resizing) {
	    const float new_w = std::max(kMinPanelW, event.motion.x);
	    const float new_h = std::max(kMinPanelH, event.motion.y);
	    SDL_SetWindowSize(entry.window, static_cast<int>(new_w), static_cast<int>(new_h));
	    sync_panel_size_from_window(entry);
	    return true;
	}
	return false;
    }

    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP) {
	return false;
    }
    if (event.button.button != SDL_BUTTON_LEFT) {
	return false;
    }

    const float mx = event.button.x;
    const float my = event.button.y;

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
	if (entry.body_scrollbar_dragging) {
	    entry.body_scrollbar_dragging = false;
	    return true;
	}
	if (entry.dragging || entry.resizing) {
	    if (entry.resizing) {
		sync_panel_size_from_window(entry);
	    }
	    if (entry.dragging) {
		float gx = 0.0f;
		float gy = 0.0f;
		SDL_GetGlobalMouseState(&gx, &gy);
		const float dx = gx - entry.drag_global_start_x;
		const float dy = gy - entry.drag_global_start_y;
		if (dx * dx + dy * dy <= 16.0f && point_in_rect(mx, my, 0.0f, 0.0f, panel_w, title_h)
		    && handle_title_double_click(entry, SDL_GetTicks())) {
		    request_dock(mgr, index);
		}
	    }
	    entry.dragging = false;
	    entry.resizing = false;
	    return true;
	}
	return false;
    }

    float close_x = 0.0f;
    float close_y = 0.0f;
    float close_w = 0.0f;
    float close_h = 0.0f;
    close_button_rect(panel_w, close_x, close_y, close_w, close_h);
    if (point_in_rect(mx, my, close_x, close_y, close_w, close_h)) {
	close_popup_at(mgr, index);
	return true;
    }

    float dock_x = 0.0f;
    float dock_y = 0.0f;
    float dock_w = 0.0f;
    float dock_h = 0.0f;
    dock_button_rect(panel_w, dock_x, dock_y, dock_w, dock_h);
    if (point_in_rect(mx, my, dock_x, dock_y, dock_w, dock_h)) {
	request_dock(mgr, index);
	return true;
    }

    const float grip_x = panel_w - kResizeGrip;
    const float grip_y = panel_h - kResizeGrip;
    if (point_in_rect(mx, my, grip_x, grip_y, kResizeGrip, kResizeGrip)) {
	entry.resizing = true;
	return true;
    }

    if (handle_format_bar_mouse(mgr, entry, desk, mx, my)) {
	return true;
    }

    if (point_in_rect(mx, my, 0.0f, 0.0f, panel_w, title_h)) {
	entry.dragging = true;
	float gx = 0.0f;
	float gy = 0.0f;
	SDL_GetGlobalMouseState(&gx, &gy);
	int window_x = 0;
	int window_y = 0;
	SDL_GetWindowPosition(entry.window, &window_x, &window_y);
	entry.drag_grab_offset_x = gx - static_cast<float>(window_x);
	entry.drag_grab_offset_y = gy - static_cast<float>(window_y);
	entry.drag_global_start_x = gx;
	entry.drag_global_start_y = gy;
	return true;
    }

    if (textbox_point_in_body_scrollbar(panel.session, 0.0f, 0.0f, panel_w, note_h, mx, my)) {
	float thumb_x = 0.0f;
	float thumb_y = 0.0f;
	float thumb_w = 0.0f;
	float thumb_h = 0.0f;
	textbox_body_scrollbar_thumb_rect(panel.session, panel.viewport, 0.0f, 0.0f, panel_w, note_h,
					  thumb_x, thumb_y, thumb_w, thumb_h);
	if (!point_in_rect(mx, my, thumb_x, thumb_y, thumb_w, thumb_h)) {
	    entry.body_scrollbar_drag_offset_y = thumb_h * 0.5f;
	    textbox_scroll_body_from_thumb_y(panel.session, panel.viewport, 0.0f, 0.0f, panel_w,
					     note_h, my, entry.body_scrollbar_drag_offset_y);
	    textbox_body_scrollbar_thumb_rect(panel.session, panel.viewport, 0.0f, 0.0f, panel_w,
					      note_h, thumb_x, thumb_y, thumb_w, thumb_h);
	}
	entry.body_scrollbar_dragging = true;
	entry.body_scrollbar_drag_offset_y = my - thumb_y;
	return true;
    }

    if (textbox_handle_body_click(panel.session, panel.viewport, 0.0f, 0.0f, panel_w, note_h, mx, my,
				  (SDL_GetModState() & SDL_KMOD_SHIFT) != 0)) {
	return true;
    }

    return true;
}

bool handle_popup_keyboard(StickyPopupManager& mgr, std::size_t index, StickyGui& desk,
			   const SDL_Event& event) {
    if (index >= mgr.entries.size()) {
	return false;
    }
    StickyPopupEntry& entry = mgr.entries[index];
    sync_panel_size_from_window(entry);
    const float note_h = popup_note_height(entry.panel.height);

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
	const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
	if (ctrl && event.key.scancode == SDL_SCANCODE_T && !shift) {
	    apply_theme(mgr, desk, static_cast<std::size_t>(gui_theme_cycle(mgr.theme_id)));
	    return true;
	}
	if (ctrl && !shift
	    && (event.key.scancode == SDL_SCANCODE_1 || event.key.scancode == SDL_SCANCODE_2
		|| event.key.scancode == SDL_SCANCODE_3)) {
	    const std::size_t idx = static_cast<std::size_t>(event.key.scancode - SDL_SCANCODE_1);
	    apply_theme(mgr, desk, idx);
	    return true;
	}
	if (ctrl && event.key.scancode == SDL_SCANCODE_F && shift) {
	    entry.panel.session.note.typography =
		note_typography_cycle_font(entry.panel.session.note.typography);
	    textbox_enforce_wrap_width(entry.panel.session, textbox_body_wrap_width_for(entry.panel.width, entry.panel.session.note.typography), entry.panel.session.note.typography);
	    textbox_pin_viewport_to_start(entry.panel.viewport);
	    return true;
	}
	if (ctrl
	    && (event.key.scancode == SDL_SCANCODE_EQUALS || event.key.scancode == SDL_SCANCODE_KP_PLUS
		|| event.key.scancode == SDL_SCANCODE_MINUS
		|| event.key.scancode == SDL_SCANCODE_KP_MINUS)) {
	    const int delta =
		(event.key.scancode == SDL_SCANCODE_MINUS || event.key.scancode == SDL_SCANCODE_KP_MINUS)
		? -1
		: 1;
	    entry.panel.session.note.typography =
		note_typography_adjust_size(entry.panel.session.note.typography, delta);
	    textbox_enforce_wrap_width(entry.panel.session, textbox_body_wrap_width_for(entry.panel.width, entry.panel.session.note.typography), entry.panel.session.note.typography);
	    return true;
	}
	if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
	    if (entry.format_picker != StickyPopupEntry::FormatPicker::None
		|| entry.show_theme_picker) {
		entry.format_picker = StickyPopupEntry::FormatPicker::None;
		entry.show_theme_picker = false;
		return true;
	    }
	}
    }

    bool quit = false;
    const bool handled = textbox_handle_sdl_event_width(entry.panel.session, event, quit, textbox_body_wrap_width_for(entry.panel.width, entry.panel.session.note.typography), entry.panel.session.note.typography, false);
    if (handled) {
	textbox_scroll_to_cursor(
	    entry.panel.viewport, entry.panel.session,
	    textbox_visible_body_lines_for(note_h, entry.panel.session.note.typography));
    }
    return handled;
}
} // namespace

bool sticky_popup_open(StickyPopupManager& mgr, StickyPanel panel, GuiThemeId theme, int screen_x,
		       int screen_y) {
    panel.x = 0.0f;
    panel.y = 0.0f;
    panel.width = std::max(panel.width, kMinPanelW);
    panel.height = std::max(panel.height, kMinPanelH);

    const std::string title = panel.session.note.title.empty() ? "Untitled" : panel.session.note.title;
    const int w = static_cast<int>(panel.width);
    const int h = static_cast<int>(panel.height);

    SDL_Window* window = SDL_CreateWindow(
	title.c_str(), w, h,
	SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS);
    if (window == nullptr) {
	return false;
    }

    SDL_SetWindowPosition(window, screen_x, screen_y);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
	SDL_DestroyWindow(window);
	return false;
    }
    SDL_SetRenderVSync(renderer, 1);

    StickyPopupEntry entry{};
    entry.window = window;
    entry.renderer = renderer;
    entry.window_id = SDL_GetWindowID(window);
    entry.panel = std::move(panel);
    mgr.theme_id = theme;
    mgr.entries.push_back(std::move(entry));
    sync_panel_size_from_window(mgr.entries.back());
    SDL_StartTextInput(window);
    return true;
}

void sticky_popup_close_all(StickyPopupManager& mgr) {
    for (StickyPopupEntry& entry : mgr.entries) {
	persist_panel(entry.panel);
	destroy_entry(entry);
    }
    mgr.entries.clear();
}

void sticky_popup_save_all(const StickyPopupManager& mgr) {
    for (const StickyPopupEntry& entry : mgr.entries) {
	StickyPanel panel = entry.panel;
	persist_panel(panel);
    }
}

void sticky_popup_render_all(StickyPopupManager& mgr) {
    const StickyGuiTheme theme = gui_theme_get(mgr.theme_id);
    for (StickyPopupEntry& entry : mgr.entries) {
	// Repainting while SDL_SetWindowPosition runs leaves compositor trails on
	// borderless windows; the last frame moves with the window until drag ends.
	if (entry.dragging) {
	    continue;
	}

	sync_panel_size_from_window(entry);

	SDL_SetRenderDrawColor(entry.renderer, theme.desk.r, theme.desk.g, theme.desk.b, theme.desk.a);
	SDL_RenderClear(entry.renderer);

	const float note_h = popup_note_height(entry.panel.height);
	PanelChrome chrome{};
	chrome.show_close_button = true;
	chrome.show_dock_button = true;
	chrome.show_resize_grip = false;

	textbox_render_panel(entry.renderer, 0.0f, 0.0f, entry.panel.width, note_h,
			     entry.panel.session, entry.panel.viewport, true, chrome, theme);
	render_format_bar(entry.renderer, entry, theme, mgr.theme_id);

	// Resize grip in the window corner (below the note, on the format strip).
	const float grip = kResizeGrip;
	SDL_FRect resize_grip{entry.panel.width - grip, entry.panel.height - grip, grip, grip};
	gui_set_render_color(entry.renderer, theme.grip);
	SDL_RenderFillRect(entry.renderer, &resize_grip);

	SDL_RenderPresent(entry.renderer);
    }
}

bool sticky_popup_handle_event(StickyPopupManager& mgr, StickyGui& desk, const SDL_Event& event,
			       SDL_WindowID desk_window_id, bool& quit_app) {
    (void)desk_window_id;
    (void)quit_app;

    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
	SDL_Window* window = SDL_GetWindowFromID(event.window.windowID);
	if (window != nullptr) {
	    SDL_StartTextInput(window);
	}
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
	StickyPopupEntry* entry = find_entry(mgr, event.window.windowID);
	if (entry != nullptr) {
	    sync_panel_size_from_window(*entry);
	    return true;
	}
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
	for (std::size_t i = 0; i < mgr.entries.size(); ++i) {
	    if (mgr.entries[i].window_id == event.window.windowID) {
		close_popup_at(mgr, i);
		return true;
	    }
	}
    }

    SDL_WindowID target_id = 0;
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
	target_id = event.motion.windowID;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
	target_id = event.button.windowID;
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
	target_id = event.wheel.windowID;
    } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
	target_id = event.key.windowID;
    } else if (event.type == SDL_EVENT_TEXT_INPUT) {
	target_id = event.text.windowID;
    }

    if (target_id != 0) {
	for (std::size_t i = 0; i < mgr.entries.size(); ++i) {
	    if (mgr.entries[i].window_id != target_id) {
		continue;
	    }
	    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
		const float note_h = popup_note_height(mgr.entries[i].panel.height);
		return textbox_handle_mouse_wheel(mgr.entries[i].panel.session,
						  mgr.entries[i].panel.viewport, note_h,
						  event.wheel.y);
	    }
	    if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
		|| event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		return handle_popup_mouse(mgr, i, desk, event);
	    }
	    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP
		|| event.type == SDL_EVENT_TEXT_INPUT) {
		return handle_popup_keyboard(mgr, i, desk, event);
	    }
	}
    }

    return false;
}

std::size_t sticky_popup_count(const StickyPopupManager& mgr) {
    return mgr.entries.size();
}

bool sticky_popup_any_dragging(const StickyPopupManager& mgr) {
    for (const StickyPopupEntry& entry : mgr.entries) {
	if (entry.dragging) {
	    return true;
	}
    }
    return false;
}

bool sticky_popup_consume_dock_request(StickyPopupManager& mgr, StickyPanel& out_panel) {
    if (mgr.dock_request_index >= mgr.entries.size()) {
	return false;
    }
    const std::size_t index = mgr.dock_request_index;
    mgr.dock_request_index = static_cast<std::size_t>(-1);
    out_panel = std::move(mgr.entries[index].panel);
    destroy_entry(mgr.entries[index]);
    mgr.entries.erase(mgr.entries.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}
