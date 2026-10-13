/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/vod.h"
#include "ui/browse_grid.h"
#include "ui/browse_rows.h"
#include "ui/search_text.h"
#include "platform/ime.h"
#include "gfx/art.h"
#include <algorithm>
#include <limits>
#include <set>

namespace ui {
Vod::Vod(jf::Client *client) : m_client(client) { m_nav.snap(1.f); }
std::string Vod::position_key(const std::string &group) const { return (m_shows ? "shows/" : "movies/") + group; }
void Vod::invalidate_page() { m_page.clear(); m_page_first = -1; m_page_limit = 0; }
void Vod::activate() {
    const auto identity = iptv_vod::provider_identity();
    if (identity != m_identity) {
        m_identity = identity; m_positions.clear(); m_row_scroll.clear(); m_row_pages.clear(); m_last_group.clear(); m_groups.clear(); m_matches.clear();
        m_index = m_row = 0; m_query.clear(); m_return = {}; m_seen = ~uint64_t(0); m_scroll.snap(0); invalidate_page();
    }
    synchronise(); iptv_vod::refresh(); m_animating = true;
}
bool Vod::needs_redraw() const { return m_seen != iptv_vod::snapshot().generation || m_search_focus != 0; }
void Vod::synchronise() {
    const auto snapshot = iptv_vod::snapshot();
    if (snapshot.generation != m_seen) {
        const auto identity = iptv_vod::provider_identity();
        if (identity != m_identity) {
            m_identity = identity; m_positions.clear(); m_row_scroll.clear(); m_row_pages.clear(); m_last_group.clear(); m_groups.clear(); m_matches.clear();
            m_query.clear(); m_return = {}; m_index = m_row = 0; m_scroll.snap(0); invalidate_page();
        }
        regroup(snapshot);
    }
}
int Vod::count() const {
    if (m_query.empty()) return m_row >= 0 && (size_t)m_row < m_groups.size() ? grid_count(m_groups[m_row].count) : 0;
    size_t total = 0;
    for (const auto &group : m_matches) total = std::min((size_t)std::numeric_limits<int>::max(), total + group.count);
    return grid_count(total);
}
std::vector<iptv_vod::Item> Vod::read_page(int first, int limit) const {
    if (first < 0 || limit <= 0) return {};
    limit = std::min(limit, 32);
    if (m_query.empty()) {
        if (m_row < 0 || (size_t)m_row >= m_groups.size()) return {};
        return iptv_vod::page(m_shows, m_groups[m_row].name, (size_t)first, (size_t)limit);
    }
    // Search only the selected VOD type, across its categories. The index supplies
    // counts; at most one visible page of actual item records is read from disk.
    std::vector<iptv_vod::Item> result;
    size_t offset = (size_t)first;
    for (const auto &group : m_matches) {
        if (offset >= group.count) { offset -= group.count; continue; }
        const size_t take = std::min(group.count - offset, (size_t)limit - result.size());
        auto page = iptv_vod::page(m_shows, group.name, offset, take, m_query);
        result.insert(result.end(), std::make_move_iterator(page.begin()), std::make_move_iterator(page.end()));
        offset = 0;
        if (result.size() >= (size_t)limit) break;
    }
    return result;
}
void Vod::cache_page(int first, int limit) {
    limit = std::min(limit, 32);
    if (m_page_first == first && m_page_limit == limit) return;
    m_page = read_page(first,limit); m_page_first = first; m_page_limit = limit;
}
const iptv_vod::Item *Vod::selected() {
    if (m_index < 0 || m_index >= count()) return nullptr;
    if (m_index < m_page_first || m_index >= m_page_first + (int)m_page.size()) cache_page(m_index, 1);
    return m_index >= m_page_first && m_index < m_page_first + (int)m_page.size() ? &m_page[m_index-m_page_first] : nullptr;
}
void Vod::remember() {
    if (!m_query.empty() || m_row < 0 || (size_t)m_row >= m_groups.size()) return;
    const auto *item = selected();
    if (item) m_positions[position_key(m_groups[m_row].name)] = {m_index,item->source,item->id};
    m_last_group[m_shows] = m_groups[m_row].name;
}
void Vod::regroup(const iptv_vod::Snapshot &snapshot) {
    std::string selected_source, selected_id;
    if (!m_query.empty() && m_index >= m_page_first && m_index < m_page_first + (int)m_page.size()) {
        const auto &item = m_page[m_index-m_page_first]; selected_source=item.source; selected_id=item.id;
    }
    const auto group = m_row >= 0 && (size_t)m_row < m_groups.size() ? m_groups[m_row].name : m_last_group[m_shows];
    // Do not silently turn a movie query into a series query while results arrive.
    if (m_query.empty() && m_groups.empty() && !snapshot.loading) {
        if (m_shows && !snapshot.show_count && snapshot.movie_count) m_shows=false;
        else if (!m_shows && !snapshot.movie_count && snapshot.show_count) m_shows=true;
    }
    m_groups = iptv_vod::groups(m_shows);
    m_row = std::clamp(m_row,0,std::max(0,grid_count(m_groups.size())-1));
    for (size_t i=0;i<m_groups.size();++i) if (m_groups[i].name==group) { m_row=(int)i; break; }
    if (!m_query.empty()) {
        m_matches=iptv_vod::groups(m_shows,m_query);
        if (!selected_id.empty()) {
            size_t offset=0;
            for (const auto &g:m_matches) {
                const int at=iptv_vod::locate_item(m_shows,g.name,selected_source,selected_id,m_query);
                if (at>=0) {m_index=grid_count(offset+(size_t)at);break;}
                offset+=g.count;
            }
        }
    } else if (!m_groups.empty()) {
        const auto found=m_positions.find(position_key(m_groups[m_row].name));
        if (found!=m_positions.end()) {
            const auto &pos=found->second;m_index=pos.index;
            if (!pos.id.empty()) {
                const int at=iptv_vod::locate_item(m_shows,m_groups[m_row].name,pos.source,pos.id);
                if (at>=0)m_index=at;
            }
        }
    }
    m_index=std::clamp(m_index,0,std::max(0,count()-1));
    m_seen=snapshot.generation; invalidate_page(); m_row_pages.clear(); m_animating=true;
}
void Vod::select_group(int row,bool header) {
    remember();m_row=std::clamp(row,0,std::max(0,grid_count(m_groups.size())-1));
    m_index=0;m_header=header;m_control=m_shows?1:0;invalidate_page();
    if (!m_groups.empty()) {
        const auto found=m_positions.find(position_key(m_groups[m_row].name));
        if (found!=m_positions.end()) {
            const auto &pos=found->second;m_index=pos.index;
            const int at=iptv_vod::locate_item(m_shows,m_groups[m_row].name,pos.source,pos.id);
            if (at>=0)m_index=at;
        }
    }
    m_index=std::clamp(m_index,0,std::max(0,count()-1));
    const BrowseRowsGeometry geometry;
    m_scroll.to(geometry.vertical(m_row,header));m_nav.to(header||m_row==0?1.f:0.f);m_animating=true;
}
void Vod::clear_search() {
    if (m_query.empty()) return;
    m_query.clear();m_matches.clear();invalidate_page();
    for (size_t i=0;i<m_groups.size();++i) if (m_groups[i].name==m_return.group) {m_row=(int)i;break;}
    m_index=m_return.position.index;
    if (!m_groups.empty()&&!m_return.position.id.empty()) {
        const int at=iptv_vod::locate_item(m_shows,m_groups[m_row].name,m_return.position.source,m_return.position.id);
        if (at>=0)m_index=at;
    }
    m_index=std::clamp(m_index,0,std::max(0,count()-1));
    m_header=m_return.header;m_control=m_return.control;m_scroll.snap(m_return.scroll);m_search_focus=m_return.navigation?2:1;m_animating=true;
}
void Vod::switch_type(bool shows) {
    if (shows==m_shows) return;
    clear_search();remember();m_shows=shows;m_groups.clear();m_row_pages.clear();m_row=0;m_index=0;invalidate_page();
    regroup(iptv_vod::snapshot());m_scroll.snap(0);m_nav.snap(1);m_search_focus=1;m_animating=true;
}
Action Vod::search(bool from_navigation) {
    if (m_menu.active()||ime::active()) return {};
    synchronise();const auto identity=m_identity;const bool shows=m_shows;const std::weak_ptr<int> life=m_life;
    ime::request(ime::Kind::Search,shows?"Search VOD TV shows":"Search VOD movies",m_query,
        [this,life,identity,shows,from_navigation](const std::string &raw) {
            if(life.expired()||identity!=m_identity||identity!=iptv_vod::provider_identity()||shows!=m_shows)return;
            const auto query=search_text(raw);
            if(query.empty()){clear_search();return;}
            if(m_query.empty()){
                remember();const auto *item=selected();
                m_return={m_groups.empty()?std::string():m_groups[m_row].name,
                          {m_index,item?item->source:std::string(),item?item->id:std::string()},m_header,m_control,m_scroll.value,from_navigation};
            }
            m_search_focus=1;m_query=query;m_matches=iptv_vod::groups(m_shows,m_query);m_index=0;m_header=false;
            m_scroll.snap(0);m_nav.snap(1);invalidate_page();m_animating=true;
        });
    return {};
}
void Vod::open_groups(const std::string &query) {
    m_menu_mode=Categories;m_destinations.clear();std::vector<std::string> labels;const auto needle=search_fold(query);
    for(const auto &group:m_groups)if(search_fold(group.name).find(needle)!=std::string::npos){labels.push_back(group.name);m_destinations.push_back(group.name);}
    m_menu.open("Jump to category (Triangle: find category)",std::move(labels));
}
void Vod::enter_from_top(){m_header=count()==0;m_control=m_shows?1:0;m_animating=true;}
Action Vod::input(uint32_t p) {
    Action action;if(ime::active())return action;synchronise();
    if(m_menu.active()) {
        if(m_menu_mode==Categories&&(p&NUVIO_BTN_TRIANGLE)) {
            const std::weak_ptr<int> life=m_life;const auto identity=m_identity;
            ime::request(ime::Kind::Search,"Find category","",[this,life,identity](const std::string &q){if(!life.expired()&&identity==m_identity)open_groups(search_text(q));});return action;
        }
        const int choice=m_menu.input(p);
        if(choice<0)return action;
        if(m_menu_mode==Options) {
            if(choice==0&&m_client) {
                m_progress=browse_state::progress(browse_state::provider_scope(*m_client));
                std::vector<std::string> labels;for(const auto &v:m_progress)labels.push_back(v.source+" - "+v.title);
                m_menu_mode=ContinueWatching;m_menu.open("VOD Continue Watching",std::move(labels));
            } else if(choice==1)open_groups();
            else if(choice==2)iptv_vod::refresh(true);
        } else if(m_menu_mode==ContinueWatching&&(size_t)choice<m_progress.size()) {
            const auto &v=m_progress[choice];action.kind=Action::OpenVOD;action.vod_id=v.id;action.vod_parent=v.parent;
            action.vod_source=v.source;action.vod_episode=v.episode;action.vod_series=v.episode;action.iptv_title=v.title;
        } else if(m_menu_mode==Categories&&(size_t)choice<m_destinations.size()) {
            const auto group=m_destinations[choice];clear_search();
            for(size_t i=0;i<m_groups.size();++i)if(m_groups[i].name==group){select_group((int)i,false);m_search_focus=1;break;}
        }
        return action;
    }
    if(p&NUVIO_BTN_OPTIONS){remember();m_menu_mode=Options;m_menu.open("VOD options",{"Continue Watching (local)","Jump to category","Refresh catalogue"});return action;}
    if(p&NUVIO_BTN_TRIANGLE)return search();
    if(!m_query.empty()&&(p&NUVIO_BTN_CIRCLE)){clear_search();return action;}
    const auto snapshot=iptv_vod::snapshot();
    if((p&NUVIO_BTN_SQUARE)&&snapshot.movie_count&&snapshot.show_count){switch_type(!m_shows);m_header=false;return action;}
    if(m_query.empty()) {
        if(m_header) {
            if(p&NUVIO_BTN_LEFT){m_control=0;return action;}
            if(p&NUVIO_BTN_RIGHT){m_control=1;return action;}
            if(p&(NUVIO_BTN_UP|NUVIO_BTN_CIRCLE)){action.kind=Action::ToNav;return action;}
            if(p&(NUVIO_BTN_CROSS|NUVIO_BTN_DOWN)) {
                if((m_control==0&&snapshot.movie_count)||(m_control==1&&snapshot.show_count))switch_type(m_control==1);
                m_header=false;return action;
            }
        }
        if(p&NUVIO_BTN_UP) {if(m_row>0)select_group(m_row-1,false);else {m_header=true;m_control=m_shows?1:0;}}
        else if(p&NUVIO_BTN_DOWN) {if(m_row+1<(int)m_groups.size())select_group(m_row+1,false);else m_bump=true;}
        else if(p&NUVIO_BTN_LEFT) {if(m_index>0){--m_index;remember();}else m_bump=true;}
        else if(p&NUVIO_BTN_RIGHT) {if(m_index+1<count()){++m_index;remember();}else m_bump=true;}
        else if(p&NUVIO_BTN_CIRCLE) {m_header=true;m_control=m_shows?1:0;}
        m_animating=true;
    } else {
        const BrowseGridGeometry geometry;
        const auto moved=move_browse_grid(p,count(),geometry.cols,m_index,m_header);
        if(moved.to_nav)action.kind=Action::ToNav;
        m_bump|=moved.bump;m_animating|=moved.handled;
    }
    if((p&NUVIO_BTN_CROSS)&&!m_header)if(const auto *v=selected()) {
        action.kind=Action::OpenVOD;action.vod_source=v->source;action.vod_id=v->id;
        action.vod_series=v->series;action.iptv_title=v->name;remember();
    }
    return action;
}
void Vod::draw(double,float dt) {
    synchronise();m_animating=false;const auto snapshot=iptv_vod::snapshot();
    const bool searching=!m_query.empty();
    const BrowseGridGeometry grid;const BrowseRowsGeometry rows(2.f/3.f,345.f);
    m_index=std::clamp(m_index,0,std::max(0,count()-1));
    m_scroll.to(searching?grid.scroll_target(m_index,m_header):rows.vertical(m_row,m_header));
    m_animating|=m_scroll.step(dt,11.f);
    m_nav.to(m_scroll.target<1.f?1.f:0.f);m_animating|=m_nav.step(dt,10.f);
    gfx::fill({0,0,gfx::W,gfx::H},kBg);
    const float opacity=m_nav.value;
    if(opacity>.01f) {
        gfx::text(kPad,176,searching?(m_shows?"VOD TV shows - search results":"VOD movies - search results"):"VOD",{gfx::Bold,50,1400},alpha(kText,opacity));
        const char *types[]={"Movies","TV Shows"};
        for(int i=0;i<2;++i) {
            const gfx::Rect button{kPad+i*250.f,204,230.f,60};
            const bool focus=m_focused&&m_header&&m_control==i&&!m_menu.active()&&!ime::active();
            glass_panel(button,14,opacity,false,focus?1.f:(i==(m_shows?1:0)?.2f:0.f));
            if(focus)draw_focus_outline(button,14,opacity);
            gfx::text(button.x+20,button.y+39,types[i],{gfx::SemiBold,24,button.w-40},alpha(kText,opacity));
        }
        if(searching)gfx::text(kPad+515,243,m_query+" - "+std::to_string(count())+" titles",{gfx::Medium,24,950},alpha(kText2,opacity));
    }
    const float clip_top=280.f*opacity,bottom=gfx::H-72.f;
    gfx::push_scissor({0,clip_top,gfx::W,bottom-clip_top});
    if(searching) {
        const float top=330.f-m_scroll.value;const auto range=grid.visible(count(),top,clip_top,bottom);
        cache_page(range.first,range.second-range.first);
        for(int pass=0;pass<2;++pass)for(size_t j=0;j<m_page.size();++j) {
            const int i=m_page_first+(int)j;const auto &v=m_page[j];
            const bool focus=m_focused&&!m_header&&!m_menu.active()&&!ime::active()&&i==m_index;
            if((pass==0)==focus)continue;
            const float lift=m_lifts.step("search/"+v.source+"/"+v.id,focus,dt,&m_animating);
            draw_browse_card(grid.tile(i,top),v.name,v.poster,false,lift);
        }
    } else {
        const auto visible=rows.rows((int)m_groups.size(),m_scroll.value,clip_top,bottom);
        std::set<std::string> retained;
        for(int r=visible.first;r<visible.second;++r) {
            const auto &group=m_groups[r];const auto key=position_key(group.name);retained.insert(key);
            const auto pos=m_positions.find(key);const int index=r==m_row?m_index:pos==m_positions.end()?0:pos->second.index;
            auto &sx=m_row_scroll[key];sx.to(rows.horizontal(index));m_animating|=sx.step(dt,12.f);
            const float y=rows.card(r,0,m_scroll.value,0).y;
            gfx::text(kPad,y-35,group.name,{gfx::SemiBold,29,1450},kText);
            const auto range=rows.columns(grid_count(group.count),sx.value);auto &page=m_row_pages[key];
            const int limit=range.second-range.first;
            if(page.first!=range.first||page.limit!=limit){page.items=iptv_vod::page(m_shows,group.name,range.first,limit);page.first=range.first;page.limit=limit;}
            for(int pass=0;pass<2;++pass)for(size_t j=0;j<page.items.size();++j) {
                const int i=page.first+(int)j;const auto &v=page.items[j];
                const bool focus=m_focused&&!m_header&&!m_menu.active()&&!ime::active()&&r==m_row&&i==m_index;
                if((pass==0)==focus)continue;
                if(r==m_row&&i==m_index){m_positions[key]={i,v.source,v.id};m_last_group[m_shows]=group.name;}
                const float lift=m_lifts.step(key+"/"+v.source+"/"+v.id,focus,dt,&m_animating);
                draw_browse_card(rows.card(r,i,m_scroll.value,sx.value),v.name,v.poster,false,lift);
            }
        }
        for(auto i=m_row_pages.begin();i!=m_row_pages.end();)if(!retained.count(i->first))i=m_row_pages.erase(i);else ++i;
    }
    gfx::pop_scissor();
    if(searching?count()==0:m_groups.empty())gfx::text(kPad,380,searching?"No matching titles in this section. Triangle: edit search.":snapshot.loading?"Loading provider catalogue...":snapshot.error.empty()?"No titles in this section.":snapshot.error,{gfx::Medium,27,1600},kText2);
    draw_pad_hints(kPad,gfx::H-30,{{PadButton::Triangle,m_shows?"Search TV shows":"Search movies"},{PadButton::Square,"Movies / TV Shows"},{PadButton::Options,"Categories / options"},{PadButton::Circle,searching?"Clear search":"Back"}},0,22);
    m_menu.draw();m_animating|=art::animating()||(snapshot.loading&&m_groups.empty())||ime::active();
}
} // namespace ui
