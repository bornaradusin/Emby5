/* Emby5 native AGC theme palettes, adapted from ps5-homebrew-ui design references. */
#pragma once
#include <cstdint>
namespace ui {
enum class SurfaceStyle { Glass, Frost, Hard, Neumorphic, Flat, Gloss, Bevel, Outline, Glow, Pixel, Soft, Sketch };
struct ThemePalette { const char *name; uint32_t page, panel, text, accent; SurfaceStyle style; float radius; float border; float depth; };
const ThemePalette &theme_palette();
const char *theme_name(int id);
constexpr int kThemeCount = 31;
uint32_t theme_bg();
uint32_t theme_text();
uint32_t theme_text2();
uint32_t theme_text3();
}
