#include "text_font.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>

namespace {
std::string lower_copy(std::string s) {
    for (char& c : s) {
	c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}
} // namespace

NoteTypography note_typography_default() {
    return NoteTypography{};
}

bool note_font_is_ttf(NoteFontId id) {
    return id != NoteFontId::Debug;
}

NoteTypography note_typography_normalize(NoteTypography typo) {
    if (static_cast<int>(typo.font) < 0 || static_cast<int>(typo.font) >= kNoteFontCount) {
	typo.font = NoteFontId::Debug;
    }
    if (typo.font == NoteFontId::Debug) {
	typo.size_px = kDebugFontSizePx;
	return typo;
    }
    typo.size_px = note_typography_nearest_preset_size(typo.size_px);
    return typo;
}

const char* note_font_id_to_string(NoteFontId id) {
    switch (id) {
    case NoteFontId::Sans:
	return "sans";
    case NoteFontId::Serif:
	return "serif";
    case NoteFontId::Mono:
	return "mono";
    case NoteFontId::Times:
	return "times";
    case NoteFontId::Papyrus:
	return "papyrus";
    case NoteFontId::ArtDeco:
	return "artdeco";
    case NoteFontId::Debug:
    default:
	return "debug";
    }
}

const char* note_font_display_name(NoteFontId id) {
    switch (id) {
    case NoteFontId::Sans:
	return "Sans";
    case NoteFontId::Serif:
	return "Serif";
    case NoteFontId::Mono:
	return "Mono";
    case NoteFontId::Times:
	return "Times";
    case NoteFontId::Papyrus:
	return "Papyrus";
    case NoteFontId::ArtDeco:
	return "Art Deco";
    case NoteFontId::Debug:
    default:
	return "Debug";
    }
}

bool note_font_id_from_string(const std::string& s, NoteFontId& out) {
    const std::string key = lower_copy(s);
    if (key == "debug") {
	out = NoteFontId::Debug;
	return true;
    }
    if (key == "sans") {
	out = NoteFontId::Sans;
	return true;
    }
    if (key == "serif") {
	out = NoteFontId::Serif;
	return true;
    }
    if (key == "mono" || key == "monospace") {
	out = NoteFontId::Mono;
	return true;
    }
    if (key == "times" || key == "timesnewroman" || key == "liberation") {
	out = NoteFontId::Times;
	return true;
    }
    if (key == "papyrus" || key == "imfell" || key == "imfellenglish") {
	out = NoteFontId::Papyrus;
	return true;
    }
    if (key == "artdeco" || key == "art_deco" || key == "limelight") {
	out = NoteFontId::ArtDeco;
	return true;
    }
    return false;
}

NoteFontId note_font_at_index(std::size_t index) {
    if (index >= static_cast<std::size_t>(kNoteFontCount)) {
	return NoteFontId::Debug;
    }
    return static_cast<NoteFontId>(index);
}

std::size_t note_font_index(NoteFontId id) {
    const int v = static_cast<int>(id);
    if (v < 0 || v >= kNoteFontCount) {
	return 0;
    }
    return static_cast<std::size_t>(v);
}

std::string note_typography_label(NoteTypography typo) {
    typo = note_typography_normalize(typo);
    if (typo.font == NoteFontId::Debug) {
	return std::string("Font: ") + note_font_display_name(typo.font) + " 8x8";
    }
    return std::string("Font: ") + note_font_display_name(typo.font) + " " +
	   std::to_string(typo.size_px) + "px";
}

NoteFontId note_font_cycle(NoteFontId id) {
    const std::size_t next = (note_font_index(id) + 1) % static_cast<std::size_t>(kNoteFontCount);
    return note_font_at_index(next);
}

NoteTypography note_typography_cycle_font(NoteTypography typo) {
    return note_typography_set_font(typo, note_font_cycle(note_typography_normalize(typo).font));
}

NoteTypography note_typography_set_font(NoteTypography typo, NoteFontId font) {
    typo = note_typography_normalize(typo);
    typo.font = font;
    if (typo.font == NoteFontId::Debug) {
	typo.size_px = kDebugFontSizePx;
    } else if (typo.size_px <= kDebugFontSizePx) {
	typo.size_px = kTtfDefaultSizePx;
    }
    return note_typography_normalize(typo);
}

NoteTypography note_typography_set_size(NoteTypography typo, int size_px) {
    typo = note_typography_normalize(typo);
    if (typo.font == NoteFontId::Debug) {
	return typo;
    }
    typo.size_px = size_px;
    return note_typography_normalize(typo);
}

int note_typography_nearest_preset_size(int size_px) {
    if (size_px < kTtfMinSizePx) {
	return kTtfSizePresets[0];
    }
    if (size_px > kTtfMaxSizePx) {
	return kTtfSizePresets[kTtfSizePresetCount - 1];
    }
    int best = kTtfSizePresets[0];
    int best_dist = std::abs(size_px - best);
    for (std::size_t i = 1; i < kTtfSizePresetCount; ++i) {
	const int cand = kTtfSizePresets[i];
	const int dist = std::abs(size_px - cand);
	if (dist < best_dist) {
	    best = cand;
	    best_dist = dist;
	}
    }
    return best;
}

NoteTypography note_typography_adjust_size(NoteTypography typo, int delta_steps) {
    typo = note_typography_normalize(typo);
    if (typo.font == NoteFontId::Debug || delta_steps == 0) {
	return typo;
    }
    std::size_t idx = 0;
    for (std::size_t i = 0; i < kTtfSizePresetCount; ++i) {
	if (kTtfSizePresets[i] == typo.size_px) {
	    idx = i;
	    break;
	}
    }
    if (delta_steps > 0) {
	idx = std::min(idx + static_cast<std::size_t>(delta_steps), kTtfSizePresetCount - 1);
    } else {
	const std::size_t back = static_cast<std::size_t>(-delta_steps);
	idx = idx > back ? idx - back : 0;
    }
    typo.size_px = kTtfSizePresets[idx];
    return note_typography_normalize(typo);
}

TextFontMetrics text_font_metrics(NoteTypography typo) {
    typo = note_typography_normalize(typo);
    if (typo.font == NoteFontId::Debug) {
	return TextFontMetrics{static_cast<float>(kDebugFontSizePx),
			       static_cast<float>(kDebugFontSizePx)};
    }
    const float size = static_cast<float>(typo.size_px);
    return TextFontMetrics{std::ceil(size * 0.6f), std::ceil(size * 1.2f)};
}
