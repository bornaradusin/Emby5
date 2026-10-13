/* Emby5 - GPL-3.0-or-later. Bounded, texture-free control geometry. */
#pragma once
#include "gfx/gfx.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace gfx { namespace shape_geometry {
constexpr int arc_steps=16, max_points=4*(arc_steps+1);
struct Point {float x=0,y=0;};
struct Path {std::array<Point,max_points> points{};int count=0;};
inline Shape normalise(Shape s) {
    auto &r=s.rect;
    if(!std::isfinite(r.x)||!std::isfinite(r.y)||!std::isfinite(r.w)||!std::isfinite(r.h))return {};
    r.w=std::max(0.f,r.w);r.h=std::max(0.f,r.h);
    s.corner=std::isfinite(s.corner)?std::max(0.f,std::min(s.corner,std::min(r.w,r.h)/2)):0;
    return s;
}
inline Shape inset(Shape s,float d) {
    s=normalise(s);if(!std::isfinite(d))return {};
    s.rect={s.rect.x+d,s.rect.y+d,std::max(0.f,s.rect.w-2*d),std::max(0.f,s.rect.h-2*d)};
    // Diagonal chamfers offset perpendicular to the edge; square corners stay square.
    if(s.corner>0)s.corner=std::max(0.f,s.corner-d*(s.chamfer?0.5857864376f:1.f));
    return normalise(s);
}
// Fixed topology per corner type. Coincident points at zero-radius are intentional;
// outer and inner rings must stay paired even when the inner corner collapses.
inline Path boundary(Shape input) {
    const auto s=normalise(input);const auto r=s.rect;const float c=s.corner;
    Path p;
    if(s.chamfer) {
        p.count=8;
        p.points[0]={r.x+c,r.y};p.points[1]={r.x+r.w-c,r.y};
        p.points[2]={r.x+r.w,r.y+c};p.points[3]={r.x+r.w,r.y+r.h-c};
        p.points[4]={r.x+r.w-c,r.y+r.h};p.points[5]={r.x+c,r.y+r.h};
        p.points[6]={r.x,r.y+r.h-c};p.points[7]={r.x,r.y+c};
    } else {
        const float cx[]={r.x+r.w-c,r.x+r.w-c,r.x+c,r.x+c};
        const float cy[]={r.y+c,r.y+r.h-c,r.y+r.h-c,r.y+c};
        for(int k=0;k<4;++k)for(int i=0;i<=arc_steps;++i){
            const float a=(-1.f+k+float(i)/arc_steps)*1.5707963267948966f;
            p.points[p.count++]={cx[k]+c*std::cos(a),cy[k]+c*std::sin(a)};
        }
    }
    return p;
}
}}
