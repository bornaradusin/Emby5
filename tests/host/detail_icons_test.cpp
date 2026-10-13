/* Real application icon paths and NanoSVG rasteriser; GPU upload/draw mocked. */
#include "ui/detail_icons.h"
#include "ui_image.h"
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>
static int decodes=0, draws=0;
namespace gfx { struct Texture {std::vector<uint32_t> pixels;}; }
static std::set<gfx::Texture*> live;
extern "C" int ui_image_decode(const uint8_t *data,size_t count,int w,int h,ui_image *out) {
    ++decodes;assert(w==128 && h==128);
    std::vector<char> xml(count+1);std::memcpy(xml.data(),data,count);
    auto *svg=nsvgParse(xml.data(),"px",96);assert(svg && svg->width==128 && svg->height==128);
    auto *r=nsvgCreateRasterizer();assert(r);
    out->w=w;out->h=h;out->px=static_cast<uint32_t*>(std::calloc(size_t(w)*h,4));assert(out->px);
    nsvgRasterize(r,svg,0,0,1,reinterpret_cast<unsigned char*>(out->px),w,h,4*w);
    nsvgDeleteRasterizer(r);nsvgDelete(svg);
    size_t solid=0,edge=0,clear=0;for(int i=0;i<w*h;++i) {
        unsigned a=out->px[i]>>24;if(a==255)++solid;else if(a==0)++clear;else ++edge;
    }
    assert(solid>100 && edge>30 && clear>1000); // antialiased edges and nonempty path
    return 0;
}
extern "C" void ui_image_free(ui_image *img) {std::free(img->px);*img={};}
namespace gfx {
Texture *texture_from_image(const ui_image *img) {
    auto *t=new Texture;t->pixels.assign(img->px,img->px+size_t(img->w)*img->h);live.insert(t);return t;
}
void texture_release(Texture *t) {if(t){assert(live.erase(t)==1);delete t;}}
void image(const Rect &r,const Texture *t,float opacity,float,bool) {
    assert(r.w==32 && r.h==32 && live.count(const_cast<Texture*>(t)) && opacity>0);++draws;
}
}
int main() {
    const gfx::Rect rect{0,0,32,32};
    for(int i=0;i<100;++i) for(auto icon:{ui::DetailIcon::Play,ui::DetailIcon::Check,ui::DetailIcon::Heart})
        ui::draw_detail_icon(icon,rect,0xffffffff);
    assert(decodes==3 && draws==300 && live.size()==3);
    ui::draw_detail_icon(ui::DetailIcon::Check,rect,0xff30d158);
    ui::draw_detail_icon(ui::DetailIcon::Heart,rect,0xffff5a7a);
    assert(decodes==5 && live.size()==3);
    ui::draw_detail_icon(static_cast<ui::DetailIcon>(999),rect,0xffffffff);assert(decodes==5);
    for(auto *t:live)delete t;live.clear(); // no further calls; process shutdown
    std::cout<<"PASS: three real 4x NanoSVG icons, antialiased edges, bounded texture cache and recolouring\n";
}
