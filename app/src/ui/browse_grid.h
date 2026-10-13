/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/library_layout.h"
#include "ui/screen.h"
#include "nuvio_input.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ui {
// The same columns, gaps, 10% lift and image outline as Movies/TV Shows.
// Catalogue screens vary only the art's aspect ratio and their header/footer.
struct BrowseGridGeometry {
    int cols;
    float width, height, gap, pitch;
    explicit BrowseGridGeometry(float aspect = 2.f / 3.f) {
        const LibraryGeometry library(gfx::W, kPad);
        cols = library.cols; width = library.width; gap = library.gap;
        height = width / aspect;
        pitch = height + 90.f * std::max(1.f, theme_text_scale());
    }
    gfx::Rect tile(int index, float top) const {
        return {kPad + (index % cols) * (width + gap),
                top + (index / cols) * pitch, width, height};
    }
    float scroll_target(int index, bool header) const {
        const int row = std::max(0, index) / cols;
        return !header && row > 0 ? row * pitch - 60.f : 0.f;
    }
    std::pair<int,int> visible(int count, float top, float clip_top, float bottom) const {
        const int first = std::max(0, (int)std::floor((clip_top - top - height - 90.f) / pitch)) * cols;
        const int last = std::min(count, std::max(0, (int)std::ceil((bottom - top + 90.f) / pitch)) * cols);
        return {std::min(first, count), last};
    }
};

inline int grid_count(size_t count) {
    return (int)std::min(count, (size_t)std::numeric_limits<int>::max());
}

// Grid movement deliberately does not wrap horizontally or change groups.
// Circle returns to the first row, then the header, then navigation.
struct BrowseGridMove { bool handled = false, to_nav = false, bump = false; };
inline BrowseGridMove move_browse_grid(uint32_t p, int count, int cols, int &index, bool &header) {
    BrowseGridMove r;
    cols = std::max(1, cols);
    index = std::clamp(index, 0, std::max(0, count - 1));
    if (header) {
        if (p & (NUVIO_BTN_UP | NUVIO_BTN_CIRCLE)) r = {true, true, false};
        else if (p & NUVIO_BTN_DOWN) { if (count) header = false; r.handled = true; }
        return r;
    }
    const int row = index / cols, col = index % cols;
    if (p & NUVIO_BTN_RIGHT) {
        r.handled = true;
        if (col + 1 < cols && index + 1 < count) ++index; else r.bump = true;
    } else if (p & NUVIO_BTN_LEFT) {
        r.handled = true;
        if (col > 0) --index; else r.bump = true;
    } else if (p & NUVIO_BTN_DOWN) {
        r.handled = true;
        if (count > 0 && row + 1 <= (count - 1) / cols) index = (int)std::min((int64_t)index + cols, (int64_t)count - 1);
        else r.bump = true;
    } else if (p & NUVIO_BTN_UP) {
        r.handled = true;
        if (row > 0) index -= cols; else header = true;
    } else if (p & NUVIO_BTN_CIRCLE) {
        r.handled = true;
        if (row > 0) index = col; else header = true;
    }
    return r;
}

inline gfx::Rect browse_lifted_rect(const gfx::Rect &base, float lift) {
    const float scale = 1.f + .1f * theme_poster_lift(lift);
    return {base.x - base.w * (scale - 1.f) / 2.f,
            base.y - base.h * (scale - 1.f) / 2.f, base.w * scale, base.h * scale};
}

// Logos are fitted; movie posters are cropped like library posters. Neither
// inherits the user's button shape: the focus outline follows the image card.
void draw_browse_card(const gfx::Rect &base, const std::string &title,
                      const std::string &artwork, bool logo, float lift,
                      float opacity = 1.f);
} // namespace ui
