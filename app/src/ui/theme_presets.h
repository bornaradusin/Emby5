/* Emby5 native AGC theme palettes, adapted from ps5-homebrew-ui design references. */
#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "gfx/gfx.h"
#include "app/settings.h"
namespace ui {
enum class SurfaceStyle { Glass, Frost, Hard, Neumorphic, Flat, Gloss, Bevel, Outline, Glow, Pixel, Soft, Sketch };
struct ThemePalette { const char *name; uint32_t page, panel, text, accent; SurfaceStyle style; float radius; float border; float depth; };
inline double colour_luminance(uint32_t c) {
    const auto channel=[](unsigned n){double v=n/255.;return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    return .2126*channel((c>>16)&255)+.7152*channel((c>>8)&255)+.0722*channel(c&255);
}
inline double colour_contrast(uint32_t a,uint32_t b) {
    const double x=colour_luminance(a),y=colour_luminance(b);
    return (std::max(x,y)+.05)/(std::min(x,y)+.05);
}
inline uint32_t focus_panel_colour(const ThemePalette &p,bool custom) {
    if(!custom && colour_contrast(p.text,p.accent)>=4.5)return p.accent;
    uint32_t tint=0xff000000u;
    for(int shift=0;shift<24;shift+=8)tint|=(((((p.panel>>shift)&255u)*5u+((p.accent>>shift)&255u)*3u)/8u)<<shift);
    return colour_contrast(p.text,tint)>=4.5?tint:p.panel;
}
const ThemePalette &theme_palette();
ThemePalette theme_palette_for(const settings::Appearance &a);
const char *theme_variant_name(int id);
const char *theme_shape_name(int id);
// explicit control roles prevent card and avatar radii inheriting a pill override.
gfx::Shape theme_surface_shape(const gfx::Rect &r,float requested_radius,bool control);
uint32_t theme_secondary_colour(const ThemePalette &p, bool faint=false);

const char *theme_name(int id);
constexpr int kThemeCount = settings::kThemePresetCount;
constexpr int kBackgroundCount = settings::kThemeBackgroundCount;
const char *theme_accent_name(int id);
const char *theme_background_name(int id);
float theme_text_scale();
float theme_card_scale();
int theme_grid_columns();
// Applies to the browsing UI only, never video/subtitle rasterisation.
bool theme_reduced_motion();
bool theme_artwork_enabled();
float theme_backdrop_dim(float preset);
float theme_poster_lift(float lift);
float theme_description_speed();
uint32_t theme_focus_colour();
uint32_t theme_bg();
uint32_t theme_text();
uint32_t theme_text2();
uint32_t theme_text3();
}
