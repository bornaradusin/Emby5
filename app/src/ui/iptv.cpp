#include "ui/iptv.h"
#include "nuvio_input.h"
#include "gfx/gfx.h"
#include "ui/screen.h"
#include "platform/ime.h"
#include <cctype>
#include <ctime>
#include <algorithm>
#include <thread>

namespace ui {
IPTV::IPTV(jf::Client &client) : m_client(client) { m_connected = iptv_xtream::load_credentials(&m_creds); if (m_connected) reload_store(); }
IPTV::~IPTV() = default;
void IPTV::reload_store() { m_store.load(m_creds.server+"|"+m_creds.username); }
void IPTV::refresh() { iptv_live::refresh(&m_client,true); }
void IPTV::activate() {
    iptv_xtream::Credentials current;
    const bool valid=iptv_xtream::load_credentials(&current);
    if (!valid) current={};
    if (current.server != m_creds.server || current.username != m_creds.username || current.password != m_creds.password) {
        m_creds=std::move(current);
        m_connected=valid;
        m_filter=m_index=0;
    }
    reload_store(); m_category_focus=false; m_hold_dir=0; m_hold_since=0; refresh();
}
std::vector<iptv_xtream::Channel> IPTV::current() const { return iptv_live::snapshot().channels; }
std::vector<iptv_xtream::Channel> IPTV::visible(const iptv_xtream::Catalog &c) const {
    if(m_search) {
        std::string q=m_query;
        std::transform(q.begin(),q.end(),q.begin(),[](unsigned char c){return (char)std::tolower(c);});
        std::vector<iptv_xtream::Channel> out;
        for (const auto &ch:c.channels) {
            std::string name=ch.name;
            std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return (char)std::tolower(c);});
            if (name.find(q)!=std::string::npos) out.push_back(ch);
        }
        return out;
    }
    if(m_filter==0) return c.channels;
    if(m_filter==1) { std::vector<iptv_xtream::Channel> out; for (const auto &ch:c.channels) if(ch.source=="Emby") out.push_back(ch); return out; }
    if(m_filter==2) { std::vector<iptv_xtream::Channel> out; for (const auto &ch:c.channels) if(ch.source=="Xtream") out.push_back(ch); return out; }
    std::vector<iptv_xtream::Channel> out;
    if(m_filter>2 && m_filter<=(int)m_store.categories().size()+2) {
        const auto &ids=m_store.categories()[(size_t)m_filter-3].channels;
        for(const auto &id:ids) for(const auto &ch:c.channels) if(ch.source=="Xtream" && (ch.id==id || ch.id=="xtream:"+id)) {out.push_back(ch);break;}
    }
    return out;
}
void IPTV::move_selection(int delta, bool allow_category) {
    iptv_xtream::Catalog cat; cat.channels=current();
    const int count=(int)visible(cat).size();
    if (delta<0 && m_index==0 && allow_category && !m_search) {
        m_category_focus=true;
    } else {
        m_index=std::clamp(m_index+delta,0,std::max(0,count-1));
    }
}
void IPTV::update_hold(uint32_t held, double now) {
    const uint32_t dir=(held & NUVIO_BTN_DOWN) ? NUVIO_BTN_DOWN :
                       (held & NUVIO_BTN_UP) ? NUVIO_BTN_UP : 0;
    if (!dir || (m_search && ime::active()) || m_category_focus) {
        m_hold_dir=0; m_hold_since=0; return;
    }
    if (dir!=m_hold_dir) {
        m_hold_dir=dir; m_hold_since=now; m_hold_last=now; return;
    }
    const double duration=now-m_hold_since;
    if (duration<1.0) return;
    // First second arms acceleration; 2x, 3x, 4x, then 5x after four seconds.
    const int speed=std::min(5,2+(int)(duration-1.0));
    const double interval=0.19/speed;
    if (now-m_hold_last>=interval) {
        move_selection(dir==NUVIO_BTN_DOWN ? 1 : -1,false);
        m_hold_last=now;
    }
}
Action IPTV::input(uint32_t p) {
    Action a;
    
    if (ime::active()) return a;
    if (p & NUVIO_BTN_TRIANGLE) {
        if (!m_search) m_saved_index=m_index;
        m_search=true; m_index=0; m_category_focus=false; m_hold_dir=0;
        ime::request(ime::Kind::Text,"Search all IPTV channels",m_query,
                     [this](const std::string &q){m_query=q; m_index=0;});
        return a;
    }
    if (m_search && (p & NUVIO_BTN_CIRCLE)) {
        m_search=false; m_query.clear(); m_index=m_saved_index; m_hold_dir=0;
        return a;
    }
    const int last_filter = (int)m_store.categories().size()+2;
    // The category selector is a real focusable control above the channel list.
    if (!m_search && (p & NUVIO_BTN_LEFT)) {
        if (m_filter > 0) { --m_filter; m_index = 0; }
    }
    if (!m_search && (p & NUVIO_BTN_RIGHT)) {
        if (m_filter < last_filter) { ++m_filter; m_index = 0; }
    }
    if (m_category_focus) {
        if (p & NUVIO_BTN_UP) a.kind = Action::ToNav;
        if (p & NUVIO_BTN_DOWN) m_category_focus = false;
        if (p & NUVIO_BTN_CROSS) {
            m_filter = m_filter < last_filter ? m_filter + 1 : 0;
            m_index = 0;
        }
        return a;
    }
    iptv_xtream::Catalog cat; cat.channels=current();
    auto ch = visible(cat);
    if (p & NUVIO_BTN_UP) move_selection(-1);
    if (p & NUVIO_BTN_DOWN) move_selection(1);
    if (p & NUVIO_BTN_CROSS) {
        if (m_index >= 0 && m_index < (int)ch.size()) {
            const auto &selected=ch[(size_t)m_index];
            iptv_live::set_playing_channel(selected.id);
            if (selected.source=="Emby") {
                a.kind=Action::Play;
                a.item.id=selected.emby_id;
                a.item.name=selected.name;
                a.item.type="LiveTvChannel";
            } else {
                a.kind = Action::PlayIPTV;
                a.iptv_title = selected.name;
                iptv_xtream::Channel original=selected; original.id=selected.id.substr(7);
                a.iptv_url = iptv_xtream::live_url(m_creds, original);
            }
        } else refresh();
    }
    return a;
}
void IPTV::draw(double, float) {
    gfx::text(kPad,210,"IPTV",{gfx::Bold,58},kText);
    const auto snap=iptv_live::snapshot();
    if (snap.generation != m_seen) m_seen=snap.generation;
    iptv_xtream::Catalog cat; cat.channels=snap.channels;
    gfx::text(kPad,268,"Up: Category    Left/Right: Change category    Triangle: Search    X: Play",{gfx::Medium,21},kText2);
    if (snap.loading && cat.channels.empty()) gfx::text(kPad,360,"Loading live TV channels...",{gfx::Medium,28},kText2);
    if (m_filter > (int)m_store.categories().size()+2) m_filter=0;
    std::string filter=m_filter==0?"All channels":m_filter==1?"Emby Live TV":m_filter==2?"Xtream":m_store.categories()[(size_t)m_filter-3].name;
    const std::string category_label = m_search ? "Search all channels: " + (m_query.empty()?"(all)":m_query) + "  |  Circle: Close" : "Category: " + filter + "  <  >";
    if (m_category_focus) glass_panel({kPad-15,290,1450,60},18,1.f,false);
    gfx::text(kPad,328,category_label,{gfx::SemiBold,29},m_category_focus?kText:kText2);
    auto channels=visible(cat);
    if(m_index>=(int)channels.size()) m_index=std::max(0,(int)channels.size()-1);
    const int first=std::max(0,m_index-5);
    for(int i=first;i<(int)channels.size() && i<first+11;++i) {
        float y=395+(i-first)*52;
        if(i==m_index && !m_category_focus) glass_panel({kPad-15,y-31,1450,48},18,1.f,false);
        const auto &ch=channels[(size_t)i];
        std::string title=ch.name+"  ["+ch.source+"]";
        if (!ch.now.empty()) title += "  |  " + ch.now;
        gfx::text(kPad,y,title,{gfx::SemiBold,25,1350},i==m_index?kText:kText2);
    }
    if (!channels.empty() && m_index<(int)channels.size()) {
        const auto &selected=channels[(size_t)m_index];
        if (selected.source=="Xtream" && selected.id!=m_guide_requested) {
            m_guide_requested=selected.id;
            iptv_live::request_guide(&m_client,selected);
        }
        if (!selected.now.empty()) {
            gfx::text(kPad,980,"Now playing: " + selected.now,{gfx::Medium,21,1350},kText2);
            const auto epoch=(int64_t)std::time(nullptr);
            if (selected.now_end>selected.now_start) {
                const double ratio=std::clamp(double(epoch-selected.now_start)/double(selected.now_end-selected.now_start),0.0,1.0);
                gfx::fill({kPad,993,950,5},0x50ffffffu,2.5f);
                gfx::fill({kPad,993,(float)(950*ratio),5},0xffffffffu,2.5f);
            }
        }
        if (!selected.next.empty()) gfx::text(kPad,1030,"Up next: " + selected.next,{gfx::Medium,21,1350},kText2);
    }
    if(channels.empty() && !snap.loading) gfx::text(kPad,425,"No channels available from Emby or Xtream.",{gfx::Medium,24},kText2);
}
} // namespace ui
