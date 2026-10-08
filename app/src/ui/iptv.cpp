#include "ui/iptv.h"
#include "nuvio_input.h"
#include "gfx/gfx.h"
#include "ui/screen.h"
#include <algorithm>
#include <thread>

namespace ui {
IPTV::IPTV() { m_connected = iptv_xtream::load_credentials(&m_creds); if(m_connected) reload_store(); }
IPTV::~IPTV() = default;
void IPTV::reload_store() { m_store.load(m_creds.server+"|"+m_creds.username); }
void IPTV::refresh() {
    if(!m_connected) return;
    auto d=m_data;
    { std::lock_guard<std::mutex> lock(d->lock); if(d->loading) return; d->loading=true; d->error.clear(); }
    auto c=m_creds;
    std::thread([d,c]{
        iptv_xtream::Catalog cat; std::string err;
        bool ok=iptv_xtream::authenticate(c,&err) && iptv_xtream::load_catalog(c,&cat,&err);
        std::lock_guard<std::mutex> lock(d->lock);
        if(ok) { d->catalog=std::move(cat); d->ready=true; }
        else d->error=err.empty()?"Unable to load IPTV channels":err;
        d->loading=false;
    }).detach();
}
void IPTV::activate() {
    iptv_xtream::Credentials current;
    const bool valid = iptv_xtream::load_credentials(&current);
    if (!valid) {
        m_connected = false;
        m_creds = {};
        m_store = iptv_categories::Store{};
        m_filter = m_index = 0;
        return;
    }
    if (!m_connected || current.server != m_creds.server ||
        current.username != m_creds.username || current.password != m_creds.password) {
        m_creds = std::move(current);
        m_connected = true;
        reload_store();
        m_filter = m_index = 0;
        m_data = std::make_shared<Data>();
    }
    m_category_focus = false;
    // Categories can change in Settings while credentials stay the same.
    reload_store();
    if (m_filter > (int)m_store.categories().size()) m_filter = 0;
    refresh();
}
std::vector<iptv_xtream::Channel> IPTV::visible(const iptv_xtream::Catalog &c) const {
    if(m_filter==0) return c.channels;
    std::vector<iptv_xtream::Channel> out;
    if(m_filter>0 && m_filter<=(int)m_store.categories().size()) {
        const auto &ids=m_store.categories()[(size_t)m_filter-1].channels;
        for(const auto &id:ids) for(const auto &ch:c.channels) if(ch.id==id) {out.push_back(ch);break;}
    }
    return out;
}
Action IPTV::input(uint32_t p) {
    Action a;
    if (!m_connected) {
        if (p & NUVIO_BTN_UP) a.kind = Action::ToNav;
        return a;
    }
    const int last_filter = (int)m_store.categories().size();
    // The category selector is a real focusable control above the channel list.
    if (p & NUVIO_BTN_LEFT) {
        if (m_filter > 0) { --m_filter; m_index = 0; }
    }
    if (p & NUVIO_BTN_RIGHT) {
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
    iptv_xtream::Catalog cat;
    { std::lock_guard<std::mutex> lock(m_data->lock); cat = m_data->catalog; }
    auto ch = visible(cat);
    if (p & NUVIO_BTN_UP) {
        if (m_index == 0) m_category_focus = true;
        else --m_index;
    }
    if (p & NUVIO_BTN_DOWN) m_index = std::min(std::max(0, (int)ch.size() - 1), m_index + 1);
    if (p & NUVIO_BTN_CROSS) {
        if (m_index >= 0 && m_index < (int)ch.size()) {
            a.kind = Action::PlayIPTV;
            a.iptv_title = ch[(size_t)m_index].name;
            a.iptv_url = iptv_xtream::live_url(m_creds, ch[(size_t)m_index]);
        } else refresh();
    }
    return a;
}
void IPTV::draw(double, float) {
    gfx::text(kPad,210,"IPTV",{gfx::Bold,58},kText);
    if(!m_connected) {gfx::text(kPad,340,"Configure IPTV server, username and password in Settings",{gfx::Medium,28},kText2);return;}
    iptv_xtream::Catalog cat;std::string err;bool loading;
    {std::lock_guard<std::mutex> lock(m_data->lock);cat=m_data->catalog;err=m_data->error;loading=m_data->loading;}
    gfx::text(kPad,268,"Up: Select category    Left/Right: Change category    X: Select / Play",{gfx::Medium,21},kText2);
    if(loading && cat.channels.empty()) gfx::text(kPad,360,"Loading IPTV channels...",{gfx::Medium,28},kText2);
    if(!err.empty()) gfx::text(kPad,360,err,{gfx::Medium,25,1500},kText2);
    if (m_filter > (int)m_store.categories().size()) m_filter=0;
    std::string filter=m_filter==0?"All channels":m_store.categories()[(size_t)m_filter-1].name;
    const std::string category_label = "Category: " + filter + "  <  >";
    if (m_category_focus) glass_panel({kPad-15,290,1450,60},18,1.f,false);
    gfx::text(kPad,328,category_label,{gfx::SemiBold,29},m_category_focus?kText:kText2);
    auto channels=visible(cat);
    if(m_index>=(int)channels.size()) m_index=std::max(0,(int)channels.size()-1);
    int first=std::max(0,m_index-6);
    for(int i=first;i<(int)channels.size() && i<first+12;++i) {
        float y=395+(i-first)*52;
        if(i==m_index && !m_category_focus) glass_panel({kPad-15,y-31,1450,48},18,1.f,false);
        gfx::text(kPad,y,channels[(size_t)i].name,{gfx::SemiBold,25,1350},i==m_index?kText:kText2);
    }
    if(channels.empty() && !loading) gfx::text(kPad,425,"No channels here. Choose another category or refresh.",{gfx::Medium,24},kText2);
}
} // namespace ui
