/* Emby5 - GPL-3.0-or-later. No network, engine or storage calls here. */
#include "app/iptv_navigation.h"
#include "app/i18n.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
namespace iptv_navigation {
namespace {
std::string lower(std::string s) {
    for (auto &c:s) c=(char)std::tolower((unsigned char)c);
    return s;
}
struct Session {
    Playlist groups;
    std::string token;
    size_t group=0, channel=0, pending_group=0, pending_channel=0;
    bool pending=false, require_release=false;
};
std::mutex lock;
Session session;
uint64_t serial=0;
bool valid() {
    return session.groups && session.group<session.groups->size() &&
        session.channel<(*session.groups)[session.group].channels.size();
}
size_t step(size_t n,int direction,size_t count) {
    return direction>0 ? (n+1)%count : (n+count-1)%count;
}
}
Groups build_groups(const std::vector<iptv_xtream::Channel> &channels,
                    const std::vector<iptv_categories::Category> &custom,
                    const std::string &search) {
    Groups result;
    std::unordered_map<std::string,const iptv_xtream::Channel*> by_id;
    const auto query=lower(search);
    const auto visible=[&](const iptv_xtream::Channel &c){
        return !c.id.empty() && (query.empty() || lower(c.name).find(query)!=std::string::npos);
    };
    for(const auto &c:channels) if(visible(c)) by_id.emplace((c.id.find(':')==std::string::npos?"xtream:"+c.id:c.id),&c);
    for(const auto &category:custom) {
        Group row{"custom:"+category.id,category.name,{}};
        std::unordered_set<std::string> used;
        for(const auto &id:category.channels) {
            auto it=by_id.find((id.find(':')==std::string::npos?"xtream:"+id:id));
            if(it!=by_id.end() && used.insert(it->second->id).second) row.channels.push_back(*it->second);
        }
        if(!row.channels.empty()) result.push_back(std::move(row));
    }
    std::unordered_map<std::string,size_t> positions;
    std::unordered_set<std::string> used;
    for(const auto &c:channels) {
        if(!visible(c) || !used.insert(c.source+"\n"+c.id).second) continue;
        const auto name=c.category_id.empty()?
            ((c.source=="Emby"||c.source=="Jellyfin")?c.source+" Live TV":"Other Channels"):c.category_id;
        /* The id keeps the English name (pins and positions are stored by it); the title is translated. */
        const auto title=c.category_id.empty()?
            ((c.source=="Emby"||c.source=="Jellyfin")?TF("{source} Live TV",{{"source",c.source}}):std::string(T("Other Channels"))):
            c.category_id=="Other Channels"?std::string(T("Other Channels")):c.category_id;
        const auto id="provider:"+c.source+":"+(c.category_key.empty()?name:c.category_key);
        auto it=positions.find(id);
        if(it==positions.end()) {
            positions[id]=result.size();result.push_back({id,title,{c}});
        } else result[it->second].channels.push_back(c);
    }
    return result;
}
bool begin(Playlist groups,size_t group,size_t channel) {
    std::lock_guard<std::mutex> guard(lock);
    session={};
    if(!groups || group>=groups->size() || channel>=(*groups)[group].channels.size()) return false;
    session.groups=std::move(groups);session.group=group;session.channel=channel;
    session.token=std::to_string(++serial);return true;
}
bool current(Selection *out) {
    std::lock_guard<std::mutex> guard(lock);
    if(!out || !valid()) return false;
    const auto &group=(*session.groups)[session.group];
    *out={group.channels[session.channel],group.id,group.title,session.token};return true;
}
bool matches(const std::string &token,const std::string &id) {
    std::lock_guard<std::mutex> guard(lock);
    return valid() && !token.empty() && token==session.token &&
        (*session.groups)[session.group].channels[session.channel].id==id;
}
bool input_ready(const std::string &token,bool directions_down) {
    std::lock_guard<std::mutex> guard(lock);
    if(!valid() || token.empty() || token!=session.token)return false;
    if(!directions_down)session.require_release=false;
    return !session.require_release;
}
bool request_move(const std::string &token,const std::string &id,int group_step,int channel_step) {
    std::lock_guard<std::mutex> guard(lock);
    if(!valid() || session.pending || session.require_release || token.empty() || token!=session.token ||
       (*session.groups)[session.group].channels[session.channel].id!=id) return false;
    if(!((group_step==0 && (channel_step==1||channel_step==-1)) ||
         (channel_step==0 && (group_step==1||group_step==-1)))) return false;
    const auto &groups=*session.groups;
    size_t g=session.group,c=session.channel;
    if(group_step) {
        for(size_t n=0;n<groups.size();++n) {
            g=step(g,group_step,groups.size());
            if(!groups[g].channels.empty()) break;
        }
        c=0;
    } else c=step(c,channel_step,groups[g].channels.size());
    if(g==session.group && c==session.channel) return false; // no needless reopen
    session.pending=true;session.pending_group=g;session.pending_channel=c;return true;
}
bool advance() {
    std::lock_guard<std::mutex> guard(lock);
    if(!valid() || !session.pending) return false;
    session.group=session.pending_group;session.channel=session.pending_channel;session.pending=false;session.require_release=true;
    // Change the token on every transition: a late command from the previous
    // player can never skip another channel, even if that channel ID is reused.
    session.token=std::to_string(++serial);return true;
}
void end() {std::lock_guard<std::mutex> guard(lock);session={};}
}
