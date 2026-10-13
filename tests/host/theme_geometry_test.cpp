/* Production geometry/palette tests, independent of GPU/kernel APIs. */
#include "gfx/shape_geometry.h"
#include "ui/theme_presets.h"
#include "app/appearance.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
static settings::Appearance appearance;
namespace settings {Appearance get_appearance(){return appearance;}}
static void path(gfx::Shape s) {
    const auto normalized=gfx::shape_geometry::normalise(s);
    const auto p=gfx::shape_geometry::boundary(s);
    assert(p.count==(s.chamfer?8:68));
    double signed_area=0;
    for(int i=0;i<p.count;++i) {
        const auto a=p.points[i],b=p.points[(i+1)%p.count];
        assert(std::isfinite(a.x)&&std::isfinite(a.y));
        assert(a.x>=normalized.rect.x-.001f&&a.x<=normalized.rect.x+normalized.rect.w+.001f);
        assert(a.y>=normalized.rect.y-.001f&&a.y<=normalized.rect.y+normalized.rect.h+.001f);
        signed_area+=double(a.x)*b.y-double(b.x)*a.y;
    }
    assert(signed_area>=-.001);
}
int main(){
    unsigned palettes=0,shapes=0;
    for(int preset=0;preset<ui::kThemeCount;++preset)for(int v=0;v<6;++v)for(int bg=0;bg<ui::kBackgroundCount;++bg)
    for(int accent=0;accent<9;++accent)for(int material=0;material<3;++material){
        appearance={};appearance.theme=preset;appearance.colour_variant=v;appearance.theme_background=bg;
        appearance.theme_accent=accent;appearance.control_style=material;
        const auto p=ui::theme_palette();
        assert(ui::colour_contrast(p.text,p.page)>=4.5&&ui::colour_contrast(p.text,p.panel)>=4.5);
        assert(ui::colour_contrast(ui::theme_secondary_colour(p),p.panel)>=4.5);
        assert(ui::colour_contrast(ui::theme_secondary_colour(p,true),p.page)>=4.5);
        assert(ui::colour_contrast(p.text,ui::focus_panel_colour(p,true))>=4.5);
        assert(ui::colour_contrast(ui::theme_focus_colour(),p.panel)>=3.);
        ++palettes;
    }
    for(int preset=0;preset<ui::kThemeCount;++preset)for(int shape=0;shape<6;++shape){
        appearance={};appearance.theme=preset;appearance.button_shape=shape;
        for(const auto rect:{gfx::Rect{10,20,360,76},gfx::Rect{10,20,76,76},gfx::Rect{0,0,980,80},
                            gfx::Rect{0,0,7,2},gfx::Rect{0,0,1,1},gfx::Rect{0,0,0,0}}){
            const auto s=ui::theme_surface_shape(rect,16,true);path(s);
            assert(s.corner<=std::min(rect.w,rect.h)*.5f);
            if(shape==1)assert(s.corner==0&&!s.chamfer);
            if(shape==4)assert(s.corner==std::min(rect.w,rect.h)*.5f);
            if(shape==5)assert(s.chamfer);
            for(float n:{-5.f,-2.f,0.f,1.f,3.f,40.f}){
                const auto t=gfx::shape_geometry::inset(s,n);path(t);
                assert(gfx::shape_geometry::boundary(s).count==gfx::shape_geometry::boundary(t).count);
                if(s.corner==0)assert(t.corner==0);
            }
            ++shapes;
        }
        // The override must not turn large panels, posters or avatars into pills/chamfers.
        const auto panel=ui::theme_surface_shape({0,0,620,560},18,false);
        assert(!panel.chamfer&&panel.corner<=18);
    }
    const auto bad=gfx::shape_geometry::normalise({{0,0,std::numeric_limits<float>::infinity(),4},8,true});
    assert(bad.rect.w==0&&bad.rect.h==0);
    std::cout<<"PASS: "<<palettes<<" palette combinations, "<<shapes<<" control geometries, contrast, continuous paired contours, inset clamps and panel shape isolation\n";
}
