/* Shared geometry: input column count and rendered cards must stay in sync. */
#pragma once
#include "ui/theme_presets.h"
namespace ui {
struct LibraryGeometry {
    int cols;
    float gap, width, height, pitch;
    LibraryGeometry(float canvas_width, float padding)
        : cols(theme_grid_columns()), gap(48.f),
          width((canvas_width-2*padding-(cols-1)*gap)/cols),
          height(width*1.5f), pitch(height+90.f) {}
};
}
