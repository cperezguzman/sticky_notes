#pragma once

#include "text_font.h"
#include "text_style.h"

#include <SDL3/SDL.h>

#include <cstddef>

void text_font_draw(SDL_Renderer* renderer, NoteTypography typo, float x, float y,
		    const char* text);

void text_font_draw_styled(SDL_Renderer* renderer, NoteTypography typo, float x, float y,
			   const char* text, const uint8_t* style_flags, std::size_t len);

// Horizontal advance for prefix text[0..col). Falls back to monospace metrics without a renderer.
float text_font_prefix_width(SDL_Renderer* renderer, NoteTypography typo, const char* text,
			     std::size_t col, const uint8_t* style_flags, std::size_t len);

// Column under local_x within a line (0 = leftmost). Uses proportional advances when possible.
std::size_t text_font_column_at_x(SDL_Renderer* renderer, NoteTypography typo, const char* text,
				  std::size_t len, const uint8_t* style_flags, float local_x);

void text_font_shutdown(SDL_Renderer* renderer);
