#include "ui/iptv.h"
#include "app/iptv_navigation.h"
#include "nuvio_input.h"
#include "gfx/gfx.h"
#include "gfx/art.h"
#include "ui/screen.h"
#include "platform/ime.h"
#include <cctype>
#include <ctime>
#include <algorithm>
#include <thread>
#include <map>
#include <unordered_map>

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
    if(m_filter==1) { std::vector<iptv_xtream::Channel> out; for (const auto &ch:c.channels) if(ch.source=="Emby" || ch.source=="Jellyfin") out.push_back(ch); return out; }
    if(m_filter==2) { std::vector<iptv_xtream::Channel> out; for (const auto &ch:c.channels) if(ch.source=="Xtream") out.push_back(ch); return out; }
    std::vector<iptv_xtream::Channel> out;
    if(m_filter>2 && m_filter<=(int)m_store.categories().size()+2) {
        const auto &ids=m_store.categories()[(size_t)m_filter-3].channels;
        for(const auto &id:ids) for(const auto &ch:c.channels) if(ch.source=="Xtream" && (ch.id==id || ch.id=="xtream:"+id)) {out.push_back(ch);break;}
    }
    return out;
}
void IPTV::build_groups(const iptv_live::Snapshot &snapshot) {
    const std::string previous=m_row>=0 && m_row<(int)m_groups->size()?(*m_groups)[m_row].id:"";
    auto groups=iptv_navigation::build_groups(snapshot.channels,m_store.categories(),m_search?m_query:"");
    std::map<std::string,int> positions;
    for(size_t i=0;i<m_groups->size() && i<m_columns.size();++i)positions[(*m_groups)[i].id]=m_columns[i];
    m_groups=std::make_shared<const iptv_navigation::Groups>(std::move(groups));
    m_columns.resize(m_groups->size());m_row=0;
    for(size_t i=0;i<m_groups->size();++i){
        m_columns[i]=std::clamp(positions[(*m_groups)[i].id],0,(int)(*m_groups)[i].channels.size()-1);
        if((*m_groups)[i].id==previous)m_row=(int)i;
    }
}
void IPTV::focus_channel(const std::string &group_id,const std::string &channel_id) {
    for(size_t r=0;r<m_groups->size();++r) if((*m_groups)[r].id==group_id) {
        const auto &list=(*m_groups)[r].channels;
        for(size_t c=0;c<list.size();++c) if(list[c].id==channel_id){m_row=(int)r;m_columns[r]=(int)c;return;}
    }
}
void IPTV::move_selection(int delta,bool) {
    if(m_groups->empty()) return;
    m_row=std::clamp(m_row+delta,0,(int)m_groups->size()-1);
}
void IPTV::update_hold(uint32_t held, double now) {
    const uint32_t dir=(held & NUVIO_BTN_DOWN) ? NUVIO_BTN_DOWN :
                       (held & NUVIO_BTN_UP) ? NUVIO_BTN_UP : 0;
    if(!dir || ime::active()) {m_hold_dir=0;m_hold_since=0;return;}
    if(dir!=m_hold_dir) {m_hold_dir=dir;m_hold_since=now;m_hold_last=now;return;}
    const double duration=now-m_hold_since;
    if(duration<1) return;
    const int speed=std::min(5,2+(int)(duration-1));
    if(now-m_hold_last>=0.19/speed) {move_selection(dir==NUVIO_BTN_DOWN?1:-1,false);m_hold_last=now;}
}
Action IPTV::input(uint32_t p) {
    Action a;
    if(ime::active()) return a;
    const auto snap=iptv_live::summary();
    if(snap.generation!=m_seen) {const auto full=iptv_live::snapshot();build_groups(full);m_seen=full.generation;}
    if(p & NUVIO_BTN_TRIANGLE) {
        m_search=true;
        ime::request(ime::Kind::Text,"Search Live TV channels",m_query,
            [this](const std::string &q){m_query=q;m_row=0;build_groups(iptv_live::snapshot());});
        return a;
    }
    if(m_search && (p & NUVIO_BTN_CIRCLE)) {m_search=false;m_query.clear();build_groups(iptv_live::snapshot());return a;}
    if(p & NUVIO_BTN_UP) {if(m_row==0) a.kind=Action::ToNav;else move_selection(-1);}
    if(p & NUVIO_BTN_DOWN) move_selection(1);
    if(m_row>=0 && m_row<(int)m_groups->size()) {
        auto &group=(*m_groups)[(size_t)m_row];
        int &col=m_columns[(size_t)m_row];
        if(p & NUVIO_BTN_LEFT) col=std::max(0,col-1);
        if(p & NUVIO_BTN_RIGHT) col=std::min((int)group.channels.size()-1,col+1);
        if((p & NUVIO_BTN_CROSS) && col<(int)group.channels.size()) {
            const auto &selected=group.channels[(size_t)col];
            // Launch with its exact row identity, including overlapping custom groups.
            auto playlist=m_groups;size_t row=(size_t)m_row,column=(size_t)col;
            if(m_search) {
                playlist=std::make_shared<const iptv_navigation::Groups>(
                    iptv_navigation::build_groups(iptv_live::snapshot().channels,m_store.categories()));
                bool found=false;
                for(size_t r=0;r<playlist->size() && !found;++r) if((*playlist)[r].id==group.id)
                    for(size_t c=0;c<(*playlist)[r].channels.size();++c)
                        if((*playlist)[r].channels[c].id==selected.id){row=r;column=c;found=true;break;}
                if(!found)return a;
            }
            if(iptv_navigation::begin(playlist,row,column))a.kind=Action::PlayLiveTV;
        }
    }
    return a;
}
void IPTV::draw(double, float) {
    gfx::text(kPad,210,"Live TV",{gfx::Bold,58},kText);
    const auto snap=iptv_live::summary();
    if(snap.generation!=m_seen) {const auto full=iptv_live::snapshot();build_groups(full);m_seen=full.generation;}
    if(snap.loading && m_groups->empty()) {
        gfx::text(kPad,345,"Loading Live TV channels...",{gfx::Medium,28},kText2);return;
    }
    if(m_groups->empty()) {
        gfx::text(kPad,345,"No channels available. Add a playlist or Xtream account in Settings.",{gfx::Medium,26},kText2);return;
    }
    constexpr float cardW=260, cardH=146, gap=24;
    // Keep the selected row centred, with its neighbours visible above/below.
    for(int r=0;r<(int)m_groups->size();++r) {
        const float y=430+(r-m_row)*255.f;
        if(y<230 || y>1050) continue;
        const Group &row=(*m_groups)[(size_t)r];
        gfx::text(kPad,y-35,row.title,{gfx::SemiBold,31,1500},r==m_row?kText:kText2);
        const int focus=m_columns[(size_t)r];
        const int begin=std::max(0,focus-2);
        for(int i=begin;i<(int)row.channels.size() && i<begin+6;++i) {
            const float x=kPad+(i-begin)*(cardW+gap);
            const bool selected=r==m_row && i==focus;
            const auto &ch=row.channels[(size_t)i];
            gfx::Rect box{x,y,cardW,cardH};
            glass_panel(box,14,selected?1.f:0.48f,false);
            if(!ch.logo.empty()) if(const gfx::Texture *image=art::get(ch.logo,320,200)) {
                gfx::image({x+16,y+8,cardW-32,cardH-42},image,1.f,0,true);
            }
            gfx::text(x+10,y+cardH-14,ch.name,{gfx::SemiBold,19,cardW-20},selected?kText:kText2);
            if(selected) gfx::text(x+8,y+cardH+25,ch.source,{gfx::Medium,17},kText2);
        }
    }
    if(m_row>=0 && m_row<(int)m_groups->size()) {
        const auto &row=(*m_groups)[(size_t)m_row];
        const auto &ch=row.channels[(size_t)m_columns[(size_t)m_row]];
        if(ch.source=="Xtream" && ch.id!=m_guide_requested) {
            m_guide_requested=ch.id;iptv_live::request_guide(&m_client,ch);
        }
        if(!ch.now.empty()) gfx::text(kPad,1005,"Now: "+ch.now,{gfx::Medium,22,1350},kText2);
        if(!ch.next.empty()) gfx::text(kPad,1040,"Next: "+ch.next,{gfx::Medium,20,1350},kText2);
    }
}
} // namespace ui
