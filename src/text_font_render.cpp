#define STB_TRUETYPE_IMPLEMENTATION
#include "text_font_render.h"

#include "../vendor/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct AtlasKey {
    NoteFontId font = NoteFontId::Sans;
    int size_px = kTtfDefaultSizePx;
    uint8_t face_flags = 0; // Bold | Italic only
    SDL_Renderer* renderer = nullptr;

    bool operator==(const AtlasKey& o) const {
	return font == o.font && size_px == o.size_px && face_flags == o.face_flags
	    && renderer == o.renderer;
    }
};

struct AtlasKeyHash {
    std::size_t operator()(const AtlasKey& k) const {
	return (static_cast<std::size_t>(k.font) << 20)
	    ^ (static_cast<std::size_t>(k.size_px) << 4)
	    ^ static_cast<std::size_t>(k.face_flags)
	    ^ (reinterpret_cast<std::size_t>(k.renderer) >> 4);
    }
};

struct FontAtlas {
    SDL_Texture* texture = nullptr;
    stbtt_bakedchar chars[96]{};
    float char_w = 16.0f;
    float line_h = 16.0f;
    int atlas_w = 512;
    int atlas_h = 512;
};

struct TtfCache {
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool attempted = false;
    bool ok = false;
    bool info_ok = false;
};

std::unordered_map<AtlasKey, FontAtlas, AtlasKeyHash> g_atlases;
std::unordered_map<std::string, TtfCache> g_ttf_files;

TtfCache* get_ttf_cache(const std::string& path) {
    TtfCache& cache = g_ttf_files[path];
    if (!cache.attempted) {
	cache.attempted = true;
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	if (!in) {
	    cache.ok = false;
	    return &cache;
	}
	const auto n = static_cast<std::size_t>(in.tellg());
	in.seekg(0);
	cache.bytes.resize(n);
	in.read(reinterpret_cast<char*>(cache.bytes.data()), static_cast<std::streamsize>(n));
	cache.ok = n > 0;
	if (cache.ok) {
	    cache.info_ok = stbtt_InitFont(&cache.info, cache.bytes.data(), 0) != 0;
	}
    }
    return &cache;
}

std::string face_path_for(NoteFontId font, bool bold, bool italic) {
    // Prefer bundled assets/; fall back to common system paths.
    // Cache results — wrap measures call this per glyph and fopen was dominating (~50ms/wrap).
    static std::unordered_map<int, std::string> path_cache;
    const int key = (static_cast<int>(font) << 2) | (bold ? 2 : 0) | (italic ? 1 : 0);
    const auto cached = path_cache.find(key);
    if (cached != path_cache.end()) {
	return cached->second;
    }

    auto try_list = [](std::initializer_list<const char*> paths) -> std::string {
	for (const char* path : paths) {
	    std::ifstream in(path, std::ios::binary);
	    if (in.good()) {
		return path;
	    }
	}
	return {};
    };

    std::string found;
    switch (font) {
    case NoteFontId::Sans:
	if (bold && italic) {
	    found = try_list({"assets/fonts/DejaVuSans-BoldOblique.ttf",
			      "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf",
			      "assets/fonts/DejaVuSans-Bold.ttf"});
	} else if (bold) {
	    found = try_list({"assets/fonts/DejaVuSans-Bold.ttf",
			      "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
			      "assets/fonts/DejaVuSans.ttf"});
	} else if (italic) {
	    found = try_list({"assets/fonts/DejaVuSans-Oblique.ttf",
			      "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf",
			      "assets/fonts/DejaVuSans.ttf"});
	} else {
	    found = try_list({"assets/fonts/DejaVuSans.ttf",
			      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"});
	}
	break;
    case NoteFontId::Serif:
	if (bold && italic) {
	    found = try_list(
		{"assets/fonts/DejaVuSerif-BoldItalic.ttf", "assets/fonts/DejaVuSerif-Bold.ttf"});
	} else if (bold) {
	    found = try_list({"assets/fonts/DejaVuSerif-Bold.ttf", "assets/fonts/DejaVuSerif.ttf"});
	} else if (italic) {
	    found = try_list({"assets/fonts/DejaVuSerif-Italic.ttf", "assets/fonts/DejaVuSerif.ttf"});
	} else {
	    found = try_list({"assets/fonts/DejaVuSerif.ttf"});
	}
	break;
    case NoteFontId::Mono:
	if (bold && italic) {
	    found = try_list({"assets/fonts/DejaVuSansMono-BoldOblique.ttf",
			      "assets/fonts/DejaVuSansMono-Bold.ttf"});
	} else if (bold) {
	    found = try_list(
		{"assets/fonts/DejaVuSansMono-Bold.ttf", "assets/fonts/DejaVuSansMono.ttf"});
	} else if (italic) {
	    found = try_list(
		{"assets/fonts/DejaVuSansMono-Oblique.ttf", "assets/fonts/DejaVuSansMono.ttf"});
	} else {
	    found = try_list({"assets/fonts/DejaVuSansMono.ttf"});
	}
	break;
    case NoteFontId::Times:
	if (bold && italic) {
	    found = try_list({"assets/fonts/LiberationSerif-BoldItalic.ttf",
			      "assets/fonts/LiberationSerif-Bold.ttf"});
	} else if (bold) {
	    found = try_list({"assets/fonts/LiberationSerif-Bold.ttf",
			      "assets/fonts/LiberationSerif-Regular.ttf"});
	} else if (italic) {
	    found = try_list({"assets/fonts/LiberationSerif-Italic.ttf",
			      "assets/fonts/LiberationSerif-Regular.ttf"});
	} else {
	    found = try_list({"assets/fonts/LiberationSerif-Regular.ttf"});
	}
	break;
    case NoteFontId::Papyrus:
	if (italic) {
	    found = try_list(
		{"assets/fonts/IMFellEnglish-Italic.ttf", "assets/fonts/IMFellEnglish-Regular.ttf"});
	} else {
	    found = try_list({"assets/fonts/IMFellEnglish-Regular.ttf"});
	}
	break;
    case NoteFontId::ArtDeco:
	found = try_list({"assets/fonts/Limelight-Regular.ttf"});
	break;
    case NoteFontId::Debug:
    default:
	found = {};
	break;
    }
    path_cache.emplace(key, found);
    return found;
}

FontAtlas* get_or_bake_atlas(SDL_Renderer* renderer, NoteTypography typo, uint8_t face_flags) {
    if (renderer == nullptr || !note_font_is_ttf(typo.font)) {
	return nullptr;
    }
    typo = note_typography_normalize(typo);
    const bool bold = (face_flags & TextStyleBold) != 0;
    const bool italic = (face_flags & TextStyleItalic) != 0;
    const std::string path = face_path_for(typo.font, bold, italic);
    if (path.empty()) {
	return nullptr;
    }
    TtfCache* ttf_cache = get_ttf_cache(path);
    if (ttf_cache == nullptr || !ttf_cache->ok) {
	return nullptr;
    }
    const std::vector<unsigned char>& ttf = ttf_cache->bytes;

    AtlasKey key{typo.font, typo.size_px, static_cast<uint8_t>(face_flags & (TextStyleBold | TextStyleItalic)),
		 renderer};
    auto it = g_atlases.find(key);
    if (it != g_atlases.end()) {
	return &it->second;
    }

    FontAtlas atlas{};
    const float pixel_height = static_cast<float>(typo.size_px);
    for (int dim = 512; dim <= 2048; dim *= 2) {
	atlas.atlas_w = dim;
	atlas.atlas_h = dim;
	std::vector<unsigned char> bitmap(static_cast<std::size_t>(dim * dim), 0);
	const int bake = stbtt_BakeFontBitmap(ttf.data(), 0, pixel_height, bitmap.data(), dim, dim,
					     32, 96, atlas.chars);
	if (bake <= 0 && dim < 2048) {
	    continue;
	}

	SDL_Surface* surface = SDL_CreateSurface(dim, dim, SDL_PIXELFORMAT_RGBA32);
	if (surface == nullptr) {
	    return nullptr;
	}
	auto* pixels = static_cast<Uint32*>(surface->pixels);
	for (int i = 0; i < dim * dim; ++i) {
	    const Uint8 a = bitmap[static_cast<std::size_t>(i)];
	    pixels[i] = (static_cast<Uint32>(a) << 24) | 0x00FFFFFFu;
	}
	atlas.texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_DestroySurface(surface);
	if (atlas.texture == nullptr) {
	    return nullptr;
	}
	SDL_SetTextureBlendMode(atlas.texture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(atlas.texture, SDL_SCALEMODE_LINEAR);

	const TextFontMetrics metrics = text_font_metrics(typo);
	atlas.char_w = metrics.char_w;
	atlas.line_h = metrics.line_h;

	auto [placed, _] = g_atlases.emplace(key, atlas);
	return &placed->second;
    }
    return nullptr;
}

void draw_debug_text(SDL_Renderer* renderer, float x, float y, const char* text, uint8_t flags) {
    const float ox = ((flags & TextStyleBold) != 0) ? 1.0f : 0.0f;
    SDL_RenderDebugText(renderer, std::floor(x), std::floor(y), text);
    if (ox != 0.0f) {
	SDL_RenderDebugText(renderer, std::floor(x + ox), std::floor(y), text);
    }
}

void draw_decorations(SDL_Renderer* renderer, float x, float y, float width, float line_h,
		      uint8_t flags) {
    if (width <= 0.0f) {
	return;
    }
    if ((flags & TextStyleUnderline) != 0) {
	const float uy = std::floor(y + line_h - 1.0f);
	SDL_RenderLine(renderer, x, uy, x + width, uy);
    }
    if ((flags & TextStyleStrike) != 0) {
	const float sy = std::floor(y + line_h * 0.55f);
	SDL_RenderLine(renderer, x, sy, x + width, sy);
    }
}

float glyph_advance_from_atlas(FontAtlas* atlas, unsigned char ch) {
    if (atlas == nullptr) {
	return 0.0f;
    }
    if (ch < 32 || ch > 127) {
	ch = '?';
    }
    return atlas->chars[ch - 32].xadvance;
}

float glyph_advance_from_ttf(NoteTypography typo, uint8_t face_flags, unsigned char ch) {
    typo = note_typography_normalize(typo);
    if (!note_font_is_ttf(typo.font)) {
	return text_font_metrics(typo).char_w;
    }
    if (ch < 32 || ch > 127) {
	ch = '?';
    }
    const bool bold = (face_flags & TextStyleBold) != 0;
    const bool italic = (face_flags & TextStyleItalic) != 0;
    const std::string path = face_path_for(typo.font, bold, italic);
    if (path.empty()) {
	return text_font_metrics(typo).char_w;
    }
    TtfCache* cache = get_ttf_cache(path);
    if (cache == nullptr || !cache->ok || !cache->info_ok) {
	return text_font_metrics(typo).char_w;
    }
    const float scale =
	stbtt_ScaleForPixelHeight(&cache->info, static_cast<float>(typo.size_px));
    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&cache->info, ch, &advance, &lsb);
    return scale * static_cast<float>(advance);
}

float measure_span_width(SDL_Renderer* renderer, NoteTypography typo, const char* text,
			 std::size_t len, uint8_t face_flags) {
    const TextFontMetrics metrics = text_font_metrics(typo);
    if (typo.font == NoteFontId::Debug) {
	return static_cast<float>(len) * metrics.char_w;
    }
    FontAtlas* atlas = renderer != nullptr ? get_or_bake_atlas(renderer, typo, face_flags) : nullptr;
    float w = 0.0f;
    for (std::size_t i = 0; i < len; ++i) {
	const unsigned char ch = static_cast<unsigned char>(text[i]);
	if (atlas != nullptr) {
	    w += glyph_advance_from_atlas(atlas, ch);
	} else {
	    w += glyph_advance_from_ttf(typo, face_flags, ch);
	}
    }
    return w;
}

float draw_atlas_span(SDL_Renderer* renderer, FontAtlas* atlas, float x, float y, const char* text,
		      std::size_t len) {
    if (atlas == nullptr || atlas->texture == nullptr || text == nullptr || len == 0) {
	return 0.0f;
    }
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 255;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_SetTextureColorMod(atlas->texture, r, g, b);
    SDL_SetTextureAlphaMod(atlas->texture, a);

    float pen_x = x;
    const float baseline = y + atlas->line_h * 0.8f;
    for (std::size_t i = 0; i < len; ++i) {
	unsigned char ch = static_cast<unsigned char>(text[i]);
	if (ch < 32 || ch > 127) {
	    ch = '?';
	}
	float xpos = pen_x;
	float ypos = baseline;
	stbtt_aligned_quad q{};
	stbtt_GetBakedQuad(atlas->chars, atlas->atlas_w, atlas->atlas_h, ch - 32, &xpos, &ypos, &q,
			   1);
	SDL_FRect src{q.s0 * static_cast<float>(atlas->atlas_w),
		      q.t0 * static_cast<float>(atlas->atlas_h),
		      (q.s1 - q.s0) * static_cast<float>(atlas->atlas_w),
		      (q.t1 - q.t0) * static_cast<float>(atlas->atlas_h)};
	SDL_FRect dst{q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0};
	SDL_RenderTexture(renderer, atlas->texture, &src, &dst);
	pen_x = xpos; // GetBakedQuad advances by the glyph's real xadvance
    }
    return pen_x - x;
}

} // namespace

void text_font_draw(SDL_Renderer* renderer, NoteTypography typo, float x, float y,
		    const char* text) {
    text_font_draw_styled(renderer, typo, x, y, text, nullptr, text ? std::strlen(text) : 0);
}

void text_font_draw_styled(SDL_Renderer* renderer, NoteTypography typo, float x, float y,
			   const char* text, const uint8_t* style_flags, std::size_t len) {
    if (renderer == nullptr || text == nullptr || len == 0) {
	return;
    }
    typo = note_typography_normalize(typo);
    const TextFontMetrics metrics = text_font_metrics(typo);

    float pen_x = x;
    std::size_t i = 0;
    while (i < len) {
	const uint8_t flags = style_flags ? style_flags[i] : static_cast<uint8_t>(TextStyleNone);
	std::size_t j = i + 1;
	while (j < len) {
	    const uint8_t f = style_flags ? style_flags[j] : static_cast<uint8_t>(TextStyleNone);
	    if (f != flags) {
		break;
	    }
	    ++j;
	}
	const std::size_t span_len = j - i;
	std::string span(text + i, span_len);
	float span_w = static_cast<float>(span_len) * metrics.char_w;

	if (typo.font == NoteFontId::Debug) {
	    draw_debug_text(renderer, pen_x, y, span.c_str(), flags);
	} else {
	    FontAtlas* atlas = get_or_bake_atlas(renderer, typo, flags);
	    if (atlas == nullptr) {
		draw_debug_text(renderer, pen_x, y, span.c_str(), flags);
	    } else {
		span_w = draw_atlas_span(renderer, atlas, pen_x, y, span.c_str(), span_len);
	    }
	}
	draw_decorations(renderer, pen_x, y, span_w, metrics.line_h, flags);
	pen_x += span_w;
	i = j;
    }
}

float text_font_prefix_width(SDL_Renderer* renderer, NoteTypography typo, const char* text,
			     std::size_t col, const uint8_t* style_flags, std::size_t len) {
    if (text == nullptr || col == 0 || len == 0) {
	return 0.0f;
    }
    typo = note_typography_normalize(typo);
    const std::size_t n = std::min(col, len);
    if (typo.font == NoteFontId::Debug) {
	return static_cast<float>(n) * text_font_metrics(typo).char_w;
    }

    float w = 0.0f;
    std::size_t i = 0;
    while (i < n) {
	const uint8_t flags = style_flags ? style_flags[i] : static_cast<uint8_t>(TextStyleNone);
	std::size_t j = i + 1;
	while (j < n) {
	    const uint8_t f = style_flags ? style_flags[j] : static_cast<uint8_t>(TextStyleNone);
	    if (f != flags) {
		break;
	    }
	    ++j;
	}
	w += measure_span_width(renderer, typo, text + i, j - i, flags);
	i = j;
    }
    return w;
}

std::size_t text_font_column_at_x(SDL_Renderer* renderer, NoteTypography typo, const char* text,
				  std::size_t len, const uint8_t* style_flags, float local_x) {
    if (text == nullptr || len == 0 || local_x <= 0.0f) {
	return 0;
    }
    typo = note_typography_normalize(typo);
    if (typo.font == NoteFontId::Debug) {
	const float char_w = text_font_metrics(typo).char_w;
	if (char_w <= 0.0f) {
	    return 0;
	}
	const int col = static_cast<int>(std::floor(local_x / char_w));
	if (col <= 0) {
	    return 0;
	}
	return std::min(static_cast<std::size_t>(col), len);
    }

    float x = 0.0f;
    for (std::size_t i = 0; i < len; ++i) {
	const uint8_t flags = style_flags ? style_flags[i] : static_cast<uint8_t>(TextStyleNone);
	const float adv = measure_span_width(renderer, typo, text + i, 1, flags);
	if (local_x < x + adv * 0.5f) {
	    return i;
	}
	x += adv;
    }
    return len;
}

void text_font_shutdown(SDL_Renderer* renderer) {
    for (auto it = g_atlases.begin(); it != g_atlases.end();) {
	if (renderer == nullptr || it->first.renderer == renderer) {
	    if (it->second.texture != nullptr) {
		SDL_DestroyTexture(it->second.texture);
	    }
	    it = g_atlases.erase(it);
	} else {
	    ++it;
	}
    }
}
