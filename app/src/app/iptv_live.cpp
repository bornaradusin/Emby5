/* Emby5 unified Emby Live TV and Xtream channels. GPL-3.0-or-later */
#include "app/iptv_live.h"
#include "jf/jf_http.h"
extern "C" {
#include "cJSON.h"
}
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace iptv_live {
namespace {
std::mutex mu;
Snapshot cache;
std::string identity;
std::string selected_playback_channel;
std::chrono::steady_clock::time_point last_refresh{};
std::string field(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    if (cJSON_IsNumber(v)) return std::to_string(v->valueint);
    return {};
}
const cJSON *entries(const cJSON *root) {
    return cJSON_IsArray(root) ? root : cJSON_GetObjectItemCaseSensitive(root, "Items");
}

std::string decode_title(const std::string &s) {
    static const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (s.size()<4 || s.size()%4) return s;
    std::string out; unsigned value=0; int bits=-8;
    for (unsigned char c : s) {
        if (c=='=') break;
        const size_t pos=alphabet.find((char)c);
        if (pos==std::string::npos) return s;
        value=(value<<6)|(unsigned)pos; bits+=6;
        if (bits>=0) {out.push_back((char)((value>>bits)&255)); bits-=8;}
    }
    // Only accept valid UTF-8 or printable ASCII; otherwise preserve original.
    for (unsigned char c:out) if (c<32 && c!='\t') return s;
    return out.empty()?s:out;
}

std::string stamp(const std::string &date) {
    // ISO start times; dates on the EPG are deliberately shown as returned by the server.
    return date.size() >= 16 ? date.substr(11,5) : date;
}
// ISO-8601 with UTC or numeric timezone offset, converted to Unix seconds.
int64_t iso_epoch(const std::string &s) {
    int y=0,m=0,d=0,h=0,mi=0,sec=0;
    if (std::sscanf(s.c_str(),"%d-%d-%dT%d:%d:%d",&y,&m,&d,&h,&mi,&sec)<5) return 0;
    if (m<1 || m>12 || d<1 || d>31 || h>23 || mi>59 || sec>60) return 0;
    y -= m<=2;
    const int era=(y>=0 ? y : y-399)/400;
    const unsigned yoe=(unsigned)(y-era*400);
    const unsigned doy=(153u*(unsigned)(m+(m>2?-3:9))+2)/5+(unsigned)d-1;
    const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
    int64_t epoch=((int64_t)era*146097+(int64_t)doe-719468)*86400+h*3600+mi*60+sec;
    // Emby sends either Z or an explicit timezone offset.
    if (s.size()>19) {
        const auto tz=s.find_first_of("+-",19);
        if (tz!=std::string::npos) {
            int oh=0,om=0;
            if (std::sscanf(s.c_str()+tz+1,"%2d:%2d",&oh,&om)>=1)
                epoch+=(s[tz]=='+'?-1:1)*(oh*3600+om*60);
        }
    }
    return epoch;
}
int64_t number(const cJSON *o, const char *name) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,name);
    if (cJSON_IsNumber(v)) return (int64_t)v->valuedouble;
    if (cJSON_IsString(v) && v->valuestring) return std::strtoll(v->valuestring,nullptr,10);
    return 0;
}
void add_guide(std::vector<iptv_xtream::Channel> &channels, const cJSON *items) {
    const int64_t now=(int64_t)std::time(nullptr);
    const cJSON *row;
    cJSON_ArrayForEach(row, items) {
        const std::string channel=field(row,"ChannelId");
        const std::string name=field(row,"Name");
        if (channel.empty() || name.empty()) continue;
        const int64_t start=iso_epoch(field(row,"StartDate"));
        const int64_t end=iso_epoch(field(row,"EndDate"));
        const bool airing=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row,"IsAiring")) ||
                          (start>0 && start<=now && end>now);
        for (auto &ch:channels) if (ch.emby_id==channel) {
            if (airing && (ch.now.empty() || start>ch.now_start)) {
                ch.now=name; ch.now_start=start; ch.now_end=end; ch.guide_time=stamp(field(row,"StartDate"));
            } else if (start>now && (ch.next.empty() || start<ch.next_start)) {
                ch.next=name; ch.next_start=start; ch.next_end=end;
            }
            break;
        }
    }
}
}
void set_playing_channel(const std::string &id) { std::lock_guard<std::mutex> g(mu); selected_playback_channel=id; }
std::string playing_channel() { std::lock_guard<std::mutex> g(mu); return selected_playback_channel; }
Snapshot snapshot() { std::lock_guard<std::mutex> g(mu); return cache; }
void refresh(jf::Client *client, bool force) {
    if (!client || !client->signed_in()) return;
    iptv_xtream::Credentials creds;
    bool has_xtream = iptv_xtream::load_credentials(&creds);
    const std::string key = client->server()+"|"+client->user_id()+"|"+
        (has_xtream ? creds.server+"|"+creds.username+"|"+creds.password : "");
    {
        std::lock_guard<std::mutex> g(mu);
        if (key != identity) { identity=key; cache=Snapshot{}; ++cache.generation; force=true; }
        if (cache.loading) return;
        if (!force && last_refresh.time_since_epoch().count() &&
            std::chrono::steady_clock::now()-last_refresh < std::chrono::minutes(5)) return;
        cache.loading=true;
        last_refresh=std::chrono::steady_clock::now();
    }
    std::thread([client,creds,has_xtream,key] {
        std::vector<iptv_xtream::Channel> all;
        unsigned emby_count=0, xtream_count=0;
        std::string body;
        if (client->get_json("/LiveTv/Channels?UserId="+client->user_id()+"&Limit=10000&EnableImages=true", &body)) {
            cJSON *j=cJSON_Parse(body.c_str());
            const cJSON *v;
            cJSON_ArrayForEach(v,entries(j)) {
                iptv_xtream::Channel ch;
                ch.emby_id=field(v,"Id"); ch.id="emby:"+ch.emby_id;
                ch.name=field(v,"Name"); ch.source="Emby";
                ch.logo=client->image_url(ch.emby_id,"Primary",field(cJSON_GetObjectItemCaseSensitive(v,"ImageTags"),"Primary"),240,true);
                if (!ch.emby_id.empty() && !ch.name.empty()) { all.push_back(std::move(ch)); ++emby_count; }
            }
            cJSON_Delete(j);
        }
        if (emby_count && client->get_json("/LiveTv/Programs?Limit=1000&SortBy=StartDate&SortOrder=Ascending",&body)) {
            cJSON *j=cJSON_Parse(body.c_str());
            add_guide(all,entries(j)); cJSON_Delete(j);
        }
        if (has_xtream) {
            iptv_xtream::Catalog cat; std::string error;
            if (iptv_xtream::authenticate(creds,&error) && iptv_xtream::load_catalog(creds,&cat,&error)) {
                for (auto &ch : cat.channels) {
                    ch.source="Xtream";
                    ch.id="xtream:"+ch.id; // Prevent source ID collisions.
                    ++xtream_count; all.push_back(std::move(ch));
                }
            }
        }
        std::lock_guard<std::mutex> g(mu);
        if (key != identity) return;
        cache.channels=std::move(all);
        cache.emby_count=emby_count; cache.xtream_count=xtream_count;
        cache.loading=false; ++cache.generation;
    }).detach();
}
void request_guide(jf::Client *client, const iptv_xtream::Channel &channel) {
    if (!client || channel.source != "Xtream") return;
    iptv_xtream::Credentials creds;
    if (!iptv_xtream::load_credentials(&creds)) return;
    const std::string key=client->server()+"|"+client->user_id()+"|"+creds.server+"|"+creds.username+"|"+creds.password;
    std::thread([key,creds,channel] {
        std::string url=iptv_xtream::api_url(creds,"get_short_epg")+"&stream_id="+channel.id.substr(7)+"&limit=8";
        jf::HttpResponse response=jf::http_request("GET",url,{},"",15);
        if (!response.ok()) return;
        cJSON *j=cJSON_Parse(response.body.c_str());
        const cJSON *list=cJSON_GetObjectItemCaseSensitive(j,"epg_listings");
        std::string now,next;
        int64_t now_start=0,now_end=0,next_start=0,next_end=0;
        const int64_t current=(int64_t)std::time(nullptr);
        const cJSON *entry;
        cJSON_ArrayForEach(entry,list) {
            const std::string title=decode_title(field(entry,"title"));
            if (title.empty()) continue;
            const int64_t start=number(entry,"start_timestamp");
            const int64_t end=number(entry,"stop_timestamp");
            if (start>0 && end>start) {
                if (start<=current && current<end && (now.empty() || start>now_start)) {
                    now=title;now_start=start;now_end=end;
                } else if (start>current && (next.empty() || start<next_start)) {
                    next=title;next_start=start;next_end=end;
                }
            } else if (now.empty()) now=title;
            else if (next.empty()) next=title;
        }
        cJSON_Delete(j);
        if (now.empty() && next.empty()) return;
        std::lock_guard<std::mutex> g(mu);
        if (key != identity) return;
        for (auto &ch : cache.channels) if (ch.id==channel.id) {
            ch.now=now; ch.next=next; ch.now_start=now_start; ch.now_end=now_end;
            ch.next_start=next_start; ch.next_end=next_end; ++cache.generation; break;
        }
    }).detach();
}
}
