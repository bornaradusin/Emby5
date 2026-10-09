/* Emby5 disk-backed VOD screen. SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/vod.h"
#include "ui/screen.h"
#include "nuvio_input.h"
#include "platform/ime.h"
#include "gfx/gfx.h"
#include "gfx/art.h"
#include <algorithm>
#include <thread>
namespace ui {
Vod::Vod()=default;
void Vod::activate(){iptv_vod::refresh();}
void Vod::regroup(const iptv_vod::Snapshot &snap){
    if(shows&&snap.show_count==0&&snap.movie_count>0)shows=false;
    if(!shows&&snap.movie_count==0&&snap.show_count>0)shows=true;
    auto found=iptv_vod::groups(shows,search?query:"");
    std::vector<Group> next;next.reserve(found.size());
    for(const auto &g:found)next.push_back({g.name,g.count});
    groups=std::move(next);columns.assign(groups.size(),0);row=0;
}
Action Vod::input(uint32_t p){
    Action a;if(ime::active())return a;
    const auto snap=iptv_vod::snapshot();
    if(snap.generation!=seen){regroup(snap);seen=snap.generation;}
    if(detail){
        if(p&NUVIO_BTN_CIRCLE){detail=false;episode_state.reset();return a;}
        if(!episode_state)return a;
        std::lock_guard<std::mutex> g(episode_state->mutex);
        if(!episode_state->ready)return a;
        const auto &eps=episode_state->list;
        if(p&NUVIO_BTN_UP)episode_index=std::max(0,episode_index-1);
        if(p&NUVIO_BTN_DOWN)episode_index=std::min((int)eps.size()-1,episode_index+1);
        if((p&NUVIO_BTN_CROSS)&&!eps.empty()){
            const auto &ep=eps[(size_t)episode_index];a.kind=Action::PlayIPTV;
            a.iptv_title=series_name+" - "+ep.name;a.iptv_url=ep.url;a.iptv_is_vod=true;
        }return a;
    }
    if(p&NUVIO_BTN_TRIANGLE){search=true;ime::request(ime::Kind::Text,"Search VOD titles",query,
        [this](const std::string &q){query=q;regroup(iptv_vod::snapshot());});return a;}
    if(search&&(p&NUVIO_BTN_CIRCLE)){search=false;query.clear();regroup(snap);return a;}
    if((p&NUVIO_BTN_SQUARE)&&snap.movie_count&&snap.show_count){shows=!shows;regroup(snap);return a;}
    if(p&NUVIO_BTN_UP){if(row==0)a.kind=Action::ToNav;else --row;}
    if(p&NUVIO_BTN_DOWN)row=std::min((int)groups.size()-1,row+1);
    if(row>=0&&row<(int)groups.size()){
        auto &group=groups[(size_t)row];int &col=columns[(size_t)row];
        if(p&NUVIO_BTN_LEFT)col=std::max(0,col-1);
        if(p&NUVIO_BTN_RIGHT)col=std::min((int)group.count-1,col+1);
        if((p&NUVIO_BTN_CROSS)&&group.count){
            auto item=iptv_vod::page(shows,group.title,(size_t)col,1,search?query:"");
            if(item.empty())return a;
            const auto &v=item.front();
            if(v.series){detail=true;episode_index=0;series_name=v.name;
                episode_state=std::make_shared<EpisodeState>();auto state=episode_state;
                std::thread([state,v]{auto eps=iptv_vod::episodes(v);
                    std::lock_guard<std::mutex> g(state->mutex);state->list=std::move(eps);state->ready=true;}).detach();
            }else if(!v.url.empty()){a.kind=Action::PlayIPTV;a.iptv_title=v.name;
                a.iptv_url=v.url;a.iptv_is_vod=true;}
        }
    }return a;
}
void Vod::draw(double,float){
    gfx::text(kPad,210,"VOD",{gfx::Bold,58},kText);
    const auto snap=iptv_vod::snapshot();
    if(snap.generation!=seen){regroup(snap);seen=snap.generation;}
    if(detail){
        gfx::text(kPad,315,series_name,{gfx::Bold,35,1400},kText);
        if(!episode_state)return;
        std::lock_guard<std::mutex> g(episode_state->mutex);
        if(!episode_state->ready){gfx::text(kPad,390,"Loading episodes...",{gfx::Medium,26},kText2);return;}
        if(episode_state->list.empty()){gfx::text(kPad,390,"No episodes available",{gfx::Medium,26},kText2);return;}
        for(int i=std::max(0,episode_index-5);i<(int)episode_state->list.size()&&i<episode_index+8;++i){
            const auto &ep=episode_state->list[(size_t)i];const float y=390+(i-episode_index)*70;
            if(y<270||y>1030)continue;
            if(i==episode_index)glass_panel({kPad-15,y-35,1400,60},14,1.f,false);
            gfx::text(kPad,y,"S"+std::to_string(ep.season)+" E"+std::to_string(ep.number)+"  "+ep.name,
                {gfx::SemiBold,25,1300},i==episode_index?kText:kText2);
        }return;
    }
    if(snap.movie_count&&snap.show_count)gfx::text(kPad,300,"Movies  /  TV Shows    (Square to switch)",{gfx::SemiBold,23},kText2);
    gfx::text(kPad,350,shows?"TV Shows":"Movies",{gfx::Bold,32},kText);
    if(snap.loading&&groups.empty()){gfx::text(kPad,450,"Loading provider catalogue...",{gfx::Medium,26},kText2);return;}
    if(groups.empty()){gfx::text(kPad,450,"No titles in this section",{gfx::Medium,26},kText2);return;}
    constexpr float w=210,h=300,gap=20;
    for(int r=0;r<(int)groups.size();++r){
        const float y=490+(r-row)*410.f;if(y<350||y>1100)continue;
        const auto &group=groups[(size_t)r];
        gfx::text(kPad,y-30,group.title,{gfx::SemiBold,28,1500},r==row?kText:kText2);
        const int focused=columns[(size_t)r],begin=std::max(0,focused-2);
        auto cards=iptv_vod::page(shows,group.title,(size_t)begin,7,search?query:"");
        for(size_t j=0;j<cards.size();++j){
            const int i=begin+(int)j;const auto &item=cards[j];const float x=kPad+(i-begin)*(w+gap);
            glass_panel({x,y,w,h},15,r==row&&i==focused?1.f:0.5f,false);
            if(!item.poster.empty())if(const gfx::Texture *artwork=art::get(item.poster,300,450))gfx::image({x+7,y+7,w-14,h-54},artwork,1.f,0,true);
            gfx::text(x+8,y+h-14,item.name,{gfx::SemiBold,18,w-16},r==row&&i==focused?kText:kText2);
        }
    }
}
}
