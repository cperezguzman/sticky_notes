#include "textbox_input.h"

#include "text_font_render.h"
#include "text_style.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>
#include <vector>

namespace {
bool is_printable_ascii(char32_t ch) {
    return ch >= 32 && ch <= 126;
}

std::size_t paragraph_start_line(const EditorSession& session, std::size_t line) {
    while (line > 0) {
	const std::size_t prev = line - 1;
	if (prev < session.hard_line_break_after.size()
	    && session.hard_line_break_after[prev]) {
	    break;
	}
	--line;
    }
    return line;
}

std::pair<std::size_t, std::size_t> save_cursor_paragraph(const EditorSession& session) {
    const std::size_t start = paragraph_start_line(session, session.current_line);
    std::size_t para = 0;
    for (std::size_t i = 0; i < start; ++i) {
	if (i < session.hard_line_break_after.size() && session.hard_line_break_after[i]) {
	    ++para;
	}
    }

    std::size_t off = 0;
    for (std::size_t i = start; i < session.current_line; ++i) {
	off += session.note.text[i].size();
    }
    off += session.current_column;
    return {para, off};
}

void restore_cursor_paragraph(EditorSession& session, std::size_t target_para, std::size_t target_off) {
    if (session.note.text.empty()) {
	session.current_line = 0;
	session.current_column = 0;
	return;
    }

    std::size_t para = 0;
    std::size_t off = 0;
    for (std::size_t i = 0; i < session.note.text.size(); ++i) {
	const std::size_t len = session.note.text[i].size();
	if (para == target_para && target_off >= off && target_off <= off + len) {
	    session.current_line = i;
	    session.current_column = target_off - off;
	    return;
	}

	off += len;

	const bool hard = i < session.hard_line_break_after.size()
	    && session.hard_line_break_after[i];
	if (hard) {
	    if (para == target_para && target_off == off) {
		session.current_line = i;
		session.current_column = len;
		return;
	    }
	    ++para;
	    off = 0;
	}
    }

    session.current_line = session.note.text.size() - 1;
    session.current_column = session.note.text.back().size();
}
} // namespace

void textbox_init_hard_breaks_for_loaded_note(EditorSession& session) {
    const std::size_t n = session.note.text.size();
    session.hard_line_break_after.assign(n, false);
    for (std::size_t i = 0; i + 1 < n; ++i) {
	session.hard_line_break_after[i] = true;
    }
}

void textbox_repair_persisted_soft_wraps(std::vector<std::string>& lines) {
    if (lines.size() < 2) {
	return;
    }

    std::vector<std::string> out;
    out.reserve(lines.size());
    out.push_back(lines[0]);

    for (std::size_t i = 1; i < lines.size(); ++i) {
	const std::string& cur = lines[i];
	std::string& prev = out.back();
	if (prev.empty() || cur.empty()) {
	    out.push_back(cur);
	    continue;
	}

	const unsigned char prev_last = static_cast<unsigned char>(prev.back());
	const unsigned char cur_first = static_cast<unsigned char>(cur.front());
	const bool mid_word = std::isalnum(prev_last) != 0 && std::isalnum(cur_first) != 0;
	const bool after_space = prev_last == ' ';
	const bool leading_space = cur_first == ' ';
	constexpr std::size_t kLikelyWrapCols = 60;
	const bool wrapped_paragraph = prev.size() >= kLikelyWrapCols
	    && std::islower(cur_first) != 0;
	if (mid_word || after_space || leading_space || wrapped_paragraph) {
	    prev += cur;
	} else {
	    out.push_back(cur);
	}
    }

    lines = std::move(out);
}

std::vector<std::string> textbox_storage_lines(const EditorSession& session) {
    std::vector<std::string> out;
    if (session.note.text.empty()) {
	out.emplace_back("");
	return out;
    }

    const bool have_breaks = session.hard_line_break_after.size() == session.note.text.size();
    if (!have_breaks) {
	return session.note.text;
    }

    std::string para;
    for (std::size_t i = 0; i < session.note.text.size(); ++i) {
	para += session.note.text[i];
	const bool hard = session.hard_line_break_after[i];
	const bool last = i + 1 >= session.note.text.size();
	if (hard || last) {
	    out.push_back(para);
	    para.clear();
	}
    }
    if (out.empty()) {
	out.emplace_back("");
    }
    return out;
}

NoteStyles textbox_storage_styles(const EditorSession& session) {
    NoteStyles styles = session.note.styles;
    note_styles_ensure(styles, session.note.text);
    if (session.note.text.empty()) {
	NoteStyles out;
	out.lines.emplace_back();
	return out;
    }
    const bool have_breaks = session.hard_line_break_after.size() == session.note.text.size();
    if (!have_breaks) {
	return styles;
    }

    NoteStyles out;
    std::vector<uint8_t> para;
    for (std::size_t i = 0; i < session.note.text.size(); ++i) {
	const auto& row = styles.lines[i];
	para.insert(para.end(), row.begin(), row.end());
	const bool hard = session.hard_line_break_after[i];
	const bool last = i + 1 >= session.note.text.size();
	if (hard || last) {
	    out.lines.push_back(std::move(para));
	    para.clear();
	}
    }
    if (out.lines.empty()) {
	out.lines.emplace_back();
    }
    return out;
}

void textbox_enforce_wrap(EditorSession& session, std::size_t max_columns) {
    if (max_columns == 0) {
	return;
    }
    const NoteTypography typo = note_typography_normalize(session.note.typography);
    const float max_width =
	static_cast<float>(max_columns) * text_font_metrics(typo).char_w;
    textbox_enforce_wrap_width(session, max_width, typo);
}

namespace {
std::size_t wrap_break_length(const std::string& para, const std::vector<uint8_t>& para_styles,
			      std::size_t pos, float max_width_px, NoteTypography typo) {
    const std::size_t remain = para.size() - pos;
    if (remain == 0) {
	return 0;
    }
    const uint8_t* styles =
	para_styles.size() >= para.size() ? para_styles.data() + static_cast<std::ptrdiff_t>(pos)
					  : nullptr;

    // Largest prefix of para[pos..] that fits in max_width_px.
    std::size_t lo = 1;
    std::size_t hi = remain;
    std::size_t fit = 1;
    while (lo <= hi) {
	const std::size_t mid = lo + (hi - lo) / 2;
	const float w = text_font_prefix_width(nullptr, typo, para.c_str() + pos, mid, styles, mid);
	if (w <= max_width_px) {
	    fit = mid;
	    lo = mid + 1;
	} else {
	    if (mid == 1) {
		fit = 1; // always emit at least one character
		break;
	    }
	    hi = mid - 1;
	}
    }

    // Prefer breaking after the last space within the fitted span.
    const std::size_t space = para.rfind(' ', pos + fit - 1);
    if (space != std::string::npos && space >= pos) {
	const std::size_t after_space = space - pos + 1;
	if (after_space > 0 && after_space <= fit) {
	    return after_space;
	}
    }
    return fit;
}
} // namespace

void textbox_enforce_wrap_width(EditorSession& session, float max_width_px, NoteTypography typo) {
    if (max_width_px <= 0.0f) {
	return;
    }
    typo = note_typography_normalize(typo);
    if (session.note.text.empty()) {
	session.note.text.push_back("");
    }
    if (session.hard_line_break_after.size() != session.note.text.size()) {
	textbox_init_hard_breaks_for_loaded_note(session);
    }
    note_styles_ensure(session.note.styles, session.note.text);

    const auto [target_para, target_off] = save_cursor_paragraph(session);

    std::vector<std::string> new_lines;
    std::vector<std::vector<uint8_t>> new_styles;
    std::vector<bool> new_breaks;
    new_lines.reserve(session.note.text.size());
    new_styles.reserve(session.note.text.size());
    new_breaks.reserve(session.note.text.size());

    std::size_t i = 0;
    while (i < session.note.text.size()) {
	std::string para = session.note.text[i];
	std::vector<uint8_t> para_styles = session.note.styles.lines[i];
	bool hard_end = i < session.hard_line_break_after.size()
	    ? session.hard_line_break_after[i]
	    : false;
	std::size_t j = i;
	while (j + 1 < session.note.text.size()) {
	    const bool br = j < session.hard_line_break_after.size()
		? session.hard_line_break_after[j]
		: false;
	    if (br) {
		break;
	    }
	    para += session.note.text[j + 1];
	    para_styles.insert(para_styles.end(), session.note.styles.lines[j + 1].begin(),
			       session.note.styles.lines[j + 1].end());
	    hard_end = j + 1 < session.hard_line_break_after.size()
		? session.hard_line_break_after[j + 1]
		: false;
	    ++j;
	}

	if (para.empty()) {
	    new_lines.emplace_back("");
	    new_styles.emplace_back();
	    new_breaks.push_back(hard_end);
	} else {
	    std::size_t pos = 0;
	    while (pos < para.size()) {
		const std::size_t break_at =
		    wrap_break_length(para, para_styles, pos, max_width_px, typo);
		if (break_at == 0) {
		    break;
		}
		const bool last_chunk = pos + break_at >= para.size();
		new_lines.push_back(para.substr(pos, break_at));
		new_styles.emplace_back(
		    para_styles.begin() + static_cast<std::ptrdiff_t>(pos),
		    para_styles.begin() + static_cast<std::ptrdiff_t>(pos + break_at));
		new_breaks.push_back(last_chunk ? hard_end : false);
		pos += break_at;
	    }
	}
	i = j + 1;
    }

    if (new_lines.empty()) {
	new_lines.emplace_back("");
	new_styles.emplace_back();
	new_breaks.push_back(false);
    }

    session.note.text = std::move(new_lines);
    session.note.styles.lines = std::move(new_styles);
    session.hard_line_break_after = std::move(new_breaks);
    note_styles_ensure(session.note.styles, session.note.text);
    restore_cursor_paragraph(session, target_para, target_off);
}

void textbox_init_session(EditorSession& session) {
    session = EditorSession{};
    session.note.text.clear();
    session.note.text.push_back("");
    session.hard_line_break_after = {false};
    session.current_line = 0;
    session.current_column = 0;
    editor_clear_history(session);
}

bool textbox_apply_key(EditorSession& session, TextboxKeyEvent event, std::size_t max_columns) {
    if (session.note.text.empty()) {
	textbox_init_session(session);
    }

    bool handled = false;
    bool mutates = false;
    switch (event.kind) {
    case TextboxKeyKind::Character:
	if (!is_printable_ascii(event.character)) {
	    return false;
	}
	insert_at_cursor(session, std::string(1, static_cast<char>(event.character)));
	handled = true;
	mutates = true;
	break;

    case TextboxKeyKind::Backspace:
	if (session.current_column == 0 && session.current_line > 0) {
	    handled = join_with_previous_line(session) == EditStatus::Ok;
	} else {
	    handled = erase_char_before(session) == EditStatus::Ok;
	}
	mutates = handled;
	break;

    case TextboxKeyKind::Delete:
	handled = delete_at_cursor(session) == EditStatus::Ok;
	mutates = handled;
	break;

    case TextboxKeyKind::Left:
	move_left(session);
	handled = true;
	break;

    case TextboxKeyKind::Right:
	move_right(session);
	handled = true;
	break;

    case TextboxKeyKind::Up:
	move_up(session);
	handled = true;
	break;

    case TextboxKeyKind::Down:
	move_down(session);
	handled = true;
	break;

    case TextboxKeyKind::Home:
	handled = move_home(session) == EditStatus::Ok;
	break;

    case TextboxKeyKind::End:
	handled = move_end(session) == EditStatus::Ok;
	break;

    case TextboxKeyKind::Newline:
	insert_newline_at_cursor(session);
	handled = true;
	mutates = true;
	break;

    default:
	return false;
    }

    // Navigation / selection must not re-wrap — KEY_DOWN repeats would backlog for seconds.
    if (handled && mutates && max_columns > 0) {
	textbox_enforce_wrap(session, max_columns);
    }
    return handled;
}

bool textbox_apply_key_width(EditorSession& session, TextboxKeyEvent event, float max_width_px,
			     NoteTypography typo) {
    if (session.note.text.empty()) {
	textbox_init_session(session);
    }

    const bool mutates = event.kind == TextboxKeyKind::Character
	|| event.kind == TextboxKeyKind::Backspace || event.kind == TextboxKeyKind::Delete
	|| event.kind == TextboxKeyKind::Newline;
    const bool handled = textbox_apply_key(session, event, 0);
    if (handled && mutates && max_width_px > 0.0f) {
	textbox_enforce_wrap_width(session, max_width_px, typo);
    }
    return handled;
}

std::size_t textbox_line_count(const EditorSession& session) {
    return session.note.text.size();
}

std::string textbox_line_at(const EditorSession& session, std::size_t line_index) {
    if (line_index >= session.note.text.size()) {
	return "";
    }
    return session.note.text[line_index];
}

std::size_t textbox_cursor_line(const EditorSession& session) {
    return session.current_line;
}

std::size_t textbox_cursor_column(const EditorSession& session) {
    return session.current_column;
}

std::string textbox_line_text(const EditorSession& session) {
    return textbox_line_at(session, textbox_cursor_line(session));
}

void textbox_clamp_viewport(TextboxViewport& viewport, const EditorSession& session,
			    std::size_t visible_line_count) {
    if (visible_line_count == 0) {
	visible_line_count = 1;
    }

    const std::size_t line_count = textbox_line_count(session);
    if (line_count <= visible_line_count) {
	viewport.first_visible_line = 0;
	return;
    }
    const std::size_t max_first = line_count - visible_line_count;
    if (viewport.first_visible_line > max_first) {
	viewport.first_visible_line = max_first;
    }
}

void textbox_pin_viewport_to_start(TextboxViewport& viewport) {
    viewport.first_visible_line = 0;
}

void textbox_set_cursor(EditorSession& session, std::size_t line, std::size_t column) {
    if (session.note.text.empty()) {
	session.note.text.push_back("");
	session.hard_line_break_after = {false};
    }
    if (line >= session.note.text.size()) {
	line = session.note.text.size() - 1;
    }
    session.current_line = line;
    const std::size_t len = session.note.text[line].size();
    session.current_column = std::min(column, len);
}

void textbox_click_body(EditorSession& session, TextboxViewport& viewport,
			std::size_t visible_line_count, float local_x, float local_y, float char_w,
			float line_h, float padding) {
    if (char_w <= 0.0f || line_h <= 0.0f) {
	return;
    }
    if (visible_line_count == 0) {
	visible_line_count = 1;
    }

    const float x = local_x - padding;
    const float y = local_y - padding;
    int row_in_view = static_cast<int>(std::floor(y / line_h));
    if (row_in_view < 0) {
	row_in_view = 0;
    }
    int col = static_cast<int>(std::floor(x / char_w));
    if (col < 0) {
	col = 0;
    }

    const std::size_t line_count = textbox_line_count(session);
    std::size_t line = viewport.first_visible_line + static_cast<std::size_t>(row_in_view);
    if (line_count == 0) {
	textbox_set_cursor(session, 0, 0);
	return;
    }
    if (line >= line_count) {
	line = line_count - 1;
    }
    // Click past the last visible row still lands on the last shown line.
    const std::size_t last_visible =
	std::min(viewport.first_visible_line + visible_line_count, line_count) - 1;
    if (line > last_visible) {
	line = last_visible;
    }
    textbox_set_cursor(session, line, static_cast<std::size_t>(col));
}

bool textbox_scroll_lines(TextboxViewport& viewport, const EditorSession& session,
			  std::size_t visible_line_count, int delta_lines) {
    if (delta_lines == 0) {
	return false;
    }
    textbox_clamp_viewport(viewport, session, visible_line_count);
    if (visible_line_count == 0) {
	visible_line_count = 1;
    }
    const std::size_t line_count = textbox_line_count(session);
    if (line_count <= visible_line_count) {
	const bool changed = viewport.first_visible_line != 0;
	viewport.first_visible_line = 0;
	return changed;
    }
    const std::size_t max_first = line_count - visible_line_count;
    const std::size_t before = viewport.first_visible_line;
    if (delta_lines < 0) {
	const std::size_t up = static_cast<std::size_t>(-delta_lines);
	viewport.first_visible_line = before > up ? before - up : 0;
    } else {
	const std::size_t down = static_cast<std::size_t>(delta_lines);
	viewport.first_visible_line =
	    before + down > max_first ? max_first : before + down;
    }
    return viewport.first_visible_line != before;
}

std::size_t textbox_max_first_visible(const EditorSession& session,
				      std::size_t visible_line_count) {
    if (visible_line_count == 0) {
	visible_line_count = 1;
    }
    const std::size_t line_count = textbox_line_count(session);
    if (line_count <= visible_line_count) {
	return 0;
    }
    return line_count - visible_line_count;
}

bool textbox_set_first_visible(TextboxViewport& viewport, const EditorSession& session,
			       std::size_t visible_line_count, std::size_t first) {
    const std::size_t before = viewport.first_visible_line;
    viewport.first_visible_line = first;
    textbox_clamp_viewport(viewport, session, visible_line_count);
    return viewport.first_visible_line != before;
}

void textbox_scroll_to_cursor(TextboxViewport& viewport, const EditorSession& session,
			      std::size_t visible_line_count) {
    textbox_clamp_viewport(viewport, session, visible_line_count);
    if (visible_line_count == 0) {
	visible_line_count = 1;
    }

    const std::size_t line_count = textbox_line_count(session);
    if (line_count <= visible_line_count) {
	return;
    }

    const std::size_t cursor_line = textbox_cursor_line(session);
    if (cursor_line < viewport.first_visible_line) {
	viewport.first_visible_line = cursor_line;
	return;
    }

    const std::size_t last_visible = viewport.first_visible_line + visible_line_count - 1;
    if (cursor_line > last_visible) {
	viewport.first_visible_line = cursor_line - visible_line_count + 1;
    }
}
