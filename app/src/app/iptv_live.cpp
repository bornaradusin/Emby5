/* Emby5 unified Emby Live TV and Xtream channels. GPL-3.0-or-later */
#include "app/iptv_live.h"
#include "app/i18n.h"
#include "app/io_worker.h"
#include "app/xtream_fetch.h"
#include <memory>
#include <atomic>
#include "app/iptv_m3u.h"
#include "jf/jf_http.h"
#include "evo_data_path.h"
extern "C" {
#include "cJSON.h"
}
#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace iptv_live {
namespace {
std::mutex mu;
Snapshot cache;
std::string identity;
uint64_t epoch=0;
bool refreshing=false;
std::string selected_playback_channel;
std::chrono::steady_clock::time_point last_refresh{};
// Stable provider-specific filename; credentials are used only in the hash.
std::string live_cache_path(const std::string &key) {
    uint64_t hash=1469598103934665603ULL;
    for(unsigned char c:key) hash=(hash^c)*1099511628211ULL;
    char filename[80];
    std::snprintf(filename,sizeof filename,"emby5/live-cache-%016llx.json",(unsigned long long)hash);
    return evo_data_path(filename);
}
// Cache only channel identity and display metadata. Do not persist stream URLs,
// M3U credentials or transient EPG entries.
void save_live_cache(const std::string &key, const Snapshot &snap, uint64_t ticket, const std::string &storage_key={}) {
    if(evo_mkdir(evo_data_dir())!=0 || evo_mkdir(evo_data_path("emby5"))!=0)return;
    cJSON *doc=cJSON_CreateObject();if(!doc)return;
    cJSON_AddNumberToObject(doc,"version",2);
    cJSON_AddItemToObject(doc,"xtreamScan",iptv_catalogue::scan_json(snap.xtream_scan));
    cJSON *items=cJSON_AddArrayToObject(doc,"channels");
    for(const auto &ch:snap.channels) {
        if(ch.source!="Emby" && ch.source!="Jellyfin" && ch.source!="Xtream")continue;
        cJSON *item=cJSON_CreateObject();if(!item)continue;
        cJSON_AddStringToObject(item,"id",ch.id.c_str());
        cJSON_AddStringToObject(item,"embyId",ch.emby_id.c_str());
        cJSON_AddStringToObject(item,"name",ch.name.c_str());
        cJSON_AddStringToObject(item,"group",ch.category_id.c_str());
        cJSON_AddStringToObject(item,"categoryId",ch.category_key.c_str());
        cJSON_AddStringToObject(item,"logo",ch.logo.c_str());
        cJSON_AddStringToObject(item,"ext",ch.extension.c_str());
        cJSON_AddStringToObject(item,"source",ch.source.c_str());
        cJSON_AddItemToArray(items,item);
    }
    char *data=cJSON_PrintUnformatted(doc);cJSON_Delete(doc);if(!data)return;
    const auto path=live_cache_path(storage_key.empty()?key:storage_key),tmp=path+".tmp-"+std::to_string(ticket);
    FILE *f=std::fopen(tmp.c_str(),"wb");bool ok=false;
    if(f){size_t size=std::strlen(data);ok=std::fwrite(data,1,size,f)==size;ok=std::fclose(f)==0 && ok;}
    std::free(data);
    if(ok){std::lock_guard<std::mutex> guard(mu);ok=epoch==ticket&&identity==key&&std::rename(tmp.c_str(),path.c_str())==0;}
    if(!ok)std::remove(tmp.c_str());
}
Snapshot load_live_cache(const std::string &key) {
    Snapshot result;
    FILE *f=std::fopen(live_cache_path(key).c_str(),"rb");if(!f)return result;
    std::string data;char buffer[8192];size_t n=0;
    while((n=std::fread(buffer,1,sizeof buffer,f))>0) {
        if(data.size()+n>(32u<<20)){data.clear();break;}
        data.append(buffer,n);
    }
    std::fclose(f);if(data.empty())return result;
    cJSON *doc=cJSON_Parse(data.c_str());if(!doc)return result;
    const double version=cJSON_GetNumberValue(cJSON_GetObjectItem(doc,"version"));
    if(version==1||version==2) {
        if(version==2)result.xtream_scan=iptv_catalogue::read_scan(cJSON_GetObjectItemCaseSensitive(doc,"xtreamScan"));
        const cJSON *item;
        cJSON_ArrayForEach(item,cJSON_GetObjectItem(doc,"channels")) {
            const auto get=[&](const char *k) -> std::string {
                const cJSON *v=cJSON_GetObjectItemCaseSensitive(item,k);
                return cJSON_IsString(v)&&v->valuestring?v->valuestring:"";
            };
            iptv_xtream::Channel ch;
            ch.id=get("id");ch.emby_id=get("embyId");ch.name=get("name");
            ch.category_id=get("group");ch.category_key=get("categoryId");ch.logo=get("logo");
            ch.extension=get("ext");ch.source=get("source");
            if(ch.id.empty()||ch.name.empty())continue;
            if(ch.source=="Emby" || ch.source=="Jellyfin")++result.emby_count;
            else if(ch.source=="Xtream")++result.xtream_count;
            else continue;
            result.channels.push_back(std::move(ch));
        }
    }
    cJSON_Delete(doc);return result;
}
void update_status() { // caller holds mu
    cache.emby_count=cache.xtream_count=cache.m3u_count=0;
    for(const auto &ch:cache.channels) {
        if(ch.source=="Xtream")++cache.xtream_count;
        else if(ch.source=="M3U")++cache.m3u_count;
        else if(ch.source=="Emby"||ch.source=="Jellyfin")++cache.emby_count;
    }
    cache.loading=refreshing;
    cache.emby_loading=cache.server.loading;cache.xtream_loading=cache.xtream.loading;
    cache.emby_error=cache.server.error;cache.xtream_error=cache.xtream.error;cache.m3u_error=cache.playlist.error;
    ++cache.generation;
}
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
bool channel_by_id(const std::string &id,iptv_xtream::Channel *channel) {
    std::lock_guard<std::mutex> guard(mu);
    if(!channel)return false;
    const auto it=std::find_if(cache.channels.begin(),cache.channels.end(),[&](const auto &c){return c.id==id;});
    if(it==cache.channels.end())return false;
    *channel=*it;return true;
}
void set_playing_channel(const std::string &id) { std::lock_guard<std::mutex> g(mu); selected_playback_channel=id; }
std::string playing_channel() { std::lock_guard<std::mutex> g(mu); return selected_playback_channel; }
Snapshot snapshot() { std::lock_guard<std::mutex> g(mu); return cache; }
Snapshot summary() {
    std::lock_guard<std::mutex> g(mu);Snapshot s;
    s.emby_count=cache.emby_count;s.xtream_count=cache.xtream_count;s.m3u_count=cache.m3u_count;
    s.loading=cache.loading;s.emby_loading=cache.emby_loading;s.xtream_loading=cache.xtream_loading;
    s.server=cache.server;s.xtream=cache.xtream;s.playlist=cache.playlist;
    s.generation=cache.generation;s.catalogue_generation=cache.catalogue_generation;s.emby_error=cache.emby_error;s.xtream_error=cache.xtream_error;s.m3u_error=cache.m3u_error;
    return s;
}
void refresh(jf::Client *client, bool force, bool retry_failed) {
    if(!client||iptv_catalogue::playback_active())return;
    iptv_xtream::Credentials creds;const bool has_xtream=iptv_xtream::load_credentials(&creds);
    const auto m3u_url=iptv_m3u::playlist_url();
    const std::string key=std::string(client->server_type())+"|"+client->server()+"|"+client->user_id()+"|"+
        (has_xtream?creds.server+"|"+creds.username+"|"+creds.password:"")+"|"+m3u_url;
    uint64_t ticket;bool read_disk=false;
    {
        std::lock_guard<std::mutex> g(mu);
        if(key!=identity){identity=key;++epoch;const auto generation=cache.generation+1;
            cache=Snapshot{};cache.generation=generation;cache.catalogue_generation=generation;read_disk=true;refreshing=false;last_refresh={};}
        if(refreshing)return;
        if(!force&&last_refresh.time_since_epoch().count()&&cache.xtream.error!="Refresh paused during playback") {
            const bool failed=!cache.emby_error.empty()||!cache.xtream_error.empty()||!cache.m3u_error.empty();
            if(std::chrono::steady_clock::now()-last_refresh<(failed?std::chrono::seconds(60):std::chrono::seconds(300)))return;
        }
        ticket=++epoch;refreshing=true;cache.loading=true;
        cache.server.configured=true;cache.xtream.configured=has_xtream;cache.playlist.configured=!m3u_url.empty();
        cache.server.loading=true;cache.xtream.loading=has_xtream;cache.playlist.loading=!m3u_url.empty();
        update_status();
    }
    auto failed=[ticket]{
        std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
        for(auto *s:{&cache.server,&cache.xtream,&cache.playlist})if(s->loading){
            s->loading=false;s->cached=true;s->error=N_("Live TV worker failed");
        }
        refreshing=false;last_refresh=std::chrono::steady_clock::now();update_status();
    };
    // A worker owns its session snapshot; it cannot send an old catalogue request
    // using a newly selected user's token or dereference a retired UI client.
    std::shared_ptr<jf::Client> server;
    try {
        server=std::make_shared<jf::Client>(client->server(),client->device_id(),client->device_name(),client->server_kind());
        server->set_session(client->token(),client->user_id(),client->user_name());
    } catch(...) {failed();return;}
    const bool started=emby5::spawn_io([server,creds,has_xtream,m3u_url,key,ticket,read_disk,retry_failed,force]{
        if(read_disk){
            auto disk=load_live_cache(key);
            if(has_xtream && !disk.xtream_count) {
                const auto provider=load_live_cache("xtream|"+creds.server+"|"+creds.username+"|"+creds.password);
                for(const auto &channel:provider.channels)if(channel.source=="Xtream")disk.channels.push_back(channel);
                disk.xtream_count=provider.xtream_count;disk.xtream_scan=provider.xtream_scan;
            }
            std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
            cache.channels=std::move(disk.channels);++cache.catalogue_generation;cache.xtream_scan=std::move(disk.xtream_scan);
            cache.xtream=iptv_catalogue::result_state(cache.xtream_scan,true);
            cache.xtream.configured=has_xtream;cache.xtream.loading=has_xtream;
            cache.server.cached=disk.emby_count!=0;update_status();
        }
        auto cancelled=[ticket]{std::lock_guard<std::mutex> g(mu);return epoch!=ticket;};
        auto publish=[&](const std::string &source,std::vector<iptv_xtream::Channel> channels,
                         bool replace,iptv_catalogue::State state,const iptv_catalogue::Scan *scan=nullptr){
            std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
            if(replace){
                cache.channels.erase(std::remove_if(cache.channels.begin(),cache.channels.end(),
                    [&](const iptv_xtream::Channel &c){return c.source==source;}),cache.channels.end());
                cache.channels.insert(cache.channels.end(),std::make_move_iterator(channels.begin()),std::make_move_iterator(channels.end()));
                ++cache.catalogue_generation;
            }
            if(source=="Xtream"){cache.xtream=std::move(state);if(scan)cache.xtream_scan=*scan;}
            else if(source=="M3U")cache.playlist=std::move(state);
            else cache.server=std::move(state);
            update_status();
        };
        bool retry_targets=false;
        {std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
            retry_targets=retry_failed&&(!cache.server.error.empty()||!cache.xtream.error.empty()||!cache.playlist.error.empty());}
        auto should_skip=[&](int source) {
            std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return true;
            auto &state=source==0?cache.server:source==1?cache.xtream:cache.playlist;
            if(retry_targets&&state.error.empty()){state.loading=false;update_status();return true;}
            return false;
        };
        auto source_failure=[&](const std::string &name,const char *error) {
            iptv_catalogue::State s;s.configured=true;s.cached=true;s.error=error;
            publish(name,{},false,std::move(s));
        };
        auto server_job=[&] {
            if(should_skip(0))return;
            try {
                std::vector<iptv_xtream::Channel> channels;std::string body;
                bool ok=server->get_json("/LiveTv/Channels?UserId="+server->user_id()+"&Limit=10000&EnableImages=true",&body);
                if(ok) {
                    iptv_catalogue::Json j(cJSON_Parse(body.c_str()),cJSON_Delete);
                    ok=cJSON_IsArray(entries(j.get()));const cJSON *v;
                    if(ok)cJSON_ArrayForEach(v,entries(j.get())) {
                        iptv_xtream::Channel ch;ch.emby_id=field(v,"Id");ch.id=std::string(server->server_type())+":"+ch.emby_id;
                        ch.name=field(v,"Name");ch.source=server->server_product();
                        ch.logo=server->image_url(ch.emby_id,"Primary",field(cJSON_GetObjectItemCaseSensitive(v,"ImageTags"),"Primary"),240,true);
                        if(!ch.emby_id.empty()&&!ch.name.empty())channels.push_back(std::move(ch));
                    }
                }
                iptv_catalogue::State s;s.configured=true;s.cached=!ok;
                if(!ok)s.error=N_("Server channels unavailable");
                publish(server->server_product(),std::move(channels),ok,std::move(s));
            }catch(...){source_failure(server->server_product(),N_("Server channel worker failed"));}
        };
        auto xtream_job=[&] {
            if(should_skip(1))return;
            try {
                iptv_xtream::Catalog cat;
                {
                    std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                    cat.scan=cache.xtream_scan;cat.categories=cat.scan.categories;
                    for(const auto &ch:cache.channels)if(ch.source=="Xtream") {
                        cat.channels.push_back(ch);auto &v=cat.channels.back();
                        if(v.id.rfind("xtream:",0)==0)v.id.erase(0,7);
                        if(!v.category_key.empty())v.category_id=v.category_key;
                    }
                }
                std::string error;
                // Catalogue endpoints already authenticate the request. Do not
                // block the whole list behind an extra account-info request.
                const bool retry=retry_failed||(!force&&!cat.scan.complete);
                auto progress=[&](const iptv_catalogue::Scan &scan){
                    std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                    cache.xtream.done=scan.done;cache.xtream.total=scan.total;cache.xtream.failed=scan.failed.size();
                    ++cache.generation;
                };
                auto last_saved=std::chrono::steady_clock::time_point{};
                auto available=[&](const iptv_xtream::Catalog &partial) {
                    if(cancelled())return;
                    std::unordered_map<std::string,std::string> labels;
                    for(const auto &category:partial.categories)labels[category.id]=category.name;
                    auto channels=partial.channels;
                    for(auto &channel:channels) {
                        if(channel.category_key.empty())channel.category_key=channel.category_id;
                        const auto name=labels.find(channel.category_key);
                        if(name!=labels.end()&&!name->second.empty())channel.category_id=name->second;
                        channel.source="Xtream";if(channel.id.rfind("xtream:",0)!=0)channel.id="xtream:"+channel.id;
                    }
                    auto state=iptv_catalogue::result_state(partial.scan,true);state.loading=true;
                    publish("Xtream",std::move(channels),true,std::move(state),&partial.scan);
                    const auto now=std::chrono::steady_clock::now();
                    if(now-last_saved>=std::chrono::seconds(5)) {
                        Snapshot saved;{std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;saved=cache;}
                        save_live_cache(key,saved,ticket);
                        saved.channels.erase(std::remove_if(saved.channels.begin(),saved.channels.end(),[](const auto &ch){return ch.source!="Xtream";}),saved.channels.end());
                        save_live_cache(key,saved,ticket,"xtream|"+creds.server+"|"+creds.username+"|"+creds.password);
                        last_saved=now;
                    }
                };
                const bool ok=iptv_xtream::load_catalog(creds,&cat,&error,120,cancelled,retry,progress,available);
                if(cancelled())return;
                std::unordered_map<std::string,std::string> names;
                for(const auto &c:cat.categories)names[c.id]=c.name;
                for(auto &ch:cat.channels) {
                    if(ch.category_key.empty())ch.category_key=ch.category_id;
                    const auto it=names.find(ch.category_key);if(it!=names.end()&&!it->second.empty())ch.category_id=it->second;
                    ch.source="Xtream";ch.id="xtream:"+ch.id;
                }
                auto state=iptv_catalogue::result_state(cat.scan,!ok&&!cat.channels.empty());
                // The loader has already merged failed categories with the cache.
                // Publish the usable partial result, not just all-or-nothing success.
                publish("Xtream",std::move(cat.channels),true,std::move(state),&cat.scan);
            }catch(...){source_failure("Xtream",N_("Xtream channel worker failed"));}
        };
        auto m3u_job=[&] {
            if(should_skip(2))return;
            try {
                iptv_xtream::Catalog cat;std::string error;
                const bool ok=iptv_m3u::load_catalog(m3u_url,&cat,&error);
                cat.channels.erase(std::remove_if(cat.channels.begin(),cat.channels.end(),[](const iptv_xtream::Channel &ch){
                    auto path=ch.extension.substr(0,ch.extension.find('?'));
                    std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return (char)std::tolower(c);});
                    const auto dot=path.rfind('.');const auto ext=dot==std::string::npos?"":path.substr(dot+1);
                    return ext=="mp4"||ext=="mkv"||ext=="avi"||ext=="mov"||ext=="webm";
                }),cat.channels.end());
                iptv_catalogue::State state;state.configured=true;state.cached=!ok;
                if(!ok)state.error=N_("M3U channels unavailable");
                publish("M3U",std::move(cat.channels),ok,std::move(state));
            }catch(...){source_failure("M3U",N_("M3U channel worker failed"));}
        };
        std::vector<std::function<void()>> jobs{server_job};
        if(has_xtream)jobs.push_back(xtream_job);
        if(!m3u_url.empty())jobs.push_back(m3u_job);
        emby5::run_io_jobs(jobs);
        if(cancelled())return;
        Snapshot saved;
        {std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;saved=cache;}
        save_live_cache(key,saved,ticket);
        if(has_xtream) {
            Snapshot provider=saved;
            provider.channels.erase(std::remove_if(provider.channels.begin(),provider.channels.end(),[](const auto &ch){return ch.source!="Xtream";}),provider.channels.end());
            save_live_cache(key,provider,ticket,"xtream|"+creds.server+"|"+creds.username+"|"+creds.password);
        }
        {
            std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
            refreshing=false;cache.server.loading=cache.xtream.loading=cache.playlist.loading=false;
            last_refresh=std::chrono::steady_clock::now();update_status();
        }
        // EPG enrichment is optional and does not hold catalogue loading/retry.
        std::string body;
        if(saved.emby_count&&server->get_json("/LiveTv/Programs?Limit=1000&SortBy=StartDate&SortOrder=Ascending",&body)) {
            iptv_catalogue::Json j(cJSON_Parse(body.c_str()),cJSON_Delete);add_guide(saved.channels,entries(j.get()));
            std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
            std::unordered_map<std::string,const iptv_xtream::Channel *> by_id;
            for(const auto &ch:saved.channels)if(!ch.emby_id.empty())by_id[ch.id]=&ch;
            for(auto &ch:cache.channels)if(!ch.emby_id.empty()){
                auto it=by_id.find(ch.id);if(it!=by_id.end())ch=*it->second;
            }
            ++cache.generation;
        }
    },failed);
    if(!started)failed();
}
void request_guide(jf::Client *client, const iptv_xtream::Channel &channel) {
    if (!client || channel.source != "Xtream") return;
    iptv_xtream::Credentials creds;
    if (!iptv_xtream::load_credentials(&creds)) return;
    static std::atomic<bool> guide_busy{false};
    bool expected=false;if(!guide_busy.compare_exchange_strong(expected,true))return;
    uint64_t ticket; {std::lock_guard<std::mutex> g(mu);ticket=epoch;}
    if(!emby5::spawn_io([ticket,creds,channel] {
        struct Release {~Release(){guide_busy.store(false);}} release;
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
        if (ticket != epoch) return;
        for (auto &ch : cache.channels) if (ch.id==channel.id) {
            ch.now=now; ch.next=next; ch.now_start=now_start; ch.now_end=now_end;
            ch.next_start=next_start; ch.next_end=next_end; ++cache.generation; break;
        }
    }))guide_busy.store(false);
}
}
