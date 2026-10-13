/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/browse_grid.h"
namespace ui {
// Horizontal shelves retain provider/custom order. The shared card drawing
// keeps the library's focus colour, lift and poster-size preferences.
struct BrowseRowsGeometry : BrowseGridGeometry {
    float row_pitch, first_y;
    explicit BrowseRowsGeometry(float aspect = 2.f/3.f, float y = 300.f)
        : BrowseGridGeometry(aspect), first_y(y) {
        row_pitch = height + 128.f * std::max(1.f, theme_text_scale());
    }
    float vertical(int row, bool header=false) const {
        return header || row <= 0 ? 0.f : row * row_pitch - 42.f;
    }
    float horizontal(int index) const {
        // Keep whole selected windows (and their enlarged outlines) on screen.
        return (std::max(0,index) / cols) * cols * (width + gap);
    }
    gfx::Rect card(int row,int column,float sy,float sx) const {
        return {kPad+column*(width+gap)-sx,first_y+row*row_pitch-sy,width,height};
    }
    std::pair<int,int> rows(int count,float sy,float clip_top,float bottom) const {
        int first=std::max(0,(int)std::floor((clip_top+sy-first_y-height-85.f)/row_pitch));
        int last=std::min(count,std::max(0,(int)std::ceil((bottom+sy-first_y+85.f)/row_pitch)));
        return {std::min(first,count),last};
    }
    std::pair<int,int> columns(int count,float sx) const {
        int first=std::max(0,(int)std::floor((sx-width*.1f)/(width+gap)));
        return {std::min(first,count),std::min(count,first+cols+2)};
    }
};
}
