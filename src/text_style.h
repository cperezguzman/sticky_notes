#pragma once

// text_style — character style runs / per-line flags for rich text.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum TextStyleFlag : uint8_t {
    TextStyleNone = 0,
    TextStyleBold = 1,
    TextStyleItalic = 2,
    TextStyleUnderline = 4,
    TextStyleStrike = 8,
};

struct StyleRun {
    std::size_t start = 0;  // flat index over storage lines (\n between lines)
    std::size_t length = 0;
    uint8_t flags = 0;
};

struct NoteStyles {
    // Parallel to sticky_note::text — styles_lines[i].size() == text[i].size()
    std::vector<std::vector<uint8_t>> lines;
};

void note_styles_clear(NoteStyles& styles);

void note_styles_ensure(NoteStyles& styles, const std::vector<std::string>& text);

uint8_t note_style_at(const NoteStyles& styles, std::size_t line, std::size_t col);

void note_styles_insert(NoteStyles& styles, std::size_t line, std::size_t col, std::size_t count,
			uint8_t flags);

void note_styles_erase(NoteStyles& styles, std::size_t line, std::size_t col, std::size_t count);

void note_styles_split_line(NoteStyles& styles, std::size_t line, std::size_t col);

void note_styles_join_with_previous(NoteStyles& styles, std::size_t line);

void note_styles_toggle_range(NoteStyles& styles, const std::vector<std::string>& text,
			      std::size_t a_line, std::size_t a_col, std::size_t b_line,
			      std::size_t b_col, uint8_t flag);

// Toggle flag on every char in [start, end) display range (inclusive lines).
void note_styles_apply_flag_range(NoteStyles& styles, const std::vector<std::string>& text,
				  std::size_t a_line, std::size_t a_col, std::size_t b_line,
				  std::size_t b_col, uint8_t flag, bool enable);

uint8_t note_styles_flags_in_range(const NoteStyles& styles, const std::vector<std::string>& text,
				   std::size_t a_line, std::size_t a_col, std::size_t b_line,
				   std::size_t b_col);

// Flatten / unflatten using \n between lines (like storage dump).
std::size_t note_styles_flat_index(const std::vector<std::string>& text, std::size_t line,
				   std::size_t col);

std::vector<StyleRun> note_styles_to_runs(const NoteStyles& styles,
					  const std::vector<std::string>& text);

void note_styles_from_runs(NoteStyles& styles, const std::vector<std::string>& text,
			   const std::vector<StyleRun>& runs);

std::string note_styles_flags_to_string(uint8_t flags);

bool note_styles_flags_from_string(const std::string& s, uint8_t& out);

bool note_styles_parse_run_line(const std::string& line, StyleRun& out);
