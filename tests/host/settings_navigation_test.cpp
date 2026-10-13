/* Executes the real SettingsScreen input/draw and group store. Services/GPU mocked. */
#include "ui/settings_screen.h"
#include "ui/settings_layout.h"
#include "ui/description_reader.h"
#include "app/seerr_service.h"
#include "app/update_service.h"
#include "app/iptv_live.h"
#include "app/iptv_vod.h"
#include "app/iptv_m3u.h"
#include "platform/ime.h"
#include "jf/jf_http.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <tuple>
#include <unistd.h>
static settings::All state;
static settings::Appearance preview;
static bool preview_on=false;
static int saves=0,server_saves=0,live_retry=0,vod_retry=0,interval=24;
static std::string root;
static std::vector<bool> update_switches;
static int notes_requests=0;
static std::vector<std::tuple<float,float,std::string>> texts;
static std::vector<gfx::Rect> gradient_rects;
static std::function<void(const std::string&)> keyboard;
struct ModalPanel {gfx::Rect rect;gfx::Shape shape;ui::ThemePalette palette;float focus;};
static std::vector<ModalPanel> panels;
static std::vector<gfx::Shape> focus_shapes;
static std::vector<uint32_t> text_colours;
static std::vector<std::pair<gfx::Shape,uint32_t>> shape_fills;
namespace settings {
All get(){return state;}
Appearance get_appearance(){return preview_on?preview:static_cast<Appearance>(state.local);}
void preview_appearance(const Appearance *a){preview_on=a;if(a)preview=*a;}
void set_local(const Local &l){state.local=l;++saves;}
void set_server(jf::Client &,const jf::UserPrefs &p){state.server=p;++server_saves;}
}
extern "C" {
const char *evo_data_dir(){return root.c_str();}
const char *evo_data_path(const char *leaf){static thread_local std::string p;p=root+"/"+leaf;return p.c_str();}
int evo_mkdir(const char *p){std::filesystem::create_directories(p);return 0;}
int evo_agc_runtime_supports_120hz(){return 1;}
int evo_agc_runtime_set_120hz(int){return 0;}
void evo_agc_upscale_set_mode(int){}
void evo_audio_set_night(int){}
}
const char *T(const char *s){return s;}
namespace jf {HttpResponse http_request(const std::string &,const std::string &,const std::vector<std::string>&,const std::string&,int){assert(false&&"Settings test must not contact a server");return {};}}
namespace ime {bool init(){return true;}bool active(){return false;}void cancel(){keyboard={};}
void request(Kind,const std::string&,const std::string&,std::function<void(const std::string&)> fn){keyboard=std::move(fn);}}
namespace seerr_service {Config config(){return {};}void set_config(const Config&){}Snapshot snapshot(){return {};}
void sign_out(){}void sign_in(const std::string&,const std::string&){}void approve_quick_connect(){}void reconnect(){}void test(){}
std::string suggested_url(){return {};}}
namespace update_service {Snapshot snapshot(){return {};}void check(){++notes_requests;}void set_automatic_enabled(bool b){update_switches.push_back(b);}}
namespace syncplay {bool active(){return false;}std::string group_name(){return {};}}
namespace iptv_xtream {
bool load_credentials(Credentials *c){*c={};return false;}
bool save_credentials(const Credentials &){return true;}
bool authenticate(const Credentials&,std::string*){return false;}
bool load_catalog(const Credentials&,Catalog*,std::string*,int,std::function<bool()>,bool,std::function<void(const iptv_catalogue::Scan&)>){return false;}
}
namespace iptv_live {Snapshot summary(){return {};}void refresh(jf::Client*,bool,bool){++live_retry;}}
namespace iptv_vod {Snapshot snapshot(){Snapshot s;s.movie_count=123;s.show_count=456;return s;}
void refresh(bool,bool){++vod_retry;}int refresh_interval_hours(){return interval;}void set_refresh_interval_hours(int h){interval=h;}}
namespace iptv_m3u {std::string playlist_url(){return {}; }bool save_playlist_url(const std::string&){return true;}}
namespace ui {
uint32_t alpha(uint32_t c,float a){return (c&0xffffffu)|((uint32_t)(((c>>24)&255u)*a)<<24);}
void glass_panel(const gfx::Rect&r,float rad,float,bool,float f){panels.push_back({r,theme_surface_shape(r,rad,r.h<=180||r.w<=140),theme_palette(),f});}
void draw_focus_outline(const gfx::Rect&r,float rad,float){focus_shapes.push_back(theme_surface_shape(r,rad,true));}
void draw_image_focus_outline(const gfx::Rect&,float,float){}
void Drop::to(const gfx::Rect&,int,float,float){}void Drop::draw(float,float,bool*,float){}
struct SettingsRegressionAccess {
 static bool tiles(const SettingsScreen&s){return s.m_tiles;}
 static int section(const SettingsScreen&s){return s.m_section;}
 static int row(const SettingsScreen&s){return s.m_row;}
 static int tile(const SettingsScreen&s){return s.m_tile;}
 static int page(const SettingsScreen&s){return s.m_appearance_page;}
 static int confirm(const SettingsScreen&s){return s.m_appearance_confirm;}
 static int selected_slot(const SettingsScreen&s){return s.m_appearance.selected_slot;}
 static bool dirty(const SettingsScreen&s){return s.m_appearance.dirty();}
 static void modal(SettingsScreen &s,int theme,int shape,int scale,int kind,bool compare){
   s.begin_appearance();s.m_appearance.draft.theme=theme;s.m_appearance.draft.button_shape=shape;
   s.m_appearance.draft.ui_text_percent=scale;s.m_appearance.draft.theme_accent=0;
   s.m_appearance.draft.theme_background=0;s.m_appearance.draft.colour_variant=0;
   s.m_appearance.draft.control_style=0;
   s.m_appearance_confirm=kind==0?SettingsScreen::ConfirmExit:kind==1?SettingsScreen::ConfirmOverwrite:SettingsScreen::ConfirmReset;
   s.m_confirmation_choice=kind==0?2:1;s.m_compare_appearance=compare;
   settings::preview_appearance(&s.m_appearance.draft);
 }
 static void draw_modal(SettingsScreen &s){s.draw_appearance_confirmation();}
 static void close_modal(SettingsScreen &s){s.end_appearance(false);}
 static void to_row(SettingsScreen&s,int r){
   if(s.m_appearance.active){
     if(s.m_appearance_page>=0)s.input(NUVIO_BTN_CIRCLE);
     int group=-1;
     for(int g=0;g<9;++g){s.m_appearance_page=g;const auto rs=s.appearance_rows();
       if(std::find(rs.begin(),rs.end(),r)!=rs.end())group=g;}
     s.m_appearance_page=-1;assert(group>=0);
     while(s.m_appearance_index>group)s.input(NUVIO_BTN_UP);
     while(s.m_appearance_index<group)s.input(NUVIO_BTN_DOWN);
     s.input(NUVIO_BTN_CROSS);auto rows=s.appearance_rows();
     while(s.m_row!=r)s.input(NUVIO_BTN_DOWN);return;
   }
   assert(settings_layout::section_of(r)==s.m_section);
   int guard=0;while(s.m_row<r&&++guard<100)s.input(NUVIO_BTN_DOWN);
   while(s.m_row>r&&++guard<100)s.input(NUVIO_BTN_UP);assert(s.m_row==r);
 }
 static void channels(SettingsScreen&s,std::shared_ptr<const iptv_xtream::Catalog> c){
   s.m_iptv_store.load("test");assert(s.m_iptv_store.create("My group"));
   {std::lock_guard<std::mutex> l(s.m_iptv_catalog->mutex);s.m_iptv_catalog->catalog=c;}
   s.m_iptv_page=SettingsScreen::IPTVChannels;s.m_iptv_category=0;s.m_iptv_channel=0;s.iptv_sync_catalog();
 }
 static int selected(const SettingsScreen&s){return s.m_iptv_channel;}
 static size_t assigned(const SettingsScreen&s){return s.m_iptv_store.categories().at(0).channels.size();}
 static const void *indices(const SettingsScreen&s){return s.m_filtered_channels.data();}
 static const void *searches(const SettingsScreen&s){return s.m_channel_search_names.data();}
 static const void *catalog(const SettingsScreen&s){return s.m_visible_catalog.get();}
 static size_t count(const SettingsScreen&s){return s.m_filtered_channels.size();}
};
}
namespace gfx {
static float forced_scale=1.f;
float text_scale(){return forced_scale;}
float exchange_text_scale(float v){auto old=forced_scale;forced_scale=v;return old;}
float text(float x,float y,const std::string&s,const TextStyle&,uint32_t c,int){texts.emplace_back(x,y,s);text_colours.push_back(c);return s.size()*12.f;}
float text_width(const std::string&s,const TextStyle&){return s.size()*12.f;}
float measure_text(const std::string&s,const TextStyle&){return s.size()*12.f*text_scale();}
void fill(const Rect&,uint32_t,float){}void fill_vgradient(const Rect&r,uint32_t,uint32_t,float){gradient_rects.push_back(r);}
void push_scissor(const Rect&){}void pop_scissor(){}
void stroke_shape(const Shape&,float,uint32_t){}
void fill_shape(const Shape &s,uint32_t c){shape_fills.push_back({s,c});}
}
using S=ui::SettingsScreen;using A=ui::SettingsRegressionAccess;
static bool has(const std::string&s){for(const auto&t:texts)if(std::get<2>(t)==s)return true;return false;}
static void draw(S&s){texts.clear();gradient_rects.clear();s.draw(5,.2f);
    for(const auto &r:gradient_rects)if(r.x>=1208&&r.w<300&&r.h>100){
        assert(std::abs(r.h-r.w*1.5f)<.01f);assert(r.y+r.h+27<=830); // no distorted or clipped preview posters
    }
}
static void system(S&s){s.activate();s.set_focused(true);s.input(NUVIO_BTN_DOWN);s.input(NUVIO_BTN_DOWN);
 assert(A::tile(s)==4);s.input(NUVIO_BTN_CROSS);assert(A::section(s)==7&&A::row(s)==S::Appearance&&!A::tiles(s));}
int main(){
 char p[]="/tmp/emby5-settings-ui-XXXXXX";assert(mkdtemp(p));root=p;
 jf::Client c("http://test:8096","test","PS5");c.set_session("token","user","User");
 S s(c);s.activate();s.set_focused(true);draw(s);
 assert(has("System")&&has("Live TV")&&!has("Appearance")&&!has("About")&&!has("VOD"));
 system(s);draw(s);assert(has("Appearance")&&has("What's New")&&has("Installed version")&&!has("About")&&has("Check for updates")&&has("Connected server")&&has("Persistent storage"));
 s.input(NUVIO_BTN_CROSS);assert(preview_on&&A::section(s)==4);draw(s);
 assert(has("Settings / System / Appearance")&&has("Themes & Colours")&&has("Buttons & Panels"));
 state.local.max_mbps=40;state.local.sub_size=140;state.local.upscale_mode=3;
 A::to_row(s,S::Accent);s.input(NUVIO_BTN_RIGHT);assert(preview.theme_accent==1&&state.local.theme_accent==0&&saves==0);
 s.input(NUVIO_BTN_CIRCLE);s.input(NUVIO_BTN_CIRCLE);assert(A::confirm(s)&&preview_on);
 // The themed modal still defaults to Keep editing. Cross must not discard changes.
 s.input(NUVIO_BTN_CROSS);assert(!A::confirm(s)&&A::dirty(s));
 s.input(NUVIO_BTN_CIRCLE);s.input(NUVIO_BTN_UP);s.input(NUVIO_BTN_CROSS);
 assert(A::section(s)==7&&!preview_on&&state.local.theme_accent==0&&saves==0);
 s.input(NUVIO_BTN_CROSS);A::to_row(s,S::Accent);s.input(NUVIO_BTN_RIGHT);
 A::to_row(s,S::UiTextSize);s.input(NUVIO_BTN_RIGHT);assert(preview.ui_text_percent==110);
 A::to_row(s,S::SpoilerProtection);s.input(NUVIO_BTN_RIGHT);assert(preview.spoiler_protection);
 A::to_row(s,S::TitleStyle);s.input(NUVIO_BTN_RIGHT);assert(preview.title_as_text);
 A::to_row(s,S::DetailPoster);s.input(NUVIO_BTN_RIGHT);assert(preview.detail_poster);
 A::to_row(s,S::ButtonShape);s.input(NUVIO_BTN_RIGHT);assert(preview.button_shape==1);
 A::to_row(s,S::ColourVariant);s.input(NUVIO_BTN_RIGHT);assert(preview.colour_variant==1);
 A::to_row(s,S::SaveMyTheme);s.input(NUVIO_BTN_CROSS);assert(!A::confirm(s));
 state.local.max_mbps=80;s.input(NUVIO_BTN_OPTIONS);
 assert(!preview_on&&saves==1&&state.local.my_theme_valid&&state.local.theme_accent==1&&state.local.button_shape==1);
 assert(state.local.max_mbps==80&&state.local.sub_size==140&&state.local.upscale_mode==3);
 s.input(NUVIO_BTN_CROSS);A::to_row(s,S::Theme);s.input(NUVIO_BTN_RIGHT);assert(preview.theme==(settings::kDefaultTheme+1)%ui::kThemeCount&&preview.theme_accent==0);
 A::to_row(s,S::LoadMyTheme);s.input(NUVIO_BTN_CROSS);assert(preview.theme==settings::kDefaultTheme&&preview.theme_accent==1&&preview.detail_poster);
 A::to_row(s,S::SaveMyTheme);s.input(NUVIO_BTN_CROSS);assert(A::confirm(s));
 s.input(NUVIO_BTN_CROSS);assert(!A::confirm(s)); // default keeps saved slot
 A::to_row(s,S::CustomSlot);s.input(NUVIO_BTN_RIGHT);assert(A::selected_slot(s)==1);
 A::to_row(s,S::RenameMyTheme);s.input(NUVIO_BTN_CROSS);assert(keyboard);keyboard("Evening");
 A::to_row(s,S::SaveMyTheme);s.input(NUVIO_BTN_CROSS);s.input(NUVIO_BTN_OPTIONS);
 assert(saves==2&&state.local.custom_themes[1].valid&&state.local.custom_themes[1].name=="Evening");
 s.input(NUVIO_BTN_CROSS);A::to_row(s,S::ResetAppearance);s.input(NUVIO_BTN_CROSS);assert(A::confirm(s));
 s.input(NUVIO_BTN_UP);s.input(NUVIO_BTN_CROSS);assert(preview.theme_accent==0&&!preview.detail_poster);
 A::to_row(s,S::RestoreAppearance);s.input(NUVIO_BTN_CROSS);assert(preview.theme_accent==1&&preview.detail_poster);
 s.input(NUVIO_BTN_CIRCLE);s.input(NUVIO_BTN_CIRCLE);assert(!preview_on&&saves==2);
 // All groups/controls are reachable by controller, and all supported presets can be previewed.
 for(int preset=0;preset<ui::kThemeCount;++preset){s.input(NUVIO_BTN_CROSS);A::to_row(s,S::Theme);
   for(int i=0;i<preset;++i)s.input(NUVIO_BTN_RIGHT);
   for(auto r:{S::ColourVariant,S::ButtonShape,S::Background,S::BackgroundArt,S::BackdropDim,S::PosterSize,S::ControlStyle,S::FocusOutline,S::PosterEnlarge,S::ReducedMotion,S::DescriptionScroll,S::PreviewScene,S::CompareAppearance}){
       A::to_row(s,r);s.input(NUVIO_BTN_RIGHT);draw(s);}
   s.input(NUVIO_BTN_CIRCLE);s.input(NUVIO_BTN_CIRCLE);assert(A::confirm(s));
   s.input(NUVIO_BTN_UP);s.input(NUVIO_BTN_CROSS);assert(saves==2&&state.local.theme==settings::kDefaultTheme&&state.local.theme_accent==1);
 }
 // System has exactly the agreed rows; no About page or editable storage field.
 A::to_row(s,S::CheckUpdates);assert(state.local.check_updates);
 s.input(NUVIO_BTN_LEFT);assert(!state.local.check_updates&&update_switches.back()==false);
 A::to_row(s,S::UpdateNotes);s.input(NUVIO_BTN_CROSS);draw(s);
 assert(notes_requests==1&&!state.local.check_updates);s.input(NUVIO_BTN_CIRCLE);
 A::to_row(s,S::CheckUpdates);s.input(NUVIO_BTN_CROSS);
 assert(state.local.check_updates&&update_switches.back()==true);
 const auto unchanged_saves=saves;
 s.set_server_info("Music room","4.9");
 A::to_row(s,S::ServerInfo);s.input(NUVIO_BTN_CROSS);s.input(NUVIO_BTN_LEFT);draw(s);
 assert(A::section(s)==7&&has("Emby - Music room  4.9")&&saves==unchanged_saves);
 A::to_row(s,S::StorageInfo);s.input(NUVIO_BTN_RIGHT);s.input(NUVIO_BTN_CROSS);draw(s);
 assert(A::section(s)==7&&has("/data/Emby5/")&&saves==unchanged_saves&&!has("About"));
 // Read-only identity changes with the connected backend; no credentials are printed.
 { jf::Client jf("http://jellyfin:8096/base","d","p",jf::ServerKind::Jellyfin);
   S js(jf);js.activate();system(js);js.set_server_info("TV room","10.10");draw(js);
   assert(has("Jellyfin - TV room  10.10")); }

 // Root -> Live TV -> bottom VOD rows, not a top-level VOD settings tile.
 s.activate();s.input(NUVIO_BTN_DOWN);s.input(NUVIO_BTN_RIGHT);assert(A::tile(s)==3);s.input(NUVIO_BTN_CROSS);
 A::to_row(s,S::VODStatus);draw(s);assert(has("VOD settings")&&has("Xtream movies")&&has("Xtream TV shows"));
 s.input(NUVIO_BTN_CROSS);assert(vod_retry==1);A::to_row(s,S::VODInterval);s.input(NUVIO_BTN_RIGHT);assert(interval==48);
 // Playback controls must still write the original server preferences.
 s.activate();s.input(NUVIO_BTN_RIGHT);s.input(NUVIO_BTN_CROSS);A::to_row(s,S::SubMode);s.input(NUVIO_BTN_RIGHT);
 A::to_row(s,S::Autoplay);s.input(NUVIO_BTN_RIGHT);assert(server_saves==2);
 // 50k channels: filtered/catalogue arrays are retained while holding and drawing.
 auto catalog=std::make_shared<iptv_xtream::Catalog>();for(int i=0;i<50000;++i){iptv_xtream::Channel ch;ch.id=std::to_string(i);ch.name="Channel "+ch.id;catalog->channels.push_back(ch);}
 A::channels(s,catalog);const auto ids=A::indices(s),names=A::searches(s),base=A::catalog(s);
 s.input(NUVIO_BTN_DOWN);assert(A::selected(s)==1);
 s.update_hold(NUVIO_BTN_DOWN,10);int early=0,late=0;
 for(int i=1;i<=360;++i){int before=A::selected(s);s.update_hold(NUVIO_BTN_DOWN,10+i/60.);
   if(i<=60)early+=A::selected(s)-before;if(i>300)late+=A::selected(s)-before;
   if(i%5==0)draw(s);assert(A::indices(s)==ids&&A::searches(s)==names&&A::catalog(s)==base);}
 assert(early<=2&&late>=20&&A::assigned(s)==0&&A::count(s)==50000);
 s.update_hold(0,17);assert(!s.accelerating(17));int at=A::selected(s);s.update_hold(NUVIO_BTN_UP,18);assert(A::selected(s)==at&&!s.accelerating(18));
 s.input(NUVIO_BTN_CROSS);assert(A::assigned(s)==1);s.input(NUVIO_BTN_CROSS);assert(A::assigned(s)==0);
 s.input(NUVIO_BTN_TRIANGLE);assert(keyboard);keyboard("Channel 49999");draw(s);assert(A::count(s)==1);
 s.input(NUVIO_BTN_SQUARE);draw(s);assert(A::count(s)==50000);
 // Full text can be read manually with auto-scrolling disabled/reduced motion on.
 ui::DescriptionReader reader;std::string body;for(int i=0;i<100;++i)body+="Line "+std::to_string(i)+"\n";
 reader.open("Long description",body);texts.clear();reader.draw();assert(reader.active()&&has("Line 0"));
 for(int i=0;i<120;++i)reader.input(NUVIO_BTN_DOWN);texts.clear();reader.draw();assert(has("Line 99"));
 reader.input(NUVIO_BTN_LEFT);assert(reader.first_line()<90);reader.input(NUVIO_BTN_CIRCLE);assert(!reader.active());
 // The actual confirmation renderer must use the draft palette and shape;
 // compare mode must not substitute the original theme or the old blue skin.
 int dialogs=0;
 for(int theme=0;theme<ui::kThemeCount;++theme)for(int shape=0;shape<6;++shape)
 for(int scale:{90,100,110,120})for(int kind=0;kind<3;++kind)for(bool compare:{false,true}){
   A::modal(s,theme,shape,scale,kind,compare);
   const auto expected=ui::theme_palette();const auto draft=preview;
   const float before_scale=gfx::text_scale();
   panels.clear();focus_shapes.clear();shape_fills.clear();text_colours.clear();
   const int before_saves=saves;
   A::draw_modal(s);
   assert(panels.size()==(kind==0?4u:3u));assert(focus_shapes.size()==1);
   assert(!shape_fills.empty()&&shape_fills.front().second==expected.panel);
   const auto box=panels.front().rect;
   assert(box.x>=0&&box.y>=0&&box.x+box.w<=gfx::W&&box.y+box.h<=gfx::H);
   for(size_t i=0;i<panels.size();++i){
     const auto &drawn=panels[i];assert(drawn.palette.accent==expected.accent&&drawn.palette.panel==expected.panel);
     if(i){assert(drawn.rect.x>box.x&&drawn.rect.y>box.y&&drawn.rect.x+drawn.rect.w<box.x+box.w);
       assert(drawn.rect.y+drawn.rect.h<box.y+box.h);
       assert(drawn.shape.chamfer==(shape==5));
       if(shape==1)assert(drawn.shape.corner==0);
       if(shape==4)assert(std::abs(drawn.shape.corner-drawn.rect.h*.5f)<.01f);
       if(drawn.focus>0){assert(drawn.shape.corner==focus_shapes[0].corner&&drawn.shape.chamfer==focus_shapes[0].chamfer);}
     }
   }
   assert(text_colours.front()==expected.text);
   assert(settings::same_appearance(draft,preview)&&gfx::text_scale()==before_scale&&saves==before_saves);
   s.input(NUVIO_BTN_CROSS); // safe default neither applies nor overwrites/resets
   assert(saves==before_saves&&settings::same_appearance(draft,preview));
   A::close_modal(s);++dialogs;
 }
 assert(dialogs==4608);
 std::cout<<"PASS: 4608 themed confirmation draws, palettes/shapes/text sizes/compare, safe defaults and non-saving render\n";
 std::filesystem::remove_all(root);
 std::cout<<"PASS: actual Settings navigation/System hierarchy/VOD placement; grouped appearance/themed confirmation/Apply/Cancel/three named slots/reset/compare; all preset previews; playback controls; 50000-channel cached accelerated navigation/search/assignment and manual full description\n";
}
