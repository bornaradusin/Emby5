/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "nuvio_input.h"
#include <algorithm>
#include <cmath>
namespace ui {
class HoldNavigation {
    uint32_t direction=0; double since=0,last=0;
public:
    void reset() {direction=0;since=last=0;}
    bool accelerating(double now) const {return direction && std::isfinite(now) && now-since>=1.;}
    int update(uint32_t held,double now) {
        const uint32_t dir=held&(NUVIO_BTN_UP|NUVIO_BTN_DOWN);
        if(!std::isfinite(now)||!dir||dir==(NUVIO_BTN_UP|NUVIO_BTN_DOWN)) {reset();return 0;}
        if(dir!=direction||now<last) {direction=dir;since=last=now;return 0;}
        const double duration=now-since;
        if(duration<1.) {last=now;return 0;}
        const int speed=std::min(5,2+(int)std::min(3.,duration-1.));
        const double period=.19/speed;
        if(now-last<period) return 0;
        // At most three items after a slow frame; never catch up a whole pause.
        const int steps=std::min(3,std::max(1,(int)std::min(3.,(now-last)/period)));
        last=now;
        return dir==NUVIO_BTN_DOWN?steps:-steps;
    }
};
}
