/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "gfx/gfx.h"
namespace ui {
enum class DetailIcon { Play, Check, Heart };
// Original vector paths rasterised once at 4x, not little squares/bars per frame.
void draw_detail_icon(DetailIcon icon, const gfx::Rect &rect, uint32_t color);
}
