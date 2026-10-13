/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/vod_detail.h"
#include "app/io_worker.h"
#include "nuvio_input.h"
#include "gfx/art.h"
#include <algorithm>
namespace ui {
VodDetail::VodDetail(jf::Client &c,iptv_vod::Item item,std::string episode):m_client(c),m_item(std::move(item)),m_scope(browse_state::provider_scope(c)),m_provider(iptv_vod::provider_identity()),m_initial_episode(std::move(episode)){m_view.item=m_item;}
VodDetail::~VodDetail(){std::lock_guard<std::mutex> g(m_data->lock);m_data->alive=false;}
void VodDetail::activate(){
    auto d=m_data;auto item=m_item;const auto identity=m_provider;
    auto fail=[d]{std::lock_guard<std::mutex> g(d->lock);if(d->alive){d->ready=true;d->detail.error="Could not load provider details. Circle returns to the list.";}};
    if(!emby5::spawn_io([d,item,identity]{auto v=iptv_vod::details(item);
        if(identity!=iptv_vod::provider_identity()){v={};v.error="Provider changed. Reopen this item.";}
        std::lock_guard<std::mutex> g(d->lock);if(d->alive){d->detail=std::move(v);d->ready=true;}},fail))fail();
}
bool VodDetail::needs_redraw()const{std::lock_guard<std::mutex> g(m_data->lock);return m_data->ready&&!m_loaded;}
void VodDetail::sync(){
    if(m_loaded)return;{std::lock_guard<std::mutex> g(m_data->lock);if(!m_data->ready)return;m_view=m_data->detail;}m_loaded=true;
    if(m_view.item.id.empty())m_view.item=m_item;
    for(const auto &e:m_view.episodes)if(std::find(m_seasons.begin(),m_seasons.end(),e.season)==m_seasons.end())m_seasons.push_back(e.season);
    std::sort(m_seasons.begin(),m_seasons.end());
    for(const auto &e:m_view.episodes)if(e.id==m_initial_episode){auto i=std::find(m_seasons.begin(),m_seasons.end(),e.season);if(i!=m_seasons.end())m_season=(int)(i-m_seasons.begin());break;}
    select_season();
    for(size_t i=0;i<m_indices.size();++i)if(m_view.episodes[m_indices[i]].id==m_initial_episode)m_episode=(int)i;
}
void VodDetail::select_season(){
    m_indices.clear();m_episode=0;if(m_seasons.empty())return;
    for(size_t i=0;i<m_view.episodes.size();++i)if(m_view.episodes[i].season==m_seasons[(size_t)m_season])m_indices.push_back((int)i);
}
browse_state::Progress VodDetail::selected()const{
    browse_state::Progress p;p.source=m_view.item.source;p.id=m_view.item.id;p.title=m_view.item.name;
    if(m_view.item.series&&!m_indices.empty()){
        const auto &e=m_view.episodes[m_indices[(size_t)m_episode]];p.episode=true;p.parent=p.id;p.id=e.id;
        p.title+=" - S"+std::to_string(e.season)+" E"+std::to_string(e.number)+" - "+e.name;
    }
    return p;
}
Action VodDetail::play(bool from_start){
    Action a;
    if(!m_loaded||!m_view.ok||m_scope!=browse_state::provider_scope(m_client)||m_provider!=iptv_vod::provider_identity())return a;
    if(m_view.item.series&&m_indices.empty())return a;
    const auto p=selected();std::string url=m_view.item.series?m_view.episodes[m_indices[(size_t)m_episode]].url:m_view.item.url;
    if(url.empty())return a;
    a.kind=Action::PlayIPTV;a.iptv_is_vod=true;a.iptv_title=p.title;a.iptv_url=url;
    a.vod_scope=m_scope;a.vod_source=p.source;a.vod_id=p.id;a.vod_parent=p.parent;a.vod_episode=p.episode;
    a.vod_position=from_start?0:browse_state::position(m_scope,p);return a;
}
Action VodDetail::input(uint32_t p){
    sync();Action a;
    if(m_reader.active()){m_reader.input(p);return a;}
    if(m_choices.active()){
        const int choice=m_choices.input(p);if(choice==0)return play(false);if(choice==1)return play(true);
        if(choice==2)browse_state::save_progress(m_scope,selected(),true);return a;
    }
    if(p&NUVIO_BTN_CIRCLE){a.kind=Action::Back;return a;}
    if(p&NUVIO_BTN_SQUARE){m_reader.open(m_view.item.name,m_view.description);return a;}
    if(!m_loaded||!m_view.ok)return a;
    if(p&NUVIO_BTN_OPTIONS){m_choices.open("Playback",{browse_state::position(m_scope,selected())>0?"Resume":"Play","Play from beginning","Remove local resume point"});return a;}
    if(m_zone==0){
        if(p&NUVIO_BTN_LEFT)m_button=0;if(p&NUVIO_BTN_RIGHT)m_button=1;
        if(p&NUVIO_BTN_DOWN) {if(!m_seasons.empty())m_zone=1;}
        if(p&NUVIO_BTN_CROSS)return play(m_button==1);
    }else if(m_zone==1){
        int next=m_season;if(p&NUVIO_BTN_LEFT)next=std::max(0,next-1);if(p&NUVIO_BTN_RIGHT)next=std::min((int)m_seasons.size()-1,next+1);
        if(next!=m_season){m_season=next;select_season();}
        if(p&NUVIO_BTN_UP)m_zone=0;
        if((p&(NUVIO_BTN_DOWN|NUVIO_BTN_CROSS))&&!m_indices.empty())m_zone=2;
    }else{
        if(p&NUVIO_BTN_UP){if(m_episode>0)--m_episode;else m_zone=1;}
        if(p&NUVIO_BTN_DOWN)m_episode=std::min((int)m_indices.size()-1,m_episode+1);
        if(p&NUVIO_BTN_CROSS)return play(false);
    }
    return a;
}
void VodDetail::draw(double,float dt){
    sync();m_animating=!m_loaded;gfx::fill({0,0,gfx::W,gfx::H},kBg);
    const auto &item=m_view.item;
    gfx::Rect poster{kPad,180,280,420};glass_panel(poster,14);
    if(!item.poster.empty())if(const auto *t=art::get(item.poster,400,600))gfx::image(poster,t,1.f,14,true);
    gfx::text(450,190,item.name,{gfx::Bold,44,1320},kText);
    gfx::text(450,241,item.source+(item.group.empty()?"":" - "+item.group),{gfx::Medium,23,1300},kText2);
    m_synopsis.draw(m_loaded?(m_view.description.empty()?"No description available.":m_view.description):"Loading provider details...",450,302,1300,dt,m_zone==0&&!modal(),&m_animating);
    if(m_view.duration>0)gfx::text(450,430,std::to_string((int)(m_view.duration/60))+" min",{gfx::Medium,23,600},kText2);
    if(m_loaded&&!m_view.error.empty())gfx::text(450,m_view.ok?582:470,m_view.error,{gfx::Medium,24,1300},kText2);
    if(m_loaded&&m_view.ok){
        const bool resume=browse_state::position(m_scope,selected())>0;
        for(int i=0;i<2;++i){gfx::Rect b{450+i*350.f,467,325,68};glass_panel(b,14,1.f,false,m_zone==0&&m_button==i?1.f:0.f);
            if(m_zone==0&&m_button==i)draw_focus_outline(b,14);
            gfx::text(b.x+22,b.y+44,i==0?(resume?"Resume":"Play"):"Play from beginning",{gfx::SemiBold,24,281},kText);}
        if(!m_seasons.empty()){
            const int first=std::max(0,m_season-4);
            for(int i=first;i<(int)m_seasons.size()&&i<first+7;++i){gfx::Rect b{kPad+(i-first)*230.f,637,208,60};
                glass_panel(b,14,1.f,false,i==m_season?.65f:0.f);if(m_zone==1&&i==m_season)draw_focus_outline(b,14);
                gfx::text(b.x+15,b.y+40,"Season "+std::to_string(m_seasons[(size_t)i]),{gfx::SemiBold,23,178},kText);}
            const int epfirst=std::max(0,m_episode-3);
            for(int i=epfirst;i<(int)m_indices.size()&&i<epfirst+4;++i){const auto &e=m_view.episodes[m_indices[(size_t)i]];
                gfx::Rect b{kPad,725+(i-epfirst)*65.f,gfx::W-2*kPad,56};glass_panel(b,12,.9f,false,m_zone==2&&m_episode==i?1.f:0.f);
                if(m_zone==2&&m_episode==i)draw_focus_outline(b,12);
                gfx::text(b.x+22,b.y+37,"E"+std::to_string(e.number)+"  "+e.name,{gfx::Medium,24,b.w-44},kText);}
        }else if(item.series)gfx::text(450,580,"No episodes available",{gfx::Medium,26},kText2);
    }
    gfx::text(kPad,1034,"Options: playback actions   Square: read more   Circle: back",{gfx::Medium,22,1600},kText2);
    m_choices.draw();m_reader.draw();if(art::animating())m_animating=true;
}
}
