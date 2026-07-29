#pragma once

// text_font — note body typography (platform-neutral metrics + labels).
// SDL drawing lives in text_font_render.cpp.

#include <cstddef>
#include <string>

enum class NoteFontId {
    Debug = 0, // SDL debug bitmap — default for new notes
    Sans = 1,  // DejaVu Sans
    Serif = 2, // DejaVu Serif
    Mono = 3,  // DejaVu Sans Mono
    Times = 4, // Liberation Serif (Times-compatible)
    Papyrus = 5, // IM Fell English (papyrus-like OFL)
    ArtDeco = 6, // Limelight (OFL)
};

constexpr int kNoteFontCount = 7;

constexpr int kDebugFontSizePx = 8;
constexpr int kTtfDefaultSizePx = 16;
constexpr int kTtfMinSizePx = 12;
constexpr int kTtfMaxSizePx = 48;

// Alias used by older call sites
constexpr int kSansDefaultSizePx = kTtfDefaultSizePx;
constexpr int kSansMinSizePx = kTtfMinSizePx;
constexpr int kSansMaxSizePx = kTtfMaxSizePx;

constexpr int kTtfSizePresets[] = {12, 14, 16, 18, 20, 24, 28, 32, 36, 48};
constexpr std::size_t kTtfSizePresetCount =
    sizeof(kTtfSizePresets) / sizeof(kTtfSizePresets[0]);

struct NoteTypography {
    NoteFontId font = NoteFontId::Debug;
    int size_px = kDebugFontSizePx; // Debug always renders at 8; TTF uses this (clamped)
};

NoteTypography note_typography_default();

NoteTypography note_typography_normalize(NoteTypography typo);

bool note_font_is_ttf(NoteFontId id);

const char* note_font_id_to_string(NoteFontId id);

const char* note_font_display_name(NoteFontId id);

bool note_font_id_from_string(const std::string& s, NoteFontId& out);

NoteFontId note_font_at_index(std::size_t index);

std::size_t note_font_index(NoteFontId id);

std::string note_typography_label(NoteTypography typo);

NoteFontId note_font_cycle(NoteFontId id);

NoteTypography note_typography_cycle_font(NoteTypography typo);

NoteTypography note_typography_set_font(NoteTypography typo, NoteFontId font);

NoteTypography note_typography_set_size(NoteTypography typo, int size_px);

NoteTypography note_typography_adjust_size(NoteTypography typo, int delta_steps);

int note_typography_nearest_preset_size(int size_px);

struct TextFontMetrics {
    float char_w = static_cast<float>(kDebugFontSizePx);
    float line_h = static_cast<float>(kDebugFontSizePx);
};

TextFontMetrics text_font_metrics(NoteTypography typo);
