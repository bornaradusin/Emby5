/* Optional software preview of production theme/control dispatch. Not PS5 output.
 * Build like compact_controls_test.cpp, run with an output directory argument.
 * Uses shared geometry and SVG, with no distributed font or GPU allocation. */
#include "ui/screen.h"
#include "gfx/shape_geometry.h"
#include "app/settings.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
static settings::Appearance appearance;
static std::ofstream svg;static int gradient_id;
namespace settings {All get(){All s;static_cast<Appearance&>(s.local)=appearance;return s;}Appearance get_appearance(){return appearance;}}
static std::string rgb(uint32_t c){std::ostringstream s;s<<'#'<<std::hex<<std::setw(6)<<std::setfill('0')<<(c&0xffffffu);return s.str();}
static std::string perimeter(const gfx::Shape&s){std::ostringstream o;const auto p=gfx::shape_geometry::boundary(s);for(int i=0;i<p.count;++i)o<<(i?'L':'M')<<p.points[i].x<<','<<p.points[i].y;return o.str()+"Z";}
namespace gfx{
Shape inset_shape(const Shape&s,float n){return shape_geometry::inset(s,n);}
void fill_shape(const Shape&s,uint32_t c){svg<<"<path d='"<<perimeter(s)<<"' fill='"<<rgb(c)<<"' fill-opacity='"<<(c>>24)/255.<<"'/>\n";}
void gradient_shape(const Shape&s,uint32_t a,uint32_t b){const int id=gradient_id++;
 svg<<"<defs><linearGradient id='g"<<id<<"' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='"<<rgb(a)<<"' stop-opacity='"<<(a>>24)/255.<<"'/><stop offset='1' stop-color='"<<rgb(b)<<"' stop-opacity='"<<(b>>24)/255.<<"'/></linearGradient></defs>\n";
 svg<<"<path d='"<<perimeter(s)<<"' fill='url(#g"<<id<<")'/>\n";}
void stroke_shape(const Shape&s,float w,uint32_t c){svg<<"<path d='"<<perimeter(s)<<perimeter(inset_shape(s,w))<<"' fill-rule='evenodd' fill='"<<rgb(c)<<"' fill-opacity='"<<(c>>24)/255.<<"'/>\n";}
void shadow_shape(const Shape&input,float blur,float opacity,float dy){auto s=input;s.rect.y+=dy;for(int i=5;i>=0;--i){const float ext=blur*(i+1)/6,alpha=std::min(1.f,opacity*(.025f+.012f*(5-i)));fill_shape(inset_shape(s,-ext),(uint32_t)(alpha*255+.5f)<<24);}}
void fill(const Rect&r,uint32_t c,float rad){fill_shape({r,rad,false},c);}void fill_vgradient(const Rect&r,uint32_t a,uint32_t b,float rad){gradient_shape({r,rad,false},a,b);}
void shadow(const Rect&r,float rad,float b,float a,float y){shadow_shape({r,rad,false},b,a,y);}void rim(const Rect&r,float rad,float a){stroke_shape({r,rad,false},1,(uint32_t)(a*80)<<24|0xffffffu);}
int backdrop_blur(const Rect&,float,float,float,float){return 0;}
}
static void text(float x,float y,const std::string &t,uint32_t c,int size){svg<<"<text x='"<<x<<"' y='"<<y<<"' fill='"<<rgb(c)<<"' font-family='sans-serif' font-size='"<<size<<"'>";for(char ch:t){if(ch=='&')svg<<"&amp;";else if(ch=='<')svg<<"&lt;";else svg<<ch;}svg<<"</text>\n";}
static void open(const std::filesystem::path &p,int w,int h){svg.open(p);gradient_id=0;svg<<"<svg xmlns='http://www.w3.org/2000/svg' width='"<<w<<"' height='"<<h<<"' viewBox='0 0 "<<w<<' '<<h<<"'>\n";}
static void button(gfx::Rect r,const char*label,bool focus){const auto p=ui::theme_palette();ui::glass_panel(r,16,1,true,focus?1:0);if(focus)ui::draw_focus_outline(r,16,1);text(r.x+24,r.y+34,label,p.text,20);}
int main(int argc,char**argv){if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);
 for(int page=0;page<3;++page){open(std::filesystem::path(argv[1])/("presets-"+std::to_string(page+1)+".svg"),1920,1200);gfx::fill({0,0,1920,1200},0xff090d15,0);
 for(int k=0;k<12;++k){int id=page*12+k;if(id>=ui::kThemeCount)break;appearance={};appearance.theme=id;auto p=ui::theme_palette();float x=(k%3)*640,y=(k/3)*300;
 gfx::fill({x+8,y+8,624,284},p.page,12);text(x+30,y+45,std::to_string(id+1)+". "+p.name,p.text,25);
 button({x+34,y+73,480,58},"Ordinary button",false);button({x+34,y+162,480,58},"Selected button",true);button({x+540,y+162,58,58},"+",false);
 text(x+34,y+263,"Preset default geometry and material",ui::theme_secondary_colour(p),18);}
 svg<<"</svg>";svg.close();}
 open(std::filesystem::path(argv[1])/"shapes.svg",1920,1020);
 const int themes[]={settings::kDefaultTheme,2,7,22,28,30};
 for(int row=0;row<6;++row)for(int col=0;col<6;++col){appearance={};appearance.theme=themes[row];appearance.button_shape=col;auto p=ui::theme_palette();float x=col*320,y=row*170;
 gfx::fill({x,y,320,170},p.page,0);text(x+16,y+29,std::string(p.name)+" / "+ui::theme_shape_name(col),p.text,14);
 button({x+18,y+64,284,65},"Focused",true);}
 svg<<"</svg>";svg.close();
 open(std::filesystem::path(argv[1])/"backgrounds.svg",1920,1200);
 for(int i=0;i<ui::kBackgroundCount;++i){appearance={};appearance.theme_background=i;
    const auto p=ui::theme_palette();const float x=(i%4)*480,y=(i/4)*300;
    gfx::fill({x,y,480,300},p.page,0);text(x+20,y+40,ui::theme_background_name(i),p.text,24);
    button({x+24,y+85,430,64},"Emby5",false);button({x+24,y+181,430,64},"Selected channel",true);
 }
 svg<<"</svg>";svg.close();std::cout<<"Wrote software-only surface galleries to "<<argv[1]<<"\n";
}
