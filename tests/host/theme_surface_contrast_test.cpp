/* Actual production materials composited at the surface centre; no native GPU. */
#include "ui/screen.h"
#include "gfx/shape_geometry.h"
#include "app/settings.h"
#include <cstdio>
static settings::Appearance appearance;static unsigned center;static unsigned label_ink;
namespace settings {All get(){All s;static_cast<Appearance&>(s.local)=appearance;return s;} Appearance get_appearance(){return appearance;}}
static unsigned over(unsigned c,unsigned b){unsigned o=0xff000000;double a=(c>>24)/255.;for(int s=0;s<24;s+=8)o|=(unsigned)((((c>>s)&255)*a)+(((b>>s)&255)*(1-a))+.5)<<s;return o;}
namespace gfx{
float text(float,float,const std::string&,const TextStyle&,uint32_t c,int){label_ink=c;return 40;}
float text_width(const std::string&,const TextStyle&){return 40;}
Shape inset_shape(const Shape&s,float n){return shape_geometry::inset(s,n);}
void fill_shape(const Shape&,uint32_t c){center=over(c,center);}
void gradient_shape(const Shape&,uint32_t a,uint32_t b){unsigned c=0;for(int s=0;s<32;s+=8)c|=((((a>>s)&255)+((b>>s)&255))/2)<<s;center=over(c,center);}
void stroke_shape(const Shape&,float,uint32_t){}void shadow_shape(const Shape&,float,float,float){}
void fill(const Rect&,uint32_t,float){}void fill_vgradient(const Rect&,uint32_t,uint32_t,float){}void shadow(const Rect&,float,float,float,float){}void rim(const Rect&,float,float){}int backdrop_blur(const Rect&,float,float,float,float){return 0;}
}
int main(){double min=100;int fails=0;for(int t=0;t<ui::kThemeCount;++t)for(int v=0;v<6;++v)for(int b=0;b<ui::kBackgroundCount;++b)for(int a=0;a<9;++a)for(int m=0;m<3;++m)for(int f=0;f<2;++f){appearance={};appearance.theme=t;appearance.colour_variant=v;appearance.theme_background=b;appearance.theme_accent=a;appearance.control_style=m;auto p=ui::theme_palette();center=p.page;ui::glass_panel({0,0,380,76},16,1,false,f);auto ratio=ui::colour_contrast(p.text,center);if(ratio<min)min=ratio;if(ratio<4.5){if(++fails<10)printf("fail theme%d v%d b%d a%d m%d focus%d ratio%f\n",t,v,b,a,m,f,ratio);}}
for(int t=0;t<ui::kThemeCount;++t){appearance={};appearance.theme=t;ui::draw_label_chip(0,0,"Available",0xff44cc77,1,18);if(label_ink!=0xfff5f7fbu)++fails;}
printf("PASS: all production material/selection composites; minimum centre text contrast %.2f; %d failures\n",min,fails);return fails?1:0;}
