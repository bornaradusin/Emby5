#include "ui/theme_presets.h"
#include "app/i18n.h"
#include "app/settings.h"
#include "app/appearance.h"
#include "ui/anim.h"
#include <cmath>
#include <algorithm>
namespace ui {
// Existing preset indices stay stable; Emby5 is appended as the default.
static const ThemePalette presets[] = {
    {"Glass",0xff080f22u,0xff12284cu,0xfff3f8ffu,0xff479cffu,SurfaceStyle::Glass,22.0f,1.0f,5.0f},
    {"Acrylic",0xff081c26u,0xff123b48u,0xffeffdffu,0xff26d7dcu,SurfaceStyle::Frost,14.0f,1.0f,4.0f},
    {"Brutal",0xfff4d83eu,0xfffff2b1u,0xff221714u,0xffaf123eu,SurfaceStyle::Hard,0.0f,3.0f,5.0f},
    {"Clay",0xffdedbeau,0xfff1edf7u,0xff332f4cu,0xff744acau,SurfaceStyle::Neumorphic,24.0f,1.0f,6.0f},
    {"Tiles",0xff082638u,0xff103f56u,0xfff2fcffu,0xffffb52eu,SurfaceStyle::Flat,2.0f,1.0f,1.0f},
    {"Gloss",0xff171027u,0xff34204bu,0xfffef5ffu,0xffe96ac8u,SurfaceStyle::Gloss,16.0f,1.0f,5.0f},
    {"Classic",0xffd3d9deu,0xfff2f4eeu,0xff182d35u,0xff1b5990u,SurfaceStyle::Bevel,0.0f,2.0f,2.0f},
    {"Blueprint",0xff071b35u,0xff0c3056u,0xffe9f7ffu,0xff3ed3ffu,SurfaceStyle::Outline,0.0f,1.5f,0.0f},
    {"Hazard",0xff102008u,0xff243b0bu,0xfff5ffe8u,0xffb5f331u,SurfaceStyle::Glow,9.0f,2.0f,4.0f},
    {"Candy",0xfffce0eeu,0xfffff4fcu,0xff552140u,0xffc51e82u,SurfaceStyle::Soft,30.0f,1.0f,4.0f},
    {"Contrast",0xff030303u,0xff121212u,0xffffffffu,0xffffeb36u,SurfaceStyle::Outline,0.0f,3.0f,0.0f},
    {"Pixel",0xff190d2eu,0xff321950u,0xfffff6d1u,0xfff17afbu,SurfaceStyle::Pixel,0.0f,3.0f,3.0f},
    {"Soft",0xffe8f0fcu,0xfff7fbffu,0xff233958u,0xff2465b4u,SurfaceStyle::Soft,18.0f,1.0f,2.0f},
    {"Daisy",0xfff1e4ffu,0xfffcf6ffu,0xff392050u,0xff7c2ebeu,SurfaceStyle::Soft,18.0f,1.0f,3.0f},
    {"Pico",0xffe4f2f7u,0xfff8fdffu,0xff193947u,0xff006986u,SurfaceStyle::Soft,12.0f,1.0f,3.0f},
    {"Enterprise",0xffe7edf5u,0xfffafcfeu,0xff20324cu,0xff175ac1u,SurfaceStyle::Flat,6.0f,1.0f,1.0f},
    {"Chakra",0xffddf2e9u,0xfff6fff9u,0xff143f31u,0xff087247u,SurfaceStyle::Soft,20.0f,1.0f,4.0f},
    {"Fresh",0xff05231bu,0xff0d3c2eu,0xffedfff4u,0xff37e79bu,SurfaceStyle::Flat,12.0f,1.0f,0.0f},
    {"Neutral",0xff151820u,0xff282d37u,0xfff4f6fdu,0xffaebfefu,SurfaceStyle::Flat,10.0f,1.0f,1.0f},
    {"Material",0xfff2e6f1u,0xfffff5ffu,0xff422041u,0xff99347fu,SurfaceStyle::Soft,20.0f,1.0f,4.0f},
    {"Humane",0xfff1e3d0u,0xfffff8e9u,0xff462b18u,0xff9b4b19u,SurfaceStyle::Flat,10.0f,1.0f,2.0f},
    {"Standard",0xffe9eff8u,0xfffcfdffu,0xff1e344fu,0xff1769b6u,SurfaceStyle::Flat,12.0f,1.0f,2.0f},
    {"Pill",0xff160c2bu,0xff2c1a4bu,0xfffaf3ffu,0xffb87bffu,SurfaceStyle::Flat,50.0f,1.0f,1.0f},
    {"Admin",0xff131c30u,0xff273453u,0xfff4f8ffu,0xffffb04au,SurfaceStyle::Soft,10.0f,1.0f,4.0f},
    {"Friendly",0xffdff4f0u,0xfff4fffcu,0xff164039u,0xff087c69u,SurfaceStyle::Soft,26.0f,1.0f,5.0f},
    {"Crisp",0xffeff2f6u,0xffffffffu,0xff202d42u,0xff235bddu,SurfaceStyle::Flat,0.0f,1.0f,0.0f},
    {"Layers",0xffe2ede2u,0xfff8fff4u,0xff2b4026u,0xff426f25u,SurfaceStyle::Soft,16.0f,1.0f,4.0f},
    {"Utility",0xffeeeae7u,0xfffff9f4u,0xff3c2b24u,0xffa14226u,SurfaceStyle::Flat,2.0f,1.5f,0.0f},
    {"Sketch",0xfff3e6ceu,0xfffff7e5u,0xff4b3324u,0xffa14b28u,SurfaceStyle::Sketch,3.0f,1.5f,3.0f},
    {"Featherweight",0xffeae6f4u,0xfffdfaffu,0xff392950u,0xff7846aeu,SurfaceStyle::Outline,4.0f,1.0f,0.0f},
    {"Code",0xff040e11u,0xff0b2027u,0xffe7fff4u,0xff3df3a4u,SurfaceStyle::Outline,5.0f,1.0f,0.0f},
    {"Emby5",0xff030603u,0xff0b180fu,0xfff5fff5u,0xff52c447u,SurfaceStyle::Glass,18.0f,1.2f,4.0f},
};
static_assert(sizeof(presets)/sizeof(presets[0])==kThemeCount,"stable preset IDs");
static const uint32_t accents[] = {0, 0xff52c447u, 0xff3496ffu, 0xff00d8b5u,
    0xffb277ffu, 0xffff57b2u, 0xffff9432u, 0xffff535fu, 0xffffcf38u};
const char *theme_accent_name(int id) {
    static const char *const names[] = {N_("Theme default"), N_("Emby green"), N_("Blue"), N_("Teal"), N_("Purple"), N_("Pink"), N_("Orange"), N_("Red"), N_("Gold")};
    return T(names[id >= 0 && id < 9 ? id : 0]);
}
const char *theme_background_name(int id) {
    static const char *const names[] = {N_("Theme default"), N_("Black"), N_("Dark"), N_("Light"),
        N_("Midnight blue"), N_("Ocean blue"), N_("Deep teal"), N_("Forest green"), N_("Royal purple"),
        N_("Plum"), N_("Burgundy"), N_("Warm brown"), N_("Slate blue")};
    static_assert(sizeof(names)/sizeof(names[0])==kBackgroundCount,"background IDs");
    return T(names[id >= 0 && id < kBackgroundCount ? id : 0]);
}
static uint32_t mix(uint32_t a,uint32_t b,float t) {
    uint32_t c=0xff000000u;
    for(int shift=0;shift<24;shift+=8)c|=(uint32_t)((1-t)*((a>>shift)&255u)+t*((b>>shift)&255u)+.5f)<<shift;
    return c;
}
const char *theme_variant_name(int id) {
    const char *names[]={N_("Theme default"),N_("Neutral"),N_("Blue"),N_("Green / Teal"),N_("Purple"),N_("Warm amber")};
    return T(names[std::max(0,std::min(5,id))]);
}
const char *theme_shape_name(int id) {
    const char *names[]={N_("Theme default"),N_("Square corners"),N_("Slightly rounded"),N_("Rounded"),N_("Pill / oval-ended"),N_("Angled corners")};
    return T(names[std::max(0,std::min(5,id))]);
}
ThemePalette theme_palette_for(const settings::Appearance &input) {
    const auto a=settings::normalize_appearance(input);
    auto p=presets[a.theme];
    bool light=colour_luminance(p.page)>.45;
    if(a.theme_background)light=a.theme_background==3;
    const uint32_t variant_dark[]={0,0xffc5d6efu,0xff429fffu,0xff26dfb5u,0xffba7affu,0xffffb731u};
    const uint32_t variant_light[]={0,0xff445470u,0xff185cc1u,0xff006f5au,0xff782eb8u,0xff925a05u};
    if(a.colour_variant){
        const auto v=a.colour_variant;
        p.accent=light?variant_light[v]:variant_dark[v];
        p.page=mix(light?0xfff1f3f7u:0xff0b111cu,p.accent,light?.06f:.035f);
        p.panel=mix(light?0xfffefeffu:0xff1a2534u,p.accent,light?.035f:.10f);
        p.text=light?0xff242d3au:0xffeff5fbu;
    }
    if(a.theme_accent)p.accent=accents[a.theme_accent];
    switch(a.theme_background){
    case 1:p.page=0xff000000u;p.panel=mix(0xff10151au,p.accent,.055f);p.text=0xfff5f9ffu;break;
    case 2:p.page=0xff11151cu;p.panel=mix(0xff202a36u,p.accent,.05f);p.text=0xfff5f9ffu;break;
    case 3:p.page=0xffeff2f7u;p.panel=mix(0xfffdfefeu,p.accent,.025f);p.text=0xff202a3bu;break;
    default:
        if(a.theme_background>=4){
            static const uint32_t backgrounds[]={0xff061533u,0xff063459u,0xff043831u,
                0xff08371fu,0xff291143u,0xff38132fu,0xff3e1320u,0xff352016u,0xff202f47u};
            p.page=backgrounds[a.theme_background-4];
            p.panel=mix(p.page,0xffc5daffu,.065f);p.text=0xfff5f9ffu;
        }
        break;
    }
    // Text must work on both ordinary surfaces and page backgrounds.
    if(std::min(colour_contrast(p.text,p.page),colour_contrast(p.text,p.panel))<4.5){
        const auto score=[&](uint32_t c){return std::min(colour_contrast(c,p.page),colour_contrast(c,p.panel));};
        p.text=score(0xff11151cu)>score(0xfff9fafcu)?0xff11151cu:0xfff9fafcu;
    }
    if(a.control_style==1)p.style=SurfaceStyle::Flat;
    else if(a.control_style==2)p.style=SurfaceStyle::Glass;
    return p;
}
const ThemePalette &theme_palette() {
    const auto a=settings::get_appearance();
    thread_local ThemePalette p{};thread_local settings::Appearance last;thread_local bool valid=false;
    if(!valid||!settings::same_appearance(a,last)){p=theme_palette_for(a);last=a;valid=true;}
    reduced_motion_active=a.reduced_motion;
    return p;
}
gfx::Shape theme_surface_shape(const gfx::Rect &r,float requested,bool control){
    const auto a=settings::get_appearance();const auto p=theme_palette();
    const float max_radius=std::max(0.f,std::min(r.w,r.h)*.5f);
    float radius=p.radius;bool bevel=false;
    if(!control)radius=std::min(radius,requested>=0?requested:24.f);
    if(control)switch(a.button_shape){
    case 1:radius=0;break;case 2:radius=6;break;case 3:radius=18;break;
    case 4:radius=max_radius;break;case 5:radius=std::min(14.f,r.h*.22f);bevel=true;break;
    default:if(a.theme==22)radius=max_radius;break;
    }
    return {r,std::max(0.f,std::min(radius,max_radius)),bevel};
}
uint32_t theme_secondary_colour(const ThemePalette &p,bool faint){
    for(float weight=faint?.64f:.78f;weight<=1.001f;weight+=.02f){
        const auto c=mix(p.panel,p.text,std::min(1.f,weight));
        if(std::min(colour_contrast(c,p.panel),colour_contrast(c,p.page))>=4.5)return c;
    }
    return p.text;
}
float theme_text_scale() { return std::max(90, std::min(120, settings::get_appearance().ui_text_percent)) / 100.f; }
float theme_card_scale() { const int s=settings::get_appearance().poster_size; return s==0?.86f:s==2?1.16f:1.f; }
int theme_grid_columns() { const int s=settings::get_appearance().poster_size; return s==0?7:s==2?5:6; }
const char *theme_name(int id) { return presets[id>=0 && id<kThemeCount ? id : settings::kDefaultTheme].name; }
bool theme_reduced_motion() { return settings::get_appearance().reduced_motion; }
bool theme_artwork_enabled() { return settings::get_appearance().background_artwork; }
float theme_backdrop_dim(float preset) {
    const int n=settings::get_appearance().backdrop_dim;
    const float values[]={0.f,.25f,.50f,.75f,.90f};
    return n>0&&n<5?values[n]:preset;
}
float theme_poster_lift(float lift) {
    const auto a=settings::get_appearance();
    return a.poster_enlarge&&!a.reduced_motion?lift:0.f;
}
float theme_description_speed() {
    const auto a=settings::get_appearance();
    return a.reduced_motion||a.description_scroll==0?0.f:a.description_scroll==1?9.f:18.f;
}
uint32_t theme_focus_colour() {
    const auto p=theme_palette();
    auto luminance=[](uint32_t c) {
        auto channel=[](unsigned n){double v=n/255.;return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return .2126*channel((c>>16)&255)+.7152*channel((c>>8)&255)+.0722*channel(c&255);
    };
    const double l=luminance(p.panel),a=luminance(p.accent);
    if ((std::max(l,a)+.05)/(std::min(l,a)+.05)>=3.) return p.accent;
    return l>.179?0xff111111u:0xfff7f7f7u;
}
uint32_t theme_bg() { return theme_palette().page; }
uint32_t theme_text() { return theme_palette().text; }
uint32_t theme_text2() { return theme_secondary_colour(theme_palette()); }
uint32_t theme_text3() { return theme_secondary_colour(theme_palette(),true); }
}
