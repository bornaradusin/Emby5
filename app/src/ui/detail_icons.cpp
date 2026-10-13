/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/detail_icons.h"
#include "ui_image.h"
#include <cstdio>
#include <cstring>

namespace ui {
void draw_detail_icon(DetailIcon icon, const gfx::Rect &rect, uint32_t color)
{
    // Only the render thread calls this; bounded to three textures.
    struct Entry { gfx::Texture *texture=nullptr; uint32_t rgb=0; };
    static Entry entries[3];
    const unsigned i=static_cast<unsigned>(icon);
    if (i>=3 || rect.w<=0 || rect.h<=0) return;
    auto &entry=entries[i];
    const uint32_t rgb=color & 0xffffffu;
    if (!entry.texture || entry.rgb!=rgb) {
        static const char *paths[] = {
            "<path d='M8 4 L25 16 L8 28 Z' fill='#%06x' stroke='#%06x' stroke-width='1.2' stroke-linejoin='round'/>",
            "<path d='M5 16 L12 23 L27 7' fill='none' stroke='#%06x' stroke-width='3.4' stroke-linecap='round' stroke-linejoin='round'/>",
            "<path d='M16 28 C12 24 3 18 3 11 C3 2 13 2 16 9 C19 2 29 2 29 11 C29 18 20 24 16 28 Z' fill='#%06x'/>"
        };
        char path[320], svg[480];
        std::snprintf(path,sizeof path,paths[i],rgb,rgb);
        std::snprintf(svg,sizeof svg,"<svg xmlns='http://www.w3.org/2000/svg' width='128' height='128' viewBox='0 0 32 32'>%s</svg>",path);
        ui_image pixels{};
        if (ui_image_decode(reinterpret_cast<const uint8_t *>(svg),std::strlen(svg),128,128,&pixels)==0) {
            auto *texture=gfx::texture_from_image(&pixels);
            ui_image_free(&pixels);
            if (texture) {
                gfx::texture_release(entry.texture); // deferred by gfx until in-flight frames finish
                entry.texture=texture; entry.rgb=rgb;
            }
        }
    }
    if (entry.texture) gfx::image(rect,entry.texture,static_cast<float>(color>>24)/255.f,0,false);
}
}
