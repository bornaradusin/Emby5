/* Production persistence, palette and section helpers; no GPU or PS5 APIs. */
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "../../app/src/app/settings.cpp"
#include "ui/settings_layout.h"
#include "ui/library_layout.h"
static std::string root;
extern "C" {
const char *evo_data_dir() { return root.c_str(); }
const char *evo_data_path(const char *leaf) {
    static thread_local std::string path;
    std::string relative=leaf?leaf:"";
    if (relative=="emby5") relative="config";
    else if (relative.rfind("emby5/",0)==0) relative="config/"+relative.substr(6);
    path=root+"/"+relative; return path.c_str();
}
int evo_mkdir(const char *path) { std::filesystem::create_directories(path); return 0; }
}
const char *T(const char *s) { return s; }
namespace jf { bool Client::set_prefs(const UserPrefs &) { assert(false && "appearance must not alter server preferences"); return false; } }
static void fresh() { settings::s_all=settings::All{}; }
static bool same_palette(const ui::ThemePalette &a,const ui::ThemePalette &b) {
    return a.page==b.page&&a.panel==b.panel&&a.text==b.text&&a.accent==b.accent&&
        a.style==b.style&&a.radius==b.radius&&a.border==b.border&&a.depth==b.depth;
}
int main() {
    char temp[]="/tmp/emby5-appearance-XXXXXX";assert(mkdtemp(temp));root=temp;
    std::filesystem::create_directories(root+"/config");
    // New profiles use the new default; explicit saved themes/slots stay intact.
    fresh();settings::load_local();assert(settings::get().local.theme==settings::kDefaultTheme);
    assert(std::string(ui::theme_name(settings::kDefaultTheme))=="Emby5");
    assert(ui::theme_palette().style==ui::SurfaceStyle::Glass);
    assert(ui::colour_luminance(ui::theme_palette().page)<.01);
    {std::ofstream f(root+"/config/settings.json");f<<R"({"maxMbps":20})";}
    fresh();settings::load_local();assert(settings::get().local.theme==settings::kDefaultTheme&&settings::get().local.max_mbps==20);
    // Old files do not acquire overrides and retain the same theme and playback settings.
    {std::ofstream f(root+"/config/settings.json");f<<R"({"theme":4,"maxMbps":40,"audioDelayMs":100,"stillWatching":1,"upscaling":3})";}
    fresh();settings::load_local();auto old=settings::get().local;
    assert(old.theme==4&&old.max_mbps==40&&old.audio_delay_ms==100&&old.still_watching==1&&old.upscale_mode==3);
    assert(old.theme_accent==0&&old.theme_background==0&&old.ui_text_percent==100&&old.poster_size==1);
    std::set<std::string> names;
    for(int theme=0;theme<ui::kThemeCount;++theme) {
        fresh();settings::s_all.local.theme=theme;
        const auto defaults=ui::theme_palette();names.insert(defaults.name);
        for(int bg=0;bg<ui::kBackgroundCount;++bg) for(int accent=0;accent<9;++accent) {
            auto &l=settings::s_all.local;l.theme_background=bg;l.theme_accent=accent;
            const auto custom=ui::theme_palette();
            assert(custom.style==defaults.style&&custom.radius==defaults.radius);
            assert(custom.text!=custom.panel);
            if(!bg&&!accent)assert(same_palette(defaults,custom));
            l.ui_text_percent=120;l.poster_size=2;l.max_mbps=40;l.audio_delay_ms=100;
            assert(settings::save_local_snapshot(l));fresh();settings::load_local();
            const auto after=settings::get().local;
            assert(after.theme==theme&&after.theme_background==bg&&after.theme_accent==accent);
            assert(after.ui_text_percent==120&&after.poster_size==2&&after.max_mbps==40&&after.audio_delay_ms==100);
        }
        settings::s_all.local.theme_background=0;settings::s_all.local.theme_accent=0;
        settings::s_all.local.ui_text_percent=100;settings::s_all.local.poster_size=1;
        assert(same_palette(defaults,ui::theme_palette()));
    }
    assert(names.size()==ui::kThemeCount);
    {std::ofstream f(root+"/config/settings.json");f<<R"({"theme":30,"themeAccent":999,"themeBackground":-99,"uiTextPercent":115,"posterSize":999,"maxMbps":0,"audioDelayMs":0})";}
    fresh();settings::load_local();auto bounded=settings::get().local;
    assert(bounded.theme_accent==8&&bounded.theme_background==0&&bounded.ui_text_percent==120&&bounded.poster_size==2);
    using S=ui::SettingsScreen;
    assert(ui::settings_layout::tile_count==5);
    for(const auto &tile:ui::settings_layout::tiles) {
        assert(tile.section!=4&&tile.section!=5); // no top-level duplicates
        assert(ui::settings_layout::section_of(tile.first)==tile.section);
    }
    assert(ui::settings_layout::section_of(S::Appearance)==7&&ui::settings_layout::section_of(S::StorageInfo)==7);
    int system=0,appearance=0,about=0,installed=0,whatsnew=0;
    for(int r=0;r<S::RowCount;++r) {
        const int section=ui::settings_layout::section_of(r);assert(section>=0&&section<=8);
        system+=section==7;appearance+=section==4&&r!=S::Together;about+=section==5;
        const std::string label=ui::settings_layout::label_of(r);
        installed+=label=="Installed version";whatsnew+=label=="What's New";
    }
    assert(system==6&&appearance==25&&about==0&&installed==1&&whatsnew==1);
    for(int r:{S::VODStatus,S::VODMoviesStatus,S::VODShowsStatus,S::VODM3UStatus,S::VODRefresh})
        assert(ui::settings_layout::section_of(r)==3&&!ui::settings_layout::adjustable(r));
    for(int r:{S::EmbyLive,S::XtreamLive,S::M3UStatus})assert(ui::settings_layout::section_of(r)==3);
    assert(std::string(ui::settings_layout::label_of(S::VODMoviesStatus))=="Xtream movies");
    assert(std::string(ui::settings_layout::label_of(S::VODShowsStatus))=="Xtream TV shows");
    assert(ui::settings_layout::section_of(S::Theme)==4&&ui::settings_layout::section_of(S::ServerInfo)==7);
    assert(!ui::settings_layout::adjustable(S::Appearance)&&!ui::settings_layout::adjustable(S::ResetAppearance));
    assert(ui::settings_layout::adjustable(S::UiTextSize)&&ui::settings_layout::adjustable(S::PosterSize));
    for(int size=0;size<3;++size) {
        settings::s_all.local.poster_size=size;
        const ui::LibraryGeometry g(gfx::W,ui::kPad);
        assert(g.cols==7-size&&g.width>0&&g.height>g.width);
        assert(std::abs(g.cols*g.width+(g.cols-1)*g.gap+2*ui::kPad-gfx::W)<.01f);
        if(size==1)assert(std::abs(g.width-240.f)<.01f&&std::abs(g.height-360.f)<.01f);
    }
    for(int scale:{90,100,110,120}) {
        settings::s_all.local.ui_text_percent=scale;
        assert(std::abs(ui::theme_text_scale()-scale/100.f)<.0001f);
    }
    // Update notifications default to On for absent/invalid legacy fields.
    for(const std::string &json:{"{}","{\"checkUpdates\":null}","{\"checkUpdates\":0}","{\"checkUpdates\":\"false\"}"}) {
        fresh();{std::ofstream f(root+"/config/settings.json");f<<json;}
        settings::load_local();assert(settings::get().local.check_updates);
    }
    for(bool enabled:{false,true}) {
        fresh();auto prefs=settings::get().local;prefs.check_updates=enabled;
        assert(settings::save_local_snapshot(prefs));fresh();settings::load_local();
        assert(settings::get().local.check_updates==enabled);
        settings::AppearanceEditor a;a.begin(settings::get().local);a.reset();a.choose_preset(1);
        assert(a.apply_to(settings::get().local).check_updates==enabled);
    }
    // Real persistence of every additional field and the saved custom slot.
    fresh(); auto changed=settings::get().local;
    changed.background_artwork=false;changed.backdrop_dim=4;changed.control_style=2;
    changed.strong_focus=true;changed.poster_enlarge=false;changed.reduced_motion=true;
    changed.description_scroll=1;changed.spoiler_protection=true;changed.title_as_text=true;changed.detail_poster=true;
    changed.my_theme_valid=true;changed.my_theme=changed;changed.my_theme.theme=29;changed.my_theme.ui_text_percent=110;
    assert(settings::save_local_snapshot(changed));fresh();settings::load_local();
    assert(settings::same_appearance(settings::get().local,changed));
    assert(settings::get().local.my_theme_valid&&settings::same_appearance(settings::get().local.my_theme,changed.my_theme));
    const auto persisted=settings::get().local;
    auto preview=persisted;preview.theme=6;preview.theme_accent=3;settings::preview_appearance(&preview);
    assert(settings::get_appearance().theme==6&&settings::same_appearance(settings::get().local,persisted));
    settings::preview_appearance(nullptr);assert(settings::same_appearance(settings::get_appearance(),persisted));
    settings::AppearanceEditor editor;editor.begin(persisted);editor.remember();editor.choose_preset(3);
    assert(!settings::appearance_modified(editor.draft));editor.recall();assert(settings::same_appearance(editor.draft,persisted));
    editor.draft.theme_accent=8;editor.reset();assert(editor.draft.theme==persisted.theme&&!settings::appearance_modified(editor.draft));
    auto newer=persisted;newer.max_mbps=120;newer.sub_size=150;newer.theme_music=false;
    const auto applied=editor.apply_to(newer);assert(applied.max_mbps==120&&applied.sub_size==150&&!applied.theme_music&&applied.my_theme_valid);
    jf::Item episode;episode.type="Episode";episode.played=false;settings::Appearance protection;protection.spoiler_protection=true;
    assert(settings::hide_episode_spoilers(protection,episode));episode.played=true;assert(!settings::hide_episode_spoilers(protection,episode));
    episode.type="Movie";episode.played=false;assert(!settings::hide_episode_spoilers(protection,episode));
    fresh();
    for(int t=0;t<ui::kThemeCount;++t)for(int bg=1;bg<ui::kBackgroundCount;++bg)for(int accent=0;accent<9;++accent)for(int style=0;style<3;++style){
        auto &l=settings::s_all.local;l.theme=t;l.theme_background=bg;l.theme_accent=accent;l.control_style=style;
        const auto p=ui::theme_palette();
        assert(ui::colour_contrast(p.text,p.page)>=4.5&&ui::colour_contrast(p.text,p.panel)>=4.5);
        assert(ui::colour_contrast(p.text,ui::focus_panel_colour(p,true))>=4.5);
        assert(ui::colour_contrast(ui::theme_focus_colour(),p.panel)>=3.);
    }
    // New named slots round-trip through production JSON; draft edits are isolated.
    fresh();{std::ofstream f(root+"/config/settings.json");f<<R"({"theme":4,"myTheme":{"theme":8,"themeAccent":2}})";}
    settings::load_local();assert(settings::get().local.custom_themes[0].valid);
    assert(settings::get().local.custom_themes[0].name=="My theme");
    settings::AppearanceEditor named;named.begin(settings::get().local);assert(!named.dirty());
    for(int slot=0;slot<3;++slot){
        named.select_slot(slot);named.choose_preset(slot*11);named.draft.colour_variant=slot+2;
        named.draft.button_shape=slot+3;named.rename("  Theme "+std::to_string(slot)+"  ");named.remember();
    }
    assert(named.dirty());auto edited=named.apply_to(settings::get().local);
    edited.max_mbps=80;assert(settings::save_local_snapshot(edited));fresh();settings::load_local();
    auto restored=settings::get().local;assert(restored.max_mbps==80);
    for(int slot=0;slot<3;++slot){assert(restored.custom_themes[slot].valid);
        assert(restored.custom_themes[slot].name=="Theme "+std::to_string(slot));
        assert(settings::same_appearance(restored.custom_themes[slot].appearance,edited.custom_themes[slot].appearance));}
    named.begin(restored);assert(!named.dirty());named.select_slot(1);named.rename("Unsaved");
    named.choose_preset(2);named.remember();assert(named.dirty());named.end();
    fresh();settings::load_local();assert(settings::get().local.custom_themes[1].name=="Theme 1");
    named.begin(restored);named.draft.button_shape=1;assert(named.dirty());named.restore();assert(!named.dirty());
    auto future=restored;future.max_mbps=25;future.sub_size=130;auto merged=named.apply_to(future);
    assert(merged.max_mbps==25&&merged.sub_size==130);
    assert(settings::theme_slot_name("  Night\nmode\t  ")=="Nightmode");
    std::string multibyte;for(int i=0;i<40;++i)multibyte+="\xe6\x99\x9a";
    assert(settings::theme_slot_name(multibyte).size()==63);
    assert(settings::theme_slot_name("x\xc0\xaf\xed\xa0\x80y")=="xy");
    auto invalid=restored;invalid.button_shape=999;invalid.colour_variant=-50;
    assert(settings::save_local_snapshot(invalid));fresh();settings::load_local();
    assert(settings::get().local.button_shape==5&&settings::get().local.colour_variant==0);
    std::cout<<"PASS: three named slots, legacy My theme migration, shape/variant persistence, cancelled draft isolation, safe UTF-8 names and unrelated preference preservation\n";
    {std::ofstream f(root+"/config/settings.json");f<<R"({"upscaling":1e300,"maxMbps":null,"audioDelayMs":-1e300,"subtitles":{"size":1e300,"color":null,"offset":1e300}})";}
    fresh();settings::load_local();const auto safe=settings::get().local;
    assert(safe.upscale_mode==3&&safe.max_mbps==settings::Local{}.max_mbps&&safe.audio_delay_ms==-500);
    assert(safe.sub_size==200&&safe.sub_color==settings::Local{}.sub_color&&safe.sub_offset==40);
    std::cout<<"PASS: missing/non-numeric/oversized legacy preference values keep defaults or clamp before integer conversion\n";
    std::filesystem::remove_all(root);
    std::cout<<"PASS: all stable preset IDs/background/accent persistence combinations, legacy/default/reset behaviour, bounds, System-only children, preview/editor/new fields/My-theme roundtrip, contrast checks and 5/6/7-column geometry\n";
}
