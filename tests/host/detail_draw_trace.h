/* Host-only graphics trace for exercising the actual detail-screen layout. */
#pragma once
#include "gfx/gfx.h"
#include <string>
#include <vector>
namespace detail_trace {
struct Text { float x,y;std::string value;gfx::TextStyle style;uint32_t colour; };
inline std::vector<Text> texts;
inline std::vector<gfx::Rect> panels, focused_controls, focused_images, scissors;
inline int scissor_depth=0;
inline void clear(){texts.clear();panels.clear();focused_controls.clear();focused_images.clear();scissors.clear();}
}
