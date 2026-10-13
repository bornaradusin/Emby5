/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later
 * Shared, side-effect-free geometry for the season picker and episode viewport.
 * Thumbnails stay 16:9; text and focus halos get their own room at every UI size.
 */
#pragma once
#include <algorithm>
#include <cmath>

namespace ui::detail_layout {
inline float text_scale(float value) {
    return std::isfinite(value) ? std::clamp(value, .9f, 1.2f) : 1.f;
}
inline int visible_first(int selected, int first, int count, int capacity) {
    if (count <= 0) return 0;
    capacity = std::max(1, capacity);
    selected = std::clamp(selected, 0, count - 1);
    first = std::clamp(first, 0, std::max(0, count - capacity));
    if (selected < first) first = selected;
    if (selected >= first + capacity) first = selected - capacity + 1;
    return first;
}
inline float tab_scroll(float start, float width, float viewport, float extent, float previous) {
    const float max_scroll = std::max(0.f, extent - viewport);
    float scroll = std::clamp(previous, 0.f, max_scroll);
    if (start < scroll) scroll = start;
    if (start + width > scroll + viewport) scroll = start + width - viewport;
    return std::clamp(scroll, 0.f, max_scroll);
}
struct Episodes {
    float width, height, gap = 32.f, inset = 36.f;
    float image_y, title_y, runtime_y, synopsis_y, section_height;
    int capacity;
    Episodes(float viewport, float scale, float card_scale) {
        scale = text_scale(scale);
        card_scale = std::isfinite(card_scale) ? std::clamp(card_scale, .86f, 1.16f) : 1.f;
        width = std::min(480.f * card_scale, std::max(1.f, viewport - 2.f * inset));
        height = width * 9.f / 16.f;
        capacity = std::max(1, int((viewport - 2.f * inset + gap) / (width + gap)));
        image_y = 42.f * scale + 18.f;
        title_y = image_y + height + 44.f * scale;
        runtime_y = title_y + 28.f * scale;
        synopsis_y = runtime_y + 30.f * scale;
        section_height = synopsis_y + (2.f * 27.f + 23.f) * scale + 28.f;
    }
};
inline float seasons_height(float scale, bool empty) {
    return 48.f * text_scale(scale) + 54.f * text_scale(scale) + 18.f + (empty ? 64.f : 0.f);
}
} // namespace ui::detail_layout
