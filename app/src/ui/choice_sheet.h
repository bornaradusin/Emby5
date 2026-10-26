/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/screen.h"
#include "app/i18n.h"
#include "nuvio_input.h"
namespace ui {
class ChoiceSheet {
    std::string title;
    std::vector<std::string> labels;
    int focus=0;
    bool shown=false;
public:
    void open(std::string name,std::vector<std::string> options){title=std::move(name);labels=std::move(options);focus=0;shown=true;}
    bool active() const{return shown;}
    int input(uint32_t p){
        if(p&(NUVIO_BTN_CIRCLE|NUVIO_BTN_OPTIONS)){shown=false;return -2;}
        if(p&NUVIO_BTN_UP)focus=std::max(0,focus-1);
        if(p&NUVIO_BTN_DOWN)focus=std::min(std::max(0,(int)labels.size()-1),focus+1);
        if((p&NUVIO_BTN_CROSS)&&!labels.empty()){shown=false;return focus;}
        return -1;
    }
    void draw()const{
        if(!shown)return;
        gfx::fill({0,0,gfx::W,gfx::H},0xc9000000u);
        const float h=160+std::min((size_t)8,labels.size())*72.f;
        gfx::Rect panel{(gfx::W-850)/2,(gfx::H-h)/2,850,h};glass_panel(panel,24,1.f);
        gfx::text(panel.x+36,panel.y+56,title,{gfx::Bold,30,770},kText);
        const int first=std::max(0,focus-7);
        for(int i=first;i<(int)labels.size()&&i<first+8;++i){
            gfx::Rect button{panel.x+28,panel.y+82+(i-first)*72.f,794,62};
            glass_panel(button,14,1.f,false,i==focus?1.f:0.f);
            if(i==focus)draw_focus_outline(button,14);
            gfx::text(button.x+22,button.y+41,labels[(size_t)i],{gfx::SemiBold,24,750},kText);
        }
        if(labels.empty())gfx::text(panel.x+36,panel.y+99,T("Nothing here yet"),{gfx::Medium,24,770},kText2);
        gfx::text(panel.x+36,panel.y+h-20,T("X: Select    Circle: Back"),{gfx::Medium,20,680},kText2);
        if(labels.size()>8)gfx::text(panel.x+780,panel.y+h-20,std::to_string(focus+1)+" / "+std::to_string(labels.size()),{gfx::Medium,18,150},kText2,2);
    }
};
}
