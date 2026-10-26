/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/iptv.h"
#include "app/i18n.h"
#include "ui/browse_grid.h"
#include "ui/browse_rows.h"
#include "ui/search_text.h"
#include "platform/ime.h"
#include "gfx/art.h"
#include "app/epg.h"
#include <algorithm>
#include <ctime>
#include <set>

namespace ui {
std::string IPTV::key(const iptv_xtream::Channel &channel) {
    return channel.source + "\n" + browse_state::channel_key(channel);
}
IPTV::IPTV(jf::Client &client) : m_client(client) { m_nav.snap(1.f); }
void IPTV::activate() {
    const auto scope = browse_state::provider_scope(m_client);
    if (scope != m_scope) {
        m_scope = scope; m_groups = std::make_shared<const iptv_navigation::Groups>();
        m_positions.clear(); m_row_scroll.clear(); m_query.clear(); m_results.clear(); m_return = {};
        m_index = m_row = 0; m_header = false; m_guide_requested.clear(); m_scroll.snap(0); m_seen = ~uint64_t(0);
        m_guide = false; epg::reset();   /* another server or provider: another guide */
    }
    iptv_xtream::load_credentials(&m_creds);
    m_store.load(m_creds.server + "|" + m_creds.username);
    build_groups(iptv_live::snapshot());
    m_hold_dir = 0; m_hold_since = 0;
    iptv_live::refresh(&m_client); // respects freshness; does not force another download
    m_animating = true;
}
bool IPTV::needs_redraw() const {
    return m_seen != iptv_live::summary().generation || m_hold_dir != 0 || m_search_focus != 0 ||
           (m_guide && (epg::generation() != m_guide_seen || (int64_t)std::time(nullptr) / 60 != m_guide_minute));
}
void IPTV::synchronise() {
    const auto state=iptv_live::summary();
    if(m_content_seen!=state.catalogue_generation)build_groups(iptv_live::snapshot());
    m_seen=state.generation;
}
const iptv_xtream::Channel *IPTV::channel(int index) const {
    if (index < 0) return nullptr;
    if (!m_query.empty()) {
        if ((size_t)index >= m_results.size()) return nullptr;
        const auto &ref = m_results[index];
        return &(*m_groups)[ref.group].channels[ref.channel];
    }
    if (m_row < 0 || (size_t)m_row >= m_groups->size()) return nullptr;
    const auto &channels = (*m_groups)[m_row].channels;
    return (size_t)index < channels.size() ? &channels[index] : nullptr;
}
int IPTV::count() const {
    if (!m_query.empty()) return grid_count(m_results.size());
    return m_row >= 0 && (size_t)m_row < m_groups->size() ? grid_count((*m_groups)[m_row].channels.size()) : 0;
}
void IPTV::remember() {
    if (!m_query.empty() || m_row < 0 || (size_t)m_row >= m_groups->size()) return;
    const auto *selected = channel(m_index);
    m_positions[(*m_groups)[m_row].id] = {m_index, selected ? key(*selected) : std::string()};
}
void IPTV::select_group(int row, bool header) {
    remember();
    m_row = std::clamp(row, 0, std::max(0, grid_count(m_groups->size()) - 1));
    m_index = 0; m_header = header;
    if (!m_groups->empty()) {
        const auto &group = (*m_groups)[m_row];
        const auto found = m_positions.find(group.id);
        if (found != m_positions.end()) {
            m_index = std::clamp(found->second.index, 0, std::max(0, grid_count(group.channels.size()) - 1));
            for (size_t i = 0; i < group.channels.size(); ++i)
                if (key(group.channels[i]) == found->second.key) { m_index = (int)i; break; }
        }
    }
    const BrowseRowsGeometry geometry(16.f/9.f);
    m_scroll.to(geometry.vertical(m_row,header));
    m_nav.to(header || m_row == 0 ? 1.f : 0.f);
    m_animating = true;
}
void IPTV::build_groups(const iptv_live::Snapshot &snapshot) {
    remember();
    const std::string group = m_row >= 0 && (size_t)m_row < m_groups->size() ? (*m_groups)[m_row].id : std::string();
    const auto *selected = channel(m_index);
    const std::string selected_key = selected ? key(*selected) : std::string();
    m_groups = std::make_shared<const iptv_navigation::Groups>(
        iptv_navigation::build_groups(snapshot.channels, m_store.categories()));
    m_seen = snapshot.generation; m_content_seen=snapshot.catalogue_generation;
    m_row = std::clamp(m_row, 0, std::max(0, grid_count(m_groups->size()) - 1));
    for (size_t i = 0; i < m_groups->size(); ++i) if ((*m_groups)[i].id == group) { m_row = (int)i; break; }
    if (!m_query.empty()) rebuild_search(selected_key);
    else {
        const auto pos = m_groups->empty() ? Position{} : m_positions[(*m_groups)[m_row].id];
        m_index = std::clamp(pos.index, 0, std::max(0, count() - 1));
        for (int i = 0; i < count(); ++i) if (key(*channel(i)) == pos.key) { m_index = i; break; }
    }
    // Deleted categories must not leave unbounded cursor entries behind.
    std::set<std::string> valid;
    for (const auto &g : *m_groups) valid.insert(g.id);
    for (auto it = m_positions.begin(); it != m_positions.end();)
        if (!valid.count(it->first)) it = m_positions.erase(it); else ++it;
    for(auto it=m_row_scroll.begin();it!=m_row_scroll.end();)if(!valid.count(it->first))it=m_row_scroll.erase(it);else ++it;
    m_animating = true;
}
void IPTV::rebuild_search(const std::string &selected) {
    m_results.clear();
    const auto query = search_fold(m_query);
    std::set<std::string> used;
    for (size_t r = 0; r < m_groups->size(); ++r) {
        const auto &group = (*m_groups)[r];
        for (size_t c = 0; c < group.channels.size(); ++c) {
            const auto &ch = group.channels[c];
            if (search_fold(ch.name).find(query) == std::string::npos || !used.insert(key(ch)).second) continue;
            m_results.push_back({r,c});
        }
    }
    m_index = std::clamp(m_index, 0, std::max(0, grid_count(m_results.size()) - 1));
    if (!selected.empty()) for (int i = 0; i < count(); ++i) if (key(*channel(i)) == selected) { m_index = i; break; }
}
void IPTV::clear_search() {
    if (m_query.empty()) return;
    m_query.clear(); m_results.clear();
    for (size_t r = 0; r < m_groups->size(); ++r) if ((*m_groups)[r].id == m_return.group) { m_row = (int)r; break; }
    m_index = std::clamp(m_return.position.index, 0, std::max(0, count() - 1));
    for (int i = 0; i < count(); ++i) if (key(*channel(i)) == m_return.position.key) { m_index = i; break; }
    m_header = m_return.header; m_scroll.snap(m_return.scroll); m_search_focus = m_return.navigation ? 2 : 1; m_animating = true;
}
Action IPTV::search(bool from_navigation) {
    if (m_menu.active() || ime::active()) return {};
    synchronise();
    const auto scope = m_scope;
    const std::weak_ptr<int> life = m_life;
    ime::request(ime::Kind::Search, T("Search Live TV channels"), m_query,
        [this, life, scope, from_navigation](const std::string &raw) {
            if (life.expired() || scope != m_scope || scope != browse_state::provider_scope(m_client)) return;
            const auto query = search_text(raw);
            if (query.empty()) { clear_search(); return; }
            if (m_query.empty()) {
                remember();
                m_return = {m_groups->empty() ? std::string() : (*m_groups)[m_row].id,
                            {m_index, channel(m_index) ? key(*channel(m_index)) : std::string()},
                            m_header, m_scroll.value, from_navigation};
            }
            m_query = query; m_index = 0; m_header = false; m_search_focus = 1;
            rebuild_search(); m_scroll.snap(0); m_nav.snap(1); m_animating = true;
        });
    return {};
}
void IPTV::open_groups(Menu mode, const std::string &query) {
    m_menu_mode = mode; m_destinations.clear();
    const auto pins = browse_state::pins(m_scope);
    const auto needle = search_fold(query);
    std::vector<std::string> labels;
    for (const auto &group : *m_groups) {
        if (mode == Pinned && std::find(pins.begin(),pins.end(),group.id) == pins.end()) continue;
        if (search_fold(group.title).find(needle) == std::string::npos) continue;
        labels.push_back(group.title); m_destinations.push_back(group.id);
    }
    m_menu.open(mode == Pinned ? T("Pinned groups") : T("Jump to group (Triangle: find group)"), std::move(labels));
}
void IPTV::enter_from_top() { m_header = count() == 0; m_animating = true; }
void IPTV::focus_channel(const std::string &group_id, const std::string &channel_id) {
    synchronise();
    if (!m_query.empty()) {
        for (size_t i = 0; i < m_results.size(); ++i) {
            const auto &ref = m_results[i];
            if ((*m_groups)[ref.group].id == group_id && (*m_groups)[ref.group].channels[ref.channel].id == channel_id) {
                m_index = (int)i; return;
            }
        }
        return; // keep the search results and pre-search cursor intact
    }
    for (size_t r = 0; r < m_groups->size(); ++r) if ((*m_groups)[r].id == group_id) {
        remember(); m_row = (int)r;
        for (size_t c = 0; c < (*m_groups)[r].channels.size(); ++c)
            if ((*m_groups)[r].channels[c].id == channel_id) { m_index = (int)c; m_header = false; remember(); return; }
    }
}
bool IPTV::launch(jf::Client &, const iptv_xtream::Channel &channel) {
    iptv_xtream::Credentials creds; iptv_xtream::load_credentials(&creds);
    iptv_categories::Store store; store.load(creds.server + "|" + creds.username);
    auto groups = std::make_shared<const iptv_navigation::Groups>(
        iptv_navigation::build_groups(iptv_live::snapshot().channels, store.categories()));
    for (size_t r = 0; r < groups->size(); ++r) for (size_t c = 0; c < (*groups)[r].channels.size(); ++c)
        if (key((*groups)[r].channels[c]) == key(channel)) return iptv_navigation::begin(groups,r,c);
    return false;
}
void IPTV::move(uint32_t p, Action *action) {
    if (!m_query.empty()) {
        const BrowseGridGeometry geometry(16.f/9.f);
        const auto moved=move_browse_grid(p,count(),geometry.cols,m_index,m_header);
        if(moved.to_nav&&action)action->kind=Action::ToNav;
        m_bump|=moved.bump;m_animating|=moved.handled;return;
    }
    if(p&NUVIO_BTN_CIRCLE) { if(action)action->kind=Action::ToNav; return; }
    if(p&NUVIO_BTN_UP) {
        if(m_row>0) select_group(m_row-1,false);
        else if(action) action->kind=Action::ToNav;
    } else if(p&NUVIO_BTN_DOWN) {
        if(m_header) m_header=false;
        else if(m_row+1<(int)m_groups->size()) select_group(m_row+1,false);
        else m_bump=true;
    } else if(p&NUVIO_BTN_LEFT) {
        if(m_index>0)--m_index;else m_bump=true;
    } else if(p&NUVIO_BTN_RIGHT) {
        if(m_index+1<count())++m_index;else m_bump=true;
    }
    remember();m_animating=true;
}
void IPTV::update_hold(uint32_t held, double now) {
    const uint32_t dir = (held & NUVIO_BTN_DOWN) ? NUVIO_BTN_DOWN : (held & NUVIO_BTN_UP) ? NUVIO_BTN_UP : 0;
    if (!dir || m_header || !m_focused || ime::active() || m_menu.active()) { m_hold_dir = 0; m_hold_since = 0; return; }
    if (dir != m_hold_dir) { m_hold_dir = dir; m_hold_since = m_hold_last = now; return; }
    const double elapsed = now - m_hold_since;
    if (elapsed < 1.) return;
    const int speed = std::min(5, 2 + (int)(elapsed - 1));
    if (now - m_hold_last >= .19/speed) {
        synchronise();
        if (m_guide) guide_move(dir == NUVIO_BTN_DOWN ? 1 : -1);   /* the guide: channel by channel */
        else move(dir,nullptr);
        m_hold_last = now;
    }
}
Action IPTV::input(uint32_t p) {
    Action action;
    if (ime::active()) return action;
    synchronise();
    if (m_menu.active()) {
        if ((p & NUVIO_BTN_TRIANGLE) && m_menu_mode == Groups) {
            const auto scope = m_scope; const std::weak_ptr<int> life = m_life;
            ime::request(ime::Kind::Search,T("Find group"),"",[this,life,scope](const std::string &q) {
                if (!life.expired() && scope == m_scope) open_groups(Groups, search_text(q));
            }); return action;
        }
        const int selection = m_menu.input(p);
        if (selection < 0) return action;
        if (m_menu_mode == Options) {
            if (selection == 0) open_groups(Groups);
            else if (selection == 1) open_groups(Pinned);
            else if (selection == 2 && !m_pin_group.empty())
                browse_state::toggle_pin(m_scope, m_pin_group);
            else if (selection == (m_pin_group.empty() ? 2 : 3)) iptv_live::refresh(&m_client,true);
        } else if ((size_t)selection < m_destinations.size()) {
            const auto id = m_destinations[selection]; clear_search();
            for (size_t r = 0; r < m_groups->size(); ++r) if ((*m_groups)[r].id == id) { select_group((int)r, false); m_search_focus = 1; break; }
        }
        return action;
    }
    if (m_guide && !(p & (NUVIO_BTN_OPTIONS | NUVIO_BTN_TRIANGLE))) return guide_input(p);
    if ((p & NUVIO_BTN_SQUARE) && !m_header && count() > 0) { open_guide(); return action; }
    if (p & NUVIO_BTN_OPTIONS) {
        m_menu_mode = Options;
        std::vector<std::string> labels{T("Jump to group"), T("Pinned groups")};
        m_pin_group = m_query.empty() && !m_groups->empty() ? (*m_groups)[m_row].id : std::string();
        if (!m_pin_group.empty()) labels.push_back(T("Pin / unpin current group"));
        labels.push_back(T("Refresh sources")); m_menu.open(T("Live TV options"),std::move(labels)); return action;
    }
    if (p & NUVIO_BTN_TRIANGLE) return search();
    if (!m_query.empty() && (p & NUVIO_BTN_CIRCLE)) { clear_search(); return action; }
    move(p, &action);
    if ((p & NUVIO_BTN_CROSS) && !m_header && channel(m_index)) {
        size_t r = (size_t)m_row, c = (size_t)m_index;
        if (!m_query.empty()) { r = m_results[m_index].group; c = m_results[m_index].channel; }
        // Use the full, original group: searching never changes playback Up/Down order.
        if (iptv_navigation::begin(m_groups,r,c)) action.kind = Action::PlayLiveTV;
    }
    return action;
}
void IPTV::draw(double now, float dt) {
    synchronise(); m_animating = false;
    if (m_guide) { draw_guide(now, dt); return; }
    m_index=std::clamp(m_index,0,std::max(0,count()-1));
    const bool searching=!m_query.empty();
    const BrowseGridGeometry grid(16.f/9.f);
    const BrowseRowsGeometry rows(16.f/9.f,300.f);
    m_scroll.to(searching?grid.scroll_target(m_index,m_header):rows.vertical(m_row,m_header));
    m_animating|=m_scroll.step(dt,11.f);
    m_nav.to(m_scroll.target<1.f?1.f:0.f);m_animating|=m_nav.step(dt,10.f);
    gfx::fill({0,0,gfx::W,gfx::H},kBg);
    const auto status=iptv_live::summary();const float opacity=m_nav.value;
    if(opacity>.01f) {
        gfx::text(kPad,176,searching?T("Live TV - search results"):T("Live TV"),{gfx::Bold,50,1400},alpha(kText,opacity));
        if(searching)gfx::text(kPad,224,m_query+" - "+TF("{count} channels",{{"count",std::to_string(count())}}),{gfx::Medium,25,1600},alpha(kText2,opacity));
    }
    const float clip_top=(searching?260.f:210.f)*opacity,bottom=gfx::H-138.f;
    gfx::push_scissor({0,clip_top,gfx::W,bottom-clip_top});
    if(searching) {
        const float top=310.f-m_scroll.value;
        const auto range=grid.visible(count(),top,clip_top,bottom);
        for(int pass=0;pass<2;++pass)for(int i=range.first;i<range.second;++i) {
            const auto *ch=channel(i);if(!ch)continue;
            const bool focused=m_focused&&!m_header&&!m_menu.active()&&!ime::active()&&i==m_index;
            if((pass==0)==focused)continue;
            const float lift=m_lifts.step("search/"+key(*ch),focused,dt,&m_animating);
            draw_browse_card(grid.tile(i,top),ch->name,ch->logo,true,lift);
        }
    } else {
        const auto visible=rows.rows((int)m_groups->size(),m_scroll.value,clip_top,bottom);
        for(int r=visible.first;r<visible.second;++r) {
            const auto &group=(*m_groups)[r];const auto it=m_positions.find(group.id);
            const int index=r==m_row?m_index:it==m_positions.end()?0:it->second.index;
            auto &sx=m_row_scroll[group.id];sx.to(rows.horizontal(index));m_animating|=sx.step(dt,12.f);
            const float y=rows.card(r,0,m_scroll.value,0).y;
            gfx::text(kPad,y-35,group.title,{gfx::SemiBold,29,1450},kText);
            const auto range=rows.columns((int)group.channels.size(),sx.value);
            for(int pass=0;pass<2;++pass)for(int c=range.first;c<range.second;++c) {
                const auto &ch=group.channels[c];
                const bool focused=m_focused&&!m_header&&!m_menu.active()&&!ime::active()&&r==m_row&&c==m_index;
                if((pass==0)==focused)continue;
                const float lift=m_lifts.step(group.id+"/"+key(ch),focused,dt,&m_animating);
                draw_browse_card(rows.card(r,c,m_scroll.value,sx.value),ch.name,ch.logo,true,lift);
            }
        }
    }
    gfx::pop_scissor();
    if(searching?count()==0:m_groups->empty())gfx::text(kPad,380,searching?T("No matching channels. Triangle: edit search."):status.loading?T("Loading Live TV channels..."):T("No channels available. Check your sources in Settings."),{gfx::Medium,27,1600},kText2);
    const auto *selected=channel(m_index);
    iptv_xtream::Channel guide_channel;
    if(selected&&iptv_live::channel_by_id(selected->id,&guide_channel))selected=&guide_channel;
    if(selected) {
        if(key(*selected)!=m_guide_focus){m_guide_focus=key(*selected);m_guide_focus_since=now;}
        if (m_focused && !m_menu.active() && !ime::active() &&
            now-m_guide_focus_since > .7 && selected->source == "Xtream" &&
            selected->id != m_guide_requested) {
            m_guide_requested = selected->id;
            iptv_live::request_guide(&m_client, *selected);
        }
        std::string on_now = selected->now, on_next = selected->next;
        if (on_now.empty() && on_next.empty()) {   /* the TV guide's, when the source's own is empty (M3U) */
            if (m_focused && selected->source == "M3U") epg::want(&m_client, {*selected});
            epg::now_next(*selected, &on_now, &on_next);
        }
        const std::string info = !on_now.empty() ? TF("Now: {programme}", {{"programme", on_now}}) :
            T("No programme information available");
        gfx::text(kPad,gfx::H-97,info,{gfx::Medium,22,1550},kText2);
        if(!on_next.empty())gfx::text(kPad,gfx::H-65,TF("Next: {programme}",{{"programme",on_next}}),{gfx::Medium,20,1550},kText3);
    }
    draw_pad_hints(kPad,gfx::H-28,{{PadButton::Square,T("TV guide")},{PadButton::Triangle,T("Search channels")},{PadButton::Options,T("Groups / options")},{PadButton::Circle,searching?T("Clear search"):T("Back")}},0,23);
    m_menu.draw();m_animating|=art::animating()||(status.loading&&m_groups->empty())||m_hold_dir||ime::active()||(selected&&now-m_guide_focus_since<.8);
}
} // namespace ui
