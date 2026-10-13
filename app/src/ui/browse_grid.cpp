/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/browse_grid.h"
#include "gfx/art.h"
namespace ui {
void draw_browse_card(const gfx::Rect &base, const std::string &title,
                     const std::string &artwork, bool logo, float lift, float opacity) {
    const auto r = browse_lifted_rect(base, lift);
    const float radius = 14.f * (r.w / base.w);
    if (lift > .01f) gfx::shadow(r, radius, 26, .3f * lift * opacity, 10 * lift);
    draw_glass_placeholder(r, radius, opacity);
    if (logo) {
        if (!artwork.empty()) {
            if (const auto *image = art::get(artwork, 480, 270)) {
                const float width = (float)gfx::texture_width(image), height = (float)gfx::texture_height(image);
                if (width > 0 && height > 0) {
                    const float fit = std::min((r.w - 28) / width, (r.h - 20) / height);
                    gfx::image({r.x + (r.w - width * fit)/2, r.y + (r.h - height * fit)/2,
                                width * fit, height * fit}, image, opacity, 0, false);
                }
            }
        }
        if (artwork.empty() || art::failed(artwork))
            gfx::text(r.x + r.w/2, r.y + r.h/2 + 9, title,
                      {gfx::SemiBold, 24, r.w - 28, 2, 29}, alpha(kText2, opacity), 1);
    } else {
        if (!artwork.empty()) art::draw(r, artwork, "", 480, 720, radius, opacity, 0);
        if (artwork.empty() || art::failed(artwork))
            gfx::text(r.x + 18, r.y + 52, title,
                      {gfx::SemiBold, 22, r.w - 36, 4, 28}, alpha(kText2, opacity));
    }
    if (lift > .01f) draw_image_focus_outline(r, radius, opacity * lift);
    gfx::text(r.x, r.y + r.h + 34, title, {gfx::SemiBold, 20, r.w},
              alpha(lift > .5f ? kText : kText2, opacity));
}
} // namespace ui
