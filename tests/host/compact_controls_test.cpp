/* Actual theme + surface dispatch: effects are counted, not rendered on PS5. */
#include "ui/screen.h"
#include "gfx/shape_geometry.h"
#include "app/settings.h"
#include <cassert>
#include <iostream>
static int blurs=0,gradients=0,strokes=0;static settings::Appearance appearance;
static gfx::Shape last_fill{};
namespace settings {All get(){All s;static_cast<Appearance&>(s.local)=appearance;return s;}
Appearance get_appearance(){return appearance;}}
namespace gfx {
Shape inset_shape(const Shape&s,float n){return shape_geometry::inset(s,n);}
void fill_shape(const Shape&s,uint32_t){last_fill=s;}
void gradient_shape(const Shape&s,uint32_t,uint32_t){last_fill=s;++gradients;}
void stroke_shape(const Shape&,float,uint32_t){++strokes;}
void shadow_shape(const Shape&,float,float,float){}
void fill(const Rect&,uint32_t,float){}
void fill_vgradient(const Rect&,uint32_t,uint32_t,float){++gradients;}
void shadow(const Rect&,float,float,float,float){}
void rim(const Rect&,float,float){}
int backdrop_blur(const Rect&,float,float,float,float){++blurs;return 2;}
}
int main(){
 for(int theme=0;theme<ui::kThemeCount;++theme)for(int shape=0;shape<6;++shape)for(int style=0;style<3;++style){
   appearance.theme=theme;appearance.button_shape=shape;appearance.control_style=style;
   for(float lift:{0.f,1.f}){
     const int before=blurs;ui::glass_panel({0,0,360,76},16,1,false,lift);
     assert(blurs==before);const auto expected=ui::theme_surface_shape({0,0,360,76},16,true);
     assert(last_fill.corner==expected.corner&&last_fill.chamfer==expected.chamfer);
     ui::draw_focus_outline({0,0,360,76},16,1);
   }
 }
 appearance={};int before=blurs;ui::glass_panel({0,0,700,400},16,1,false);assert(blurs==before+1);
 before=strokes;ui::glass_panel({0,0,360,76},16,0,false);assert(strokes==before);
 std::cout<<"PASS: all supported presets x 6 shapes x 3 materials, compact controls never blur, focus uses shared geometry; large glass retained\n";
}
