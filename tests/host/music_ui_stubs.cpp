/* Graphics doubles: networking, grid workers and music state are production code. */
#include "music_ui_stubs.h"
#include "ui/album.h"
#include "gfx/art.h"
#include "app/settings.h"
const char *T(const char *s){return s;}
namespace settings { Appearance get_appearance(){return {};}}
namespace ui {
uint32_t theme_bg(){return 0xff000000;}uint32_t theme_text(){return 0xffffffff;}
uint32_t theme_text2(){return 0xffdddddd;}uint32_t theme_text3(){return 0xffaaaaaa;}
bool theme_reduced_motion(){return true;}float theme_card_scale(){return 1;}float theme_poster_lift(float v){return v;}
int theme_grid_columns(){return 6;}
uint32_t alpha(uint32_t c,float){return c;}
float Lifts::step(const std::string&,bool,float,bool*){return 0;}
void Drop::to(const gfx::Rect&,int,float,float){}void Drop::draw(float,float,bool*,float){}
void Ambient::set(const std::string&,double){}void Ambient::draw(float,float,bool*){}
void glass_panel(const gfx::Rect&,float,float,bool,float){}
void draw_focus_outline(const gfx::Rect&,float,float){}
void draw_check(float,float,float,uint32_t){}
float pill_bar(float,float,const std::vector<std::string>&,int,bool,Drop&,float,float,bool*,float,float,float){return 300;}
float draw_pad_hints(float,float,const std::vector<PadHint>&,int,float,float){return 0;}
std::string poster_url(jf::Client &c,const jf::Item &item,int){
    // Match screen.cpp: tests must receive the normalized display item, not
    // silently correct an untagged ParentId here in the graphics double.
    return c.image_url(item.primary_owner.empty()?item.id:item.primary_owner,
                       "Primary",item.primary_tag,800,true);
}
void draw_poster(jf::Client &c,const jf::Item &item,const gfx::Rect&,float,float){music_test::posters.push_back(item);music_test::covers.push_back(poster_url(c,item,480));}
void ItemMenu::input(uint32_t,Action*){}void ItemMenu::draw(float,bool*){}void ItemMenu::open(const jf::Item&,bool){}
}
namespace gfx {
void fill(const Rect&,uint32_t,float){}void fill_vgradient(const Rect&,uint32_t,uint32_t,float){}
void fill_hgradient(const Rect&,uint32_t,uint32_t,float){}void image(const Rect&,const Texture*,float,float,bool){}
void shadow(const Rect&,float,float,float,float){}void push_scissor(const Rect&){}void pop_scissor(){}
void push_opacity(float){}void pop_opacity(){}
float text(float,float,const std::string &s,const TextStyle&,uint32_t,int){return s.size()*10;}
float text_width(const std::string &s,const TextStyle&){return s.size()*10;}
}
namespace art {
bool failed(const std::string &url){return music_test::failed_images.count(url)!=0;}
bool animating(){return false;}
const gfx::Texture *blurhash(const std::string&){return nullptr;}
void draw(const gfx::Rect&,const std::string &url,const std::string&,int,int,float,float,uint32_t){music_test::covers.push_back(url);}
}
