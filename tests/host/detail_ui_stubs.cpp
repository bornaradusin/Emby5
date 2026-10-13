/* Rendering/service doubles for testing the real Detail page and HTTP client. */
#include "ui/detail.h"
#include "ui/detail_icons.h"
#include "app/seerr_service.h"
#include "gfx/art.h"
#include "ui/theme_presets.h"
#include <string>
#include "app/settings.h"
#include "detail_draw_trace.h"
settings::Appearance detail_test_appearance;
namespace settings {Appearance get_appearance(){return detail_test_appearance;}}
const char *T(const char *s) {return s;}
void emby5_wait_reports(int) {}
namespace seerr_service {
bool ready(){return false;} std::shared_ptr<seerr::Client> client(){return {};}
Snapshot snapshot(){return {}; } void session_lost(){}
}
namespace seerr {
std::vector<Title> Client::search(const std::string &, int,int *){return {};}
bool Client::tv(int,Detail *){return false;}
int Client::last_status() const{return 0;}
}
namespace ui {
const ThemePalette &theme_palette(){static ThemePalette p{};return p;}
bool theme_reduced_motion(){return detail_test_appearance.reduced_motion;}
bool theme_artwork_enabled(){return detail_test_appearance.background_artwork;}
float theme_backdrop_dim(float d){return d;}
float theme_card_scale(){return detail_test_appearance.poster_size==0?.86f:detail_test_appearance.poster_size==2?1.16f:1.f;}
float theme_poster_lift(float f){return detail_test_appearance.poster_enlarge?f:0;}
float theme_description_speed(){return detail_test_appearance.description_scroll?18:0;}
void draw_focus_outline(const gfx::Rect &r,float,float){detail_trace::focused_controls.push_back(r);}
void draw_image_focus_outline(const gfx::Rect &r,float,float){detail_trace::focused_images.push_back(r);}
std::string poster_url(jf::Client &, const jf::Item &,int){return {};}

uint32_t theme_bg(){return 0xff000000;} uint32_t theme_text(){return 0xffffffff;}
uint32_t theme_text2(){return 0xffcccccc;} uint32_t theme_text3(){return 0xffaaaaaa;}
uint32_t alpha(uint32_t c,float){return c;}
float Lifts::step(const std::string &,bool,float,bool *){return 0;}
void Drop::to(const gfx::Rect &,int,float,float){} void Drop::draw(float,float,bool *,float){}
void glass_panel(const gfx::Rect &r,float,float,bool,float){detail_trace::panels.push_back(r);}
float pad_hint_width(PadButton,const std::string &,float){return 1;}
float draw_pad_hint(float,float,PadButton,const std::string &,float,float){return 1;}
void draw_note(const std::string &,float,uint32_t){}
void draw_detail_icon(DetailIcon,const gfx::Rect &,uint32_t){}
std::string landscape_url(jf::Client &, const jf::Item &,int){return {};}
std::string landscape_blurhash(const jf::Item &){return {};}
std::string request_note(const seerr::RequestResult &){return {};}
uint32_t request_note_dot(const seerr::RequestResult &){return 0;}
void ItemMenu::input(uint32_t,Action *){}
void ItemMenu::draw(float,bool *){}
bool ItemMenu::take_ask(){return false;}
void ItemMenu::open_actions(const jf::Item &,bool,bool){}
void RequestSheet::open(const seerr::Detail &,const seerr::User &,const seerr::PublicSettings &){}
void RequestSheet::input(uint32_t){}
void RequestSheet::draw(float,bool *){}
bool RequestSheet::take_done(seerr::RequestResult *){return false;}
bool RequestSheet::offers(const seerr::Detail &,const seerr::User &,const seerr::PublicSettings &){return false;}
}
namespace gfx {
float text_scale(){return detail_test_appearance.ui_text_percent/100.f;}
int texture_width(const Texture *){return 1;}int texture_height(const Texture *){return 1;}
void image(const Rect &,const Texture *,float,float,bool){}
void push_scissor(const Rect &r){detail_trace::scissors.push_back(r);++detail_trace::scissor_depth;}void pop_scissor(){--detail_trace::scissor_depth;}void push_opacity(float){}void pop_opacity(){}
void fill(const Rect &,uint32_t,float){}void fill_hgradient(const Rect &,uint32_t,uint32_t,float){}
void fill_vgradient(const Rect &,uint32_t,uint32_t,float){}void shadow(const Rect &,float,float,float,float){}
float text(float x,float y,const std::string &s,const TextStyle &st,uint32_t c,int){detail_trace::texts.push_back({x,y,s,st,c});return 10;}
float text_width(const std::string &s,const TextStyle &){return s.size()*10.f;}
float measure_text(const std::string &s,const TextStyle &st){return s.size()*st.size*.52f*text_scale();}
void push_fade_mask(const Rect &,float,float,float,float){}void pop_fade_mask(){}
}
namespace art {
const gfx::Texture *get(const std::string &,int,int){return nullptr;}
float fade(const std::string &,float){return 1;}
bool animating(){return false;}
void draw(const gfx::Rect &,const std::string &,const std::string &,int,int,float,float,uint32_t){}
}
