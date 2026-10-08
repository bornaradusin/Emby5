#include "ui/theme_presets.h"
#include "app/settings.h"
namespace ui {
static const ThemePalette presets[] = {
    {"Glass", 0xff07070au, 0xff1c1c22u, 0xfff5f5f7u, 0xffffffffu, SurfaceStyle::Glass, 24.f, 0.f, 0.f},
    {"Acrylic", 0xff151e2au, 0xff26394cu, 0xfff0f5fcu, 0xff86c7fcu, SurfaceStyle::Frost, 12.f, 1.f, 5.f},
    {"Brutal", 0xfff7eb4au, 0xffffffffu, 0xff171717u, 0xffff3b30u, SurfaceStyle::Hard, 0.f, 4.f, 11.f},
    {"Clay", 0xffdae0e8u, 0xffc3cbd9u, 0xff1b2c46u, 0xff5976a8u, SurfaceStyle::Neumorphic, 30.f, 0.f, 8.f},
    {"Tiles", 0xff141e34u, 0xff2f5bc0u, 0xffffffffu, 0xffffdf55u, SurfaceStyle::Flat, 0.f, 0.f, 0.f},
    {"Gloss", 0xff101521u, 0xff313d62u, 0xfffafaffu, 0xff80beffu, SurfaceStyle::Gloss, 16.f, 2.f, 6.f},
    {"Classic", 0xffc0c0c0u, 0xffe5e5e5u, 0xff202020u, 0xff144ab0u, SurfaceStyle::Bevel, 0.f, 3.f, 0.f},
    {"Blueprint", 0xff071c35u, 0xff0b3258u, 0xffeafaffu, 0xff51d7ffu, SurfaceStyle::Outline, 0.f, 2.f, 0.f},
    {"Hazard", 0xff091013u, 0xff102e31u, 0xffeffff6u, 0xff4dffaeu, SurfaceStyle::Glow, 8.f, 2.f, 6.f},
    {"Candy", 0xffffe8edu, 0xfff9bad4u, 0xff512244u, 0xffe44b96u, SurfaceStyle::Soft, 42.f, 0.f, 8.f},
    {"Contrast", 0xff080808u, 0xff232323u, 0xffffffffu, 0xffffff00u, SurfaceStyle::Outline, 0.f, 3.f, 0.f},
    {"Pixel", 0xff171226u, 0xff433c68u, 0xfff7efb7u, 0xffe68cfcu, SurfaceStyle::Pixel, 0.f, 3.f, 0.f},
    {"Soft", 0xffeff5fcu, 0xffffffffu, 0xff25324cu, 0xff2680e5u, SurfaceStyle::Flat, 14.f, 1.f, 1.f},
    {"Daisy", 0xfff9f5fdu, 0xffffffffu, 0xff272640u, 0xff635bffu, SurfaceStyle::Soft, 15.f, 1.f, 3.f},
    {"Pico", 0xffeff5f7u, 0xffffffffu, 0xff253640u, 0xff287ed1u, SurfaceStyle::Soft, 13.f, 1.f, 5.f},
    {"Enterprise", 0xfff5f7fbu, 0xffffffffu, 0xff17253au, 0xff1674dfu, SurfaceStyle::Flat, 7.f, 1.f, 0.f},
    {"Chakra", 0xffedf8f5u, 0xffffffffu, 0xff17332cu, 0xff14a78du, SurfaceStyle::Soft, 18.f, 2.f, 4.f},
    {"Fresh", 0xff0d161cu, 0xff1d3033u, 0xffe8fdf8u, 0xff20e3a6u, SurfaceStyle::Flat, 10.f, 1.f, 0.f},
    {"Neutral", 0xff18191bu, 0xff2c2d30u, 0xfff1f1f1u, 0xff63aee9u, SurfaceStyle::Flat, 9.f, 1.f, 0.f},
    {"Material", 0xfff7f3f3u, 0xffffffffu, 0xff2b2432u, 0xffe9489bu, SurfaceStyle::Soft, 14.f, 1.f, 5.f},
    {"Humane", 0xffeef1f5u, 0xffffffffu, 0xff262f40u, 0xff2184d5u, SurfaceStyle::Flat, 7.f, 1.f, 0.f},
    {"Standard", 0xfff5f7fau, 0xffffffffu, 0xff252d38u, 0xff1976d2u, SurfaceStyle::Flat, 12.f, 2.f, 1.f},
    {"Pill", 0xff070b11u, 0xff171e29u, 0xffffffffu, 0xff388bffu, SurfaceStyle::Flat, 50.f, 1.f, 0.f},
    {"Admin", 0xff121a2au, 0xff232d44u, 0xffeef5ffu, 0xff4893fbu, SurfaceStyle::Soft, 12.f, 2.f, 6.f},
    {"Friendly", 0xfff7ffffu, 0xffffffffu, 0xff27373cu, 0xff00cbb5u, SurfaceStyle::Soft, 25.f, 1.f, 10.f},
    {"Crisp", 0xfff8f9fau, 0xffffffffu, 0xff262e34u, 0xff1976e6u, SurfaceStyle::Flat, 0.f, 1.f, 0.f},
    {"Layers", 0xffedf7f6u, 0xffffffffu, 0xff193432u, 0xff09a899u, SurfaceStyle::Soft, 13.f, 1.f, 3.f},
    {"Utility", 0xfff5f5f5u, 0xffffffffu, 0xff2b2b2bu, 0xff4a79d1u, SurfaceStyle::Flat, 0.f, 2.f, 0.f},
    {"Sketch", 0xfffbf3dfu, 0xfffffaf0u, 0xff403829u, 0xffa86144u, SurfaceStyle::Sketch, 3.f, 2.f, 4.f},
    {"Featherweight", 0xffffffffu, 0xfffefefeu, 0xff3a3a3au, 0xff7454b5u, SurfaceStyle::Outline, 2.f, 1.f, 0.f},
    {"Code", 0xff0d1117u, 0xff161b22u, 0xffe6edf3u, 0xff2da44eu, SurfaceStyle::Outline, 6.f, 1.f, 0.f},
};
const ThemePalette &theme_palette() { int i = settings::get().local.theme; return presets[i>=0 && i<kThemeCount ? i : 0]; }
const char *theme_name(int id) { return presets[id>=0 && id<kThemeCount ? id : 0].name; }
uint32_t theme_bg() { return theme_palette().page; }
uint32_t theme_text() { return theme_palette().text; }
uint32_t theme_text2() { return (theme_palette().text & 0xffffffu) | 0xad000000u; }
uint32_t theme_text3() { return (theme_palette().text & 0xffffffu) | 0x6b000000u; }
}
