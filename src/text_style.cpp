#include "text_style.h"

#include <algorithm>
#include <cctype>
#include <sstream>

void note_styles_clear(NoteStyles& styles) {
    styles.lines.clear();
}

void note_styles_ensure(NoteStyles& styles, const std::vector<std::string>& text) {
    styles.lines.resize(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
	if (styles.lines[i].size() < text[i].size()) {
	    styles.lines[i].resize(text[i].size(), TextStyleNone);
	} else if (styles.lines[i].size() > text[i].size()) {
	    styles.lines[i].resize(text[i].size());
	}
    }
}

uint8_t note_style_at(const NoteStyles& styles, std::size_t line, std::size_t col) {
    if (line >= styles.lines.size() || col >= styles.lines[line].size()) {
	return TextStyleNone;
    }
    return styles.lines[line][col];
}

void note_styles_insert(NoteStyles& styles, std::size_t line, std::size_t col, std::size_t count,
			uint8_t flags) {
    if (count == 0) {
	return;
    }
    if (line >= styles.lines.size()) {
	styles.lines.resize(line + 1);
    }
    auto& row = styles.lines[line];
    if (col > row.size()) {
	row.resize(col, TextStyleNone);
    }
    row.insert(row.begin() + static_cast<std::ptrdiff_t>(col), count, flags);
}

void note_styles_erase(NoteStyles& styles, std::size_t line, std::size_t col, std::size_t count) {
    if (count == 0 || line >= styles.lines.size()) {
	return;
    }
    auto& row = styles.lines[line];
    if (col >= row.size()) {
	return;
    }
    const std::size_t n = std::min(count, row.size() - col);
    row.erase(row.begin() + static_cast<std::ptrdiff_t>(col),
	      row.begin() + static_cast<std::ptrdiff_t>(col + n));
}

void note_styles_split_line(NoteStyles& styles, std::size_t line, std::size_t col) {
    if (line >= styles.lines.size()) {
	styles.lines.resize(line + 1);
    }
    auto& row = styles.lines[line];
    if (col > row.size()) {
	row.resize(col, TextStyleNone);
    }
    std::vector<uint8_t> rest(row.begin() + static_cast<std::ptrdiff_t>(col), row.end());
    row.resize(col);
    styles.lines.insert(styles.lines.begin() + static_cast<std::ptrdiff_t>(line + 1),
			std::move(rest));
}

void note_styles_join_with_previous(NoteStyles& styles, std::size_t line) {
    if (line == 0 || line >= styles.lines.size()) {
	return;
    }
    auto& prev = styles.lines[line - 1];
    prev.insert(prev.end(), styles.lines[line].begin(), styles.lines[line].end());
    styles.lines.erase(styles.lines.begin() + static_cast<std::ptrdiff_t>(line));
}

namespace {
void normalize_range(const std::vector<std::string>& text, std::size_t& a_line, std::size_t& a_col,
		     std::size_t& b_line, std::size_t& b_col) {
    if (text.empty()) {
	a_line = b_line = 0;
	a_col = b_col = 0;
	return;
    }
    a_line = std::min(a_line, text.size() - 1);
    b_line = std::min(b_line, text.size() - 1);
    a_col = std::min(a_col, text[a_line].size());
    b_col = std::min(b_col, text[b_line].size());
    if (a_line > b_line || (a_line == b_line && a_col > b_col)) {
	std::swap(a_line, b_line);
	std::swap(a_col, b_col);
    }
}
} // namespace

void note_styles_apply_flag_range(NoteStyles& styles, const std::vector<std::string>& text,
				  std::size_t a_line, std::size_t a_col, std::size_t b_line,
				  std::size_t b_col, uint8_t flag, bool enable) {
    note_styles_ensure(styles, text);
    normalize_range(text, a_line, a_col, b_line, b_col);
    for (std::size_t line = a_line; line <= b_line; ++line) {
	const std::size_t start = (line == a_line) ? a_col : 0;
	const std::size_t end = (line == b_line) ? b_col : text[line].size();
	for (std::size_t col = start; col < end; ++col) {
	    if (enable) {
		styles.lines[line][col] =
		    static_cast<uint8_t>(styles.lines[line][col] | flag);
	    } else {
		styles.lines[line][col] =
		    static_cast<uint8_t>(styles.lines[line][col] & static_cast<uint8_t>(~flag));
	    }
	}
    }
}

uint8_t note_styles_flags_in_range(const NoteStyles& styles, const std::vector<std::string>& text,
				   std::size_t a_line, std::size_t a_col, std::size_t b_line,
				   std::size_t b_col) {
    if (text.empty()) {
	return TextStyleNone;
    }
    normalize_range(text, a_line, a_col, b_line, b_col);
    if (a_line == b_line && a_col == b_col) {
	return TextStyleNone;
    }
    uint8_t common = 0xFF;
    bool any = false;
    for (std::size_t line = a_line; line <= b_line; ++line) {
	const std::size_t start = (line == a_line) ? a_col : 0;
	const std::size_t end = (line == b_line) ? b_col : text[line].size();
	for (std::size_t col = start; col < end; ++col) {
	    common = static_cast<uint8_t>(common & note_style_at(styles, line, col));
	    any = true;
	}
    }
    return any ? common : static_cast<uint8_t>(TextStyleNone);
}

void note_styles_toggle_range(NoteStyles& styles, const std::vector<std::string>& text,
			      std::size_t a_line, std::size_t a_col, std::size_t b_line,
			      std::size_t b_col, uint8_t flag) {
    const uint8_t common =
	note_styles_flags_in_range(styles, text, a_line, a_col, b_line, b_col);
    const bool enable = (common & flag) == 0;
    note_styles_apply_flag_range(styles, text, a_line, a_col, b_line, b_col, flag, enable);
}

std::size_t note_styles_flat_index(const std::vector<std::string>& text, std::size_t line,
				   std::size_t col) {
    std::size_t flat = 0;
    const std::size_t lim = std::min(line, text.size());
    for (std::size_t i = 0; i < lim; ++i) {
	flat += text[i].size() + 1; // +1 for newline
    }
    if (line < text.size()) {
	flat += std::min(col, text[line].size());
    }
    return flat;
}

std::vector<StyleRun> note_styles_to_runs(const NoteStyles& styles,
					  const std::vector<std::string>& text) {
    std::vector<StyleRun> runs;
    std::size_t flat = 0;
    uint8_t cur = TextStyleNone;
    std::size_t run_start = 0;
    bool in_run = false;

    auto flush = [&](std::size_t end) {
	if (in_run && cur != TextStyleNone && end > run_start) {
	    runs.push_back(StyleRun{run_start, end - run_start, cur});
	}
	in_run = false;
	cur = TextStyleNone;
    };

    for (std::size_t line = 0; line < text.size(); ++line) {
	for (std::size_t col = 0; col < text[line].size(); ++col) {
	    const uint8_t f = note_style_at(styles, line, col);
	    if (!in_run) {
		run_start = flat;
		cur = f;
		in_run = true;
	    } else if (f != cur) {
		flush(flat);
		run_start = flat;
		cur = f;
		in_run = true;
	    }
	    ++flat;
	}
	if (line + 1 < text.size()) {
	    flush(flat);
	    ++flat; // newline has no style
	}
    }
    flush(flat);
    return runs;
}

void note_styles_from_runs(NoteStyles& styles, const std::vector<std::string>& text,
			   const std::vector<StyleRun>& runs) {
    note_styles_clear(styles);
    note_styles_ensure(styles, text);
    for (const StyleRun& run : runs) {
	if (run.length == 0 || run.flags == TextStyleNone) {
	    continue;
	}
	std::size_t flat = 0;
	for (std::size_t line = 0; line < text.size(); ++line) {
	    for (std::size_t col = 0; col < text[line].size(); ++col) {
		if (flat >= run.start && flat < run.start + run.length) {
		    styles.lines[line][col] =
			static_cast<uint8_t>(styles.lines[line][col] | run.flags);
		}
		++flat;
	    }
	    if (line + 1 < text.size()) {
		++flat;
	    }
	}
    }
}

std::string note_styles_flags_to_string(uint8_t flags) {
    std::string s;
    if (flags & TextStyleBold) {
	s.push_back('b');
    }
    if (flags & TextStyleItalic) {
	s.push_back('i');
    }
    if (flags & TextStyleUnderline) {
	s.push_back('u');
    }
    if (flags & TextStyleStrike) {
	s.push_back('s');
    }
    return s;
}

bool note_styles_flags_from_string(const std::string& s, uint8_t& out) {
    out = TextStyleNone;
    for (char c : s) {
	const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	switch (lc) {
	case 'b':
	    out = static_cast<uint8_t>(out | TextStyleBold);
	    break;
	case 'i':
	    out = static_cast<uint8_t>(out | TextStyleItalic);
	    break;
	case 'u':
	    out = static_cast<uint8_t>(out | TextStyleUnderline);
	    break;
	case 's':
	    out = static_cast<uint8_t>(out | TextStyleStrike);
	    break;
	default:
	    return false;
	}
    }
    return true;
}

bool note_styles_parse_run_line(const std::string& line, StyleRun& out) {
    std::istringstream in(line);
    std::size_t start = 0;
    std::size_t length = 0;
    std::string flags;
    if (!(in >> start >> length >> flags)) {
	return false;
    }
    uint8_t f = 0;
    if (!note_styles_flags_from_string(flags, f)) {
	return false;
    }
    out = StyleRun{start, length, f};
    return true;
}
