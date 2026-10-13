/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/detail_layout.h"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
    using namespace ui::detail_layout;
    assert(text_scale(std::numeric_limits<float>::quiet_NaN())==1);
    assert(visible_first(0,0,0,3)==0);
    int checks=0;
    for(float scale:{.9f,1.f,1.1f,1.2f})for(float card:{.86f,1.f,1.16f}) {
        const Episodes row(1680,scale,card);
        assert(row.capacity>=2);
        assert(std::abs(row.height-row.width*9.f/16.f)<.01f);
        // Room for a zoomed thumbnail and its strong outer focus ring.
        assert(row.width*.05f+7.f<=row.inset);
        assert(row.image_y-row.height*.05f-7.f>=30.f*scale);
        assert(row.title_y-22.f*scale>=row.image_y+row.height*1.05f);
        assert(row.section_height>row.synopsis_y+2.f*27.f*scale+19.f*scale);
        for(int count:{1,2,3,4,5,17,100}) {
            int first=0;
            for(int direction:{1,-1})for(int step=0;step<count;++step){
                const int selected=direction>0?step:count-1-step;
                first=visible_first(selected,first,count,row.capacity);
                assert(first>=0&&first<=selected&&selected<first+row.capacity);
                assert(first<=std::max(0,count-row.capacity));
                const int last=std::min(count,first+row.capacity);
                for(int i=first;i<last;++i){const float x=row.inset+(i-first)*(row.width+row.gap);
                    assert(x>=0&&x+row.width<=1680-row.inset+.1f);}
                ++checks;
            }
        }
    }
    // Entire selected tab, including a very long bounded label, stays visible.
    float x=0;const float width=500,inner=1660,total=12*(width+14)-14;
    for(int i=0;i<12;++i){x=tab_scroll(i*(width+14),width,inner,total,x);
        assert(i*(width+14)>=x&&i*(width+14)+width<=x+inner);}
    for(int i=11;i>=0;--i){x=tab_scroll(i*(width+14),width,inner,total,x);
        assert(i*(width+14)>=x&&i*(width+14)+width<=x+inner);}
    assert(tab_scroll(0,132,1660,132,900)==0);
    assert(tab_scroll(1500,1660,1660,5000,0)==1500);
    std::cout<<"PASS: season-tab visibility, whole episode windows, 16:9 images, scalable text/focus spacing ("<<checks<<" navigation checks)\n";
}
