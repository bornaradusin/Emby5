/* Emby5 - GPL-3.0-or-later. Full text is always available without auto motion. */
#pragma once
#include "ui/screen.h"
#include "app/i18n.h"
#include "ui/overview_scroll.h"
#include "nuvio_input.h"
namespace ui {
class DescriptionReader {
    bool opened=false;
    std::string title,body;
    std::vector<std::string> lines;
    int start=0,visible=1;
    float scale=0;
public:
    bool active() const {return opened;}
    int first_line() const {return start;}
    void open(const std::string &name,const std::string &text) {title=name;body=text;opened=true;start=0;scale=0;lines.clear();}
    void input(uint32_t p) {
        if(p&(NUVIO_BTN_CIRCLE|NUVIO_BTN_SQUARE)){opened=false;return;}
        if(p&NUVIO_BTN_UP)start=std::max(0,start-1);
        if(p&NUVIO_BTN_DOWN)start=std::min(std::max(0,(int)lines.size()-visible),start+1);
        if(p&NUVIO_BTN_LEFT)start=std::max(0,start-visible);
        if(p&NUVIO_BTN_RIGHT)start=std::min(std::max(0,(int)lines.size()-visible),start+visible);
    }
    void draw() {
        if(!opened)return;
        const gfx::Rect panel{180,150,1560,780};
        gfx::fill({0,0,gfx::W,gfx::H},0xea000000u);
        gfx::fill(panel,kBg,22);
        const float current=gfx::text_scale();
        const gfx::TextStyle style{gfx::Regular,27};
        if(scale!=current) {scale=current;lines=overview::wrap(body.empty()?T("No description available."):body,panel.w-96,
            [&](const std::string &s){return gfx::measure_text(s,style);});}
        const float line_h=40*current;
        visible=std::max(1,(int)((panel.h-210)/line_h));
        start=std::max(0,std::min(start,(int)lines.size()-visible));
        gfx::text(panel.x+48,panel.y+64,title,{gfx::Bold,34,panel.w-96},kText);
        gfx::push_scissor({panel.x+46,panel.y+110,panel.w-92,panel.h-196});
        for(int i=0;i<visible && start+i<(int)lines.size();++i)
            gfx::text(panel.x+48,panel.y+144+i*line_h,lines[(size_t)(start+i)],style,kText);
        gfx::pop_scissor();
        const std::string position=std::to_string(start+1)+"-"+std::to_string(std::min(start+visible,(int)lines.size()))+" / "+std::to_string(lines.size());
        gfx::text(panel.x+48,panel.y+panel.h-40,T("Up/Down: scroll   Left/Right: page   Circle: close"),{gfx::Medium,22,panel.w-330},kText2);
        gfx::text(panel.x+panel.w-48,panel.y+panel.h-40,position,{gfx::Medium,22,260},kText2,2);
    }
};
// Shared synopsis viewport for discovery pages. Text is wrapped only when changed.
class SynopsisView {
    std::string last;float scale=0;double speed=-1;
    std::vector<std::string> lines; overview::Scroll scroll;
public:
    void draw(const std::string &text,float x,float y,float width,float dt,bool reading,bool *animating) {
        const float s=gfx::text_scale(),line_h=37.7f*s;
        const size_t visible=s>1.f?2:3;
        const double rate=theme_description_speed();
        if(last!=text||scale!=s||speed!=rate) {last=text;scale=s;speed=rate;scroll.reset();
            lines=overview::wrap(text,width,[&](const std::string &v){return gfx::measure_text(v,{gfx::Regular,26});});}
        if(scroll.step(dt,lines.size(),reading,line_h,visible,rate)&&animating)*animating=true;
        const float offset=scroll.offset();
        gfx::push_scissor({x-2,y-29*s,width+4,visible*line_h+4});
        const size_t first=(size_t)(offset/line_h);
        for(size_t i=first;i<lines.size()&&i<first+4;++i)
            gfx::text(x,y+i*line_h-offset,lines[i],{gfx::Regular,26},kText2);
        gfx::pop_scissor();
    }
};
}
