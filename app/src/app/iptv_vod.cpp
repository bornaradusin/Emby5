/* Disk-indexed Xtream and M3U VOD catalogue. SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/iptv_vod.h"
#include "app/i18n.h"
#include "app/stable_identity.h"
#include "app/io_worker.h"
#include "app/request_budget.h"
#include "app/xtream_fetch.h"
#include <stdexcept>
#include <memory>
#include "app/iptv_m3u.h"
#include "jf/jf_http.h"
#include "evo_data_path.h"
extern "C" {
#include "cJSON.h"
}
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <thread>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
namespace iptv_vod {
namespace {
struct CachePath {std::string path;};
struct Location {uint64_t offset;std::shared_ptr<CachePath> file;};
struct Ref {uint64_t offset;std::string name,id,category;std::shared_ptr<CachePath> file;};
struct Index {
    std::unordered_map<std::string,Location> by_id;
    std::vector<std::string> order;
    std::unordered_map<std::string,std::vector<Ref>> by_group;
    size_t count=0;std::string path;iptv_catalogue::Scan scan;
    std::shared_ptr<CachePath> file;
};
std::mutex mu;
Snapshot status;
std::shared_ptr<Index> movies=std::make_shared<Index>(),shows=std::make_shared<Index>();
std::shared_ptr<const Index> preview_movies,preview_shows;
std::shared_ptr<const Index> visible_index(bool series) { // mu held
    const auto &preview=series?preview_shows:preview_movies;
    return preview?preview:(series?shows:movies);
}
std::vector<Item> m3u_movies;
std::string identity;
bool refreshing=false;
std::chrono::steady_clock::time_point last{};
int interval_hours=-1;
uint64_t epoch=0;
bool cache_loaded=false;
std::string lower(std::string s) { for(char &c:s)c=(char)std::tolower((unsigned char)c);return s; }
std::string value(const cJSON *j,const char *name) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,name);
    if(cJSON_IsString(v) && v->valuestring)return v->valuestring;
    if(cJSON_IsNumber(v))return std::to_string(v->valueint);
    return {};
}
std::string encode(const std::string &s) {
    std::string out; static const char *hex="0123456789ABCDEF";
    for(unsigned char c:s)if(std::isalnum(c)||c=='-'||c=='_'||c=='.')out+=(char)c;
    else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
    return out;
}
std::string root(std::string s) {while(!s.empty()&&s.back()=='/')s.pop_back();return s;}
std::string movie_url(const iptv_xtream::Credentials &c,const std::string &id,const std::string &ext) {
    return root(c.server)+"/movie/"+encode(c.username)+"/"+encode(c.password)+"/"+encode(id)+"."+(ext.empty()?"mp4":ext);
}
std::string episode_url(const iptv_xtream::Credentials &c,const std::string &id,const std::string &ext) {
    return root(c.server)+"/series/"+encode(c.username)+"/"+encode(c.password)+"/"+encode(id)+"."+(ext.empty()?"mp4":ext);
}
std::string cache_file(const std::string &key) {
    uint64_t h=1469598103934665603ULL;
    for(unsigned char c:key)h=(h^c)*1099511628211ULL;
    char name[64];std::snprintf(name,sizeof name,"emby5/vod-catalog-%016llx.ndjson",(unsigned long long)h);
    return evo_data_path(name);
}
std::string section_file(const std::string &key,bool series) {
    const auto base=cache_file(key);
    return base.substr(0,base.size()-7)+(series?"-series.ndjson":"-movies.ndjson");
}
int interval_impl() {
    if(interval_hours>=0)return interval_hours;
    interval_hours=24;
    FILE *f=std::fopen(evo_data_path("emby5/vod-refresh-hours.txt"),"rb");
    if(f){int v=24;if(std::fscanf(f,"%d",&v)==1&&(v==0||v==12||v==24||v==48||v==168))interval_hours=v;std::fclose(f);}
    return interval_hours;
}
bool json(const std::string &url,cJSON **out) {
    *out=nullptr;
    const auto r=jf::http_request("GET",url,{},"",25);
    if(!r.ok()||r.body.size()>(32u<<20))return false;
    *out=cJSON_Parse(r.body.c_str());return *out!=nullptr;
}
// Each NDJSON record is independently readable. New files end with refresh
// metadata, in the SAME atomic rename as the data and its newly built index.
bool write_json_line(FILE *f,const cJSON *obj) {
    char *line=cJSON_PrintUnformatted(obj);if(!line)return false;
    const bool ok=std::fputs(line,f)>=0 && std::fputc('\n',f)!=EOF;
    std::free(line);return ok;
}
bool write_item(FILE *f,const Item &v,Index &index) {
    if(!f||v.id.empty()||v.name.empty()||v.source=="M3U")return false;
    iptv_catalogue::Json obj(cJSON_CreateObject(),cJSON_Delete);if(!obj)return false;
    cJSON_AddStringToObject(obj.get(),"id",v.id.c_str());cJSON_AddStringToObject(obj.get(),"name",v.name.c_str());
    cJSON_AddStringToObject(obj.get(),"poster",v.poster.c_str());cJSON_AddStringToObject(obj.get(),"group",v.group.c_str());
    cJSON_AddStringToObject(obj.get(),"categoryId",v.category_id.c_str());
    cJSON_AddStringToObject(obj.get(),"extension",v.extension.c_str());cJSON_AddStringToObject(obj.get(),"source",v.source.c_str());
    cJSON_AddBoolToObject(obj.get(),"series",v.series);
    const long pos=std::ftell(f);
    const bool ok=pos>=0&&write_json_line(f,obj.get());
    if(ok) {
        auto &refs=index.by_group[v.group];if(refs.empty())index.order.push_back(v.group);
        refs.push_back({(uint64_t)pos,lower(v.name),v.id,v.category_id,index.file});index.by_id[v.id]={(uint64_t)pos,index.file};++index.count;
    }
    return ok;
}
struct Record { iptv_catalogue::Json json{nullptr,cJSON_Delete};bool eof=false; };
Record read_record(FILE *f) {
    std::string line;char part[4096];
    while(std::fgets(part,sizeof part,f)) {
        line+=part;
        if(line.size()>(4u<<20))return {};
        if(!line.empty()&&line.back()=='\n')break;
    }
    if(line.empty()) {Record r;r.eof=std::feof(f)!=0;return r;}
    Record r;r.json.reset(cJSON_ParseWithOpts(line.c_str(),nullptr,1));return r;
}
bool parse_item(const cJSON *obj,Item &v) {
    v.id=value(obj,"id");v.name=value(obj,"name");v.poster=value(obj,"poster");v.group=value(obj,"group");
    v.extension=value(obj,"extension");v.source=value(obj,"source");v.category_id=value(obj,"categoryId");
    v.series=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(obj,"series"));
    return !v.id.empty()&&!v.name.empty();
}
bool read_item(FILE *f,Item &v) {auto r=read_record(f);return r.json&&parse_item(r.json.get(),v);}
void add_ref(Index &index,const Item &v,long pos) {
    auto &refs=index.by_group[v.group];if(refs.empty())index.order.push_back(v.group);
    refs.push_back({(uint64_t)pos,lower(v.name),v.id,v.category_id,index.file});index.by_id[v.id]={(uint64_t)pos,index.file};++index.count;
}
bool load_cache(const std::string &path,Index &index,bool series) {
    std::unique_ptr<FILE,decltype(&std::fclose)> f(std::fopen(path.c_str(),"rb"),&std::fclose);if(!f)return false;
    Index fresh;fresh.path=path;fresh.file=std::make_shared<CachePath>(CachePath{path});bool metadata=false;
    for(;;) {
        const long pos=std::ftell(f.get());if(pos<0)return false;
        auto r=read_record(f.get());if(r.eof)break;if(!r.json)return false;
        if(const auto *scan=cJSON_GetObjectItemCaseSensitive(r.json.get(),"_emby5_catalogue")) {
            fresh.scan=iptv_catalogue::read_scan(scan);metadata=true;continue;
        }
        Item v;if(metadata||!parse_item(r.json.get(),v)||v.series!=series||v.source!="Xtream")return false;
        add_ref(fresh,v,pos);
    }
    if(!metadata)return false;
    index=std::move(fresh);return true;
}
void load_legacy(const std::string &path,Index &m,Index &s) {
    std::unique_ptr<FILE,decltype(&std::fclose)> f(std::fopen(path.c_str(),"rb"),&std::fclose);if(!f)return;
    m.path=s.path=path;m.file=s.file=std::make_shared<CachePath>(CachePath{path});
    for(;;) {
        const long pos=std::ftell(f.get());if(pos<0)break;auto r=read_record(f.get());
        if(r.eof)break;
        Item v;if(!r.json||!parse_item(r.json.get(),v)){m=Index{};s=Index{};return;}
        if(v.source=="Xtream")add_ref(v.series?s:m,v,pos);
    }
    m.scan.error=s.scan.error=N_("Cached catalogue requires refresh");
}
void update_status(bool changed=true) { // caller holds mu
    status.m3u_count=m3u_movies.size();
    status.movie_count=visible_index(false)->count+status.m3u_count;status.show_count=visible_index(true)->count;
    status.loading=refreshing;
    status.categories_done=status.movies.done+status.shows.done;
    status.categories_total=status.movies.total+status.shows.total;
    status.error.clear();
    for(const auto &pair:{std::make_pair(N_("Movies"),&status.movies),std::make_pair(N_("TV shows"),&status.shows),std::make_pair("M3U",&status.playlist)}) {
        if(pair.second->configured&&!pair.second->error.empty()) {
            if(!status.error.empty())status.error+="; ";
            status.error+=std::string(T(pair.first))+": "+i18n::message(pair.second->error);
        }
    }
    if(changed)++status.generation;
}
std::string ext_of(const std::string &url) {
    const size_t end=url.find('?'),dot=url.rfind('.',end);
    if(dot==std::string::npos)return {};
    return lower(url.substr(dot+1,(end==std::string::npos?url.size():end)-dot-1));
}
}
Snapshot snapshot() {std::lock_guard<std::mutex> g(mu);return status;}
std::vector<Group> groups(bool series,const std::string &search) {
    std::lock_guard<std::mutex> g(mu);
    const auto owned=visible_index(series);const Index &idx=*owned;std::vector<Group> result;std::string needle=lower(search);
    for(const auto &name:idx.order) {
        auto it=idx.by_group.find(name);if(it==idx.by_group.end())continue;
        size_t count=0;
        if(needle.empty())count=it->second.size();
        else for(const auto &ref:it->second)if(ref.name.find(needle)!=std::string::npos)++count;
        if(count)result.push_back({name,count});
    }
    if(!series)for(const auto &v:m3u_movies){
        if(!needle.empty()&&lower(v.name).find(needle)==std::string::npos)continue;
        auto it=std::find_if(result.begin(),result.end(),[&](const Group &g){return g.name==v.group;});
        if(it==result.end())result.push_back({v.group,1});else ++it->count;
    }
    return result;
}
std::vector<Item> page(bool series,const std::string &group,size_t first,size_t limit,const std::string &search) {
    std::vector<Item> result,playlist;std::vector<Location> selected;
    std::unordered_map<const CachePath*,std::shared_ptr<FILE>> handles;
    if(limit==0||limit>32)limit=32;
    const auto needle=lower(search);size_t match=0;
    {
        std::lock_guard<std::mutex> lock(mu);
        const auto index=visible_index(series);const auto found=index->by_group.find(group);
        if(found!=index->by_group.end())for(const auto &ref:found->second) {
            if(!needle.empty()&&ref.name.find(needle)==std::string::npos)continue;
            if(match++<first)continue;
            selected.push_back({ref.offset,ref.file});
            if(!handles.count(ref.file.get())) {
                FILE *file=ref.file?std::fopen(ref.file->path.c_str(),"rb"):nullptr;
                handles.emplace(ref.file.get(),std::shared_ptr<FILE>(file,[](FILE *p){if(p)std::fclose(p);}));
            }
            if(selected.size()>=limit)break;
        }
        if(!series&&selected.size()<limit)for(const auto &v:m3u_movies) {
            if(v.group!=group||(!needle.empty()&&lower(v.name).find(needle)==std::string::npos))continue;
            if(match++<first)continue;
            auto copy=v;copy.id=emby5::playlist_item_key(v.url);playlist.push_back(std::move(copy));
            if(selected.size()+playlist.size()>=limit)break;
        }
    }
    // File handles bind offsets to the same inode before a refresh can rename
    // a cache. Reads/parsing never hold the shared catalogue/status mutex.
    iptv_xtream::Credentials creds;const bool has=limit==1&&iptv_xtream::load_credentials(&creds);
    for(const auto &ref:selected) {
        auto file=handles[ref.file.get()];Item item;
        if(file&&std::fseek(file.get(),(long)ref.offset,SEEK_SET)==0&&read_item(file.get(),item)) {
            if(!item.series&&item.source=="Xtream"&&has)item.url=movie_url(creds,item.id,item.extension);
            result.push_back(std::move(item));
        }
    }
    result.insert(result.end(),std::make_move_iterator(playlist.begin()),std::make_move_iterator(playlist.end()));
    return result;
}
int refresh_interval_hours(){std::lock_guard<std::mutex> g(mu);return interval_impl();}
void set_refresh_interval_hours(int hours){
    if(hours!=0&&hours!=12&&hours!=24&&hours!=48&&hours!=168)return;
    std::lock_guard<std::mutex> g(mu);interval_hours=hours;
    evo_mkdir(evo_data_dir());evo_mkdir(evo_data_path("emby5"));
    FILE *f=std::fopen(evo_data_path("emby5/vod-refresh-hours.txt"),"wb");
    if(f){std::fprintf(f,"%d\n",hours);std::fclose(f);}
}
void refresh(bool force,bool retry_failed){
    if(iptv_catalogue::playback_active())return;
    iptv_xtream::Credentials c;const bool xtream=iptv_xtream::load_credentials(&c);
    const std::string m3u=iptv_m3u::playlist_url();
    const std::string key=(xtream?c.server+"|"+c.username+"|"+c.password:"")+"|"+m3u;
    uint64_t ticket;bool need_cache;
    {
        std::lock_guard<std::mutex> g(mu);
        if(identity!=key){
            identity=key;movies=std::make_shared<Index>();shows=std::make_shared<Index>();preview_movies.reset();preview_shows.reset();m3u_movies.clear();
            const auto generation=status.generation+1;
            status=Snapshot{};status.generation=generation;++epoch;last={};cache_loaded=false;refreshing=false;
        }
        if(refreshing)return;
        const bool resume_paused=status.movies.error=="Refresh paused during playback"||status.shows.error=="Refresh paused during playback";
        if(!force&&!resume_paused&&last.time_since_epoch().count()) {
            // Partial catalogues retry the missing sections after a short cooldown,
            // not a full download every Settings/Home frame or a 24-hour wait.
            const auto wait=status.error.empty()?std::chrono::seconds(300):std::chrono::seconds(60);
            if(std::chrono::steady_clock::now()-last<wait)return;
            if(interval_impl()==0&&cache_loaded)return;
        }
        refreshing=true;status.loading=true;
        status.movies.configured=xtream;status.shows.configured=xtream;status.playlist.configured=!m3u.empty();
        status.movies.loading=xtream;status.shows.loading=xtream;status.playlist.loading=!m3u.empty();
        ticket=++epoch;need_cache=!cache_loaded;++status.generation;
    }
    auto fail=[ticket] {
        std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
        for(auto *s:{&status.movies,&status.shows,&status.playlist})if(s->loading) {
            s->loading=false;s->error=N_("Catalogue worker failed");s->cached=true;
        }
        preview_movies.reset();preview_shows.reset();
        refreshing=false;last=std::chrono::steady_clock::now();update_status();
    };
    const bool started=emby5::spawn_io([key,c,xtream,m3u,force,retry_failed,ticket,need_cache] {
        auto cancelled=[ticket] {std::lock_guard<std::mutex> g(mu);return epoch!=ticket;};
        if(need_cache) {
            auto cm=std::make_shared<Index>(),cs=std::make_shared<Index>();
            const bool have_m=load_cache(section_file(key,false),*cm,false);
            const bool have_s=load_cache(section_file(key,true),*cs,true);
            // Read the existing v1.2.3/v1.2.4 mixed cache without changing it.
            if(!have_m||!have_s) {
                Index lm,ls;load_legacy(cache_file(key),lm,ls);
                if(!have_m)*cm=std::move(lm);
                if(!have_s)*cs=std::move(ls);
            }
            std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
            movies=std::move(cm);shows=std::move(cs);cache_loaded=true;
            if(xtream) {
                status.movies=iptv_catalogue::result_state(movies->scan,true);status.movies.loading=true;
                status.shows=iptv_catalogue::result_state(shows->scan,true);status.shows.loading=true;
            }
            update_status();
        }
        if(cancelled())return;
        bool targeted_retry=false;
        {
            std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
            targeted_retry=retry_failed&&(!status.movies.error.empty()||!status.shows.error.empty()||!status.playlist.error.empty());
        }
        // Use independent workers/files/deadlines. A failed/slow series API cannot
        // discard valid movies, hold up Live TV, or invalidate the other index.
        auto section=[&](bool series) {
            try {
                std::shared_ptr<const Index> old;int hours;bool skip=false,failed_before=false;
                {
                    std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                    old=series?shows:movies;hours=interval_impl();
                    const auto &state=series?status.shows:status.movies;
                    failed_before=!state.error.empty();
                    skip=targeted_retry&&state.error.empty()&&old->scan.complete;
                }
                struct stat st{};
                if(!force && !old->path.empty() && stat(old->path.c_str(),&st)==0) {
                    const auto now=std::time(nullptr);
                    skip=(hours==0 || (old->scan.complete&&!failed_before&&now>=st.st_mtime&&now-st.st_mtime<(time_t)hours*3600));
                }
                if(skip) {
                    std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                    auto &s=series?status.shows:status.movies;s.loading=false;s.cached=true;
                    if(s.error=="Refresh paused during playback")s.error.clear();
                    update_status();return;
                }
                emby5::RequestBudget budget(300,cancelled);
                evo_mkdir(evo_data_dir());evo_mkdir(evo_data_path("emby5"));
                const auto path=section_file(key,series),tmp=path+".next-"+std::to_string(ticket);
                struct TempFile {
                    std::string path;FILE *f;
                    ~TempFile(){if(f)std::fclose(f);std::remove(path.c_str());}
                } file{tmp,std::fopen(tmp.c_str(),"wb")};
                if(!file.f)throw std::runtime_error("cache write");
                auto next=std::make_shared<Index>();next->path=tmp;next->file=std::make_shared<CachePath>(CachePath{tmp});
                size_t published=0;auto published_at=std::chrono::steady_clock::time_point{};
                auto publish_preview=[&](const iptv_catalogue::Scan &scan) {
                    if(cancelled()||next->count==published)return;
                    const auto now=std::chrono::steady_clock::now();
                    if(published && next->count-published<8192 &&
                       (next->count-published<512||now-published_at<std::chrono::seconds(2)))return;
                    if(std::fflush(file.f)!=0)throw std::runtime_error("cache flush");
                    // The preview owns an immutable index over flushed records;
                    // the worker continues writing only to its private index.
                    auto view=std::make_shared<Index>(*next);
                    for(const auto &group:old->order) {
                        const auto found=old->by_group.find(group);if(found==old->by_group.end())continue;
                        for(const auto &ref:found->second) {
                            if(view->by_id.count(ref.id)||scan.completed.count(ref.category))continue;
                            auto &refs=view->by_group[group];if(refs.empty())view->order.push_back(group);
                            refs.push_back(ref);view->by_id[ref.id]={ref.offset,ref.file};++view->count;
                        }
                    }
                    {std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                        (series?preview_shows:preview_movies)=std::move(view);update_status();}
                    published=next->count;published_at=now;
                };
                std::unordered_set<std::string> seen;
                auto collect=[&](const cJSON *array,const std::string &category,const std::vector<iptv_catalogue::Category> &categories) {
                    std::unordered_map<std::string,std::string> labels;
                    for(const auto &cat:categories)labels[cat.id]=cat.name;
                    const cJSON *v;
                    cJSON_ArrayForEach(v,array) {
                        Item item;item.series=series;item.id=value(v,series?"series_id":"stream_id");
                        item.name=value(v,"name");item.poster=value(v,series?"cover":"stream_icon");
                        item.extension=value(v,"container_extension");item.category_id=value(v,"category_id");
                        if(item.category_id.empty())item.category_id=category;
                        item.group=labels[item.category_id];if(item.group.empty())item.group=series?"TV Shows":"Movies";
                        item.source="Xtream";
                        if(seen.insert(item.id).second&&!write_item(file.f,item,*next))throw std::runtime_error("cache write");
                    }
                };
                auto progress=[&](const iptv_catalogue::Scan &scan) {
                    publish_preview(scan);
                    std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                    auto &s=series?status.shows:status.movies;
                    s.done=scan.done;s.total=scan.total;s.failed=scan.failed.size();update_status(false);
                };
                const bool retry=retry_failed||(!force&&!old->scan.complete);
                next->scan=iptv_catalogue::fetch(c,series?"get_series_categories":"get_vod_categories",
                    series?"get_series":"get_vod_streams",series?"series_id":"stream_id",budget,collect,false,
                    retry?&old->scan:nullptr,progress);
                if(cancelled())return;
                if(!next->scan.changed&&!next->scan.complete) {
                    // Nothing new was accepted: do not read/rewrite hundreds of
                    // thousands of cached records or change their freshness.
                    std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                    (series?preview_shows:preview_movies).reset();
                    auto &state=series?status.shows:status.movies;
                    state=iptv_catalogue::result_state(next->scan,old->count!=0);
                    update_status();return;
                }
                bool retained=false;
                if(!next->scan.whole&&(!next->scan.complete||next->scan.incremental)&&!old->path.empty()) {
                    std::unique_ptr<FILE,decltype(&std::fclose)> f(std::fopen(old->path.c_str(),"rb"),&std::fclose);
                    if(!f&&old->count)throw std::runtime_error("cache read");
                    if(f)for(;;) {
                        const auto record=read_record(f.get());
                        if(record.eof)break;
                        if(!record.json)throw std::runtime_error("cache read");
                        if(cJSON_GetObjectItemCaseSensitive(record.json.get(),"_emby5_catalogue"))continue;
                        Item item;if(!parse_item(record.json.get(),item))throw std::runtime_error("cache read");
                        if(item.series!=series||item.source!="Xtream"||seen.count(item.id))continue;
                        // Stable IDs are stored in new caches. Legacy label-only
                        // rows are replaced on full success; a partial refresh
                        // conservatively keeps them rather than guessing identity.
                        if(!item.category_id.empty()&&next->scan.completed.count(item.category_id))continue;
                        if(next->scan.complete&&(item.category_id.empty()||std::none_of(next->scan.categories.begin(),next->scan.categories.end(),[&](const auto &cat){return cat.id==item.category_id;})))continue;
                        if(cancelled())return;
                        if(!write_item(file.f,item,*next))throw std::runtime_error("cache write");
                        seen.insert(item.id);retained=true;
                    }
                }
                auto metadata=iptv_catalogue::Json(cJSON_CreateObject(),cJSON_Delete);
                if(!metadata)throw std::runtime_error("cache metadata");
                cJSON_AddItemToObject(metadata.get(),"_emby5_catalogue",iptv_catalogue::scan_json(next->scan));
                if(!write_json_line(file.f,metadata.get()))throw std::runtime_error("cache metadata");
                bool saved=std::fflush(file.f)==0;saved=(std::fclose(file.f)==0)&&saved;file.f=nullptr;
                if(!saved)throw std::runtime_error("cache flush");
                std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                if(std::rename(tmp.c_str(),path.c_str())!=0)throw std::runtime_error("cache publish");
                next->path=path;next->file->path=path;
                (series?preview_shows:preview_movies).reset();
                (series?shows:movies)=std::move(next);
                auto &s=series?status.shows:status.movies;
                s=iptv_catalogue::result_state((series?shows:movies)->scan,retained);
                update_status();
            } catch(...) {
                std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                auto &s=series?status.shows:status.movies;
                s.loading=false;s.cached=(series?shows:movies)->count!=0;
                (series?preview_shows:preview_movies).reset();
                s.error=N_("Catalogue cache/worker failed; previous titles kept");update_status();
            }
        };
        auto playlist=[&] {
            try {
                iptv_xtream::Catalog cat;std::string error;std::vector<Item> discovered;
                const bool ok=iptv_m3u::load_catalog(m3u,&cat,&error);
                if(ok)for(const auto &ch:cat.channels) {
                    const auto ext=ext_of(ch.extension);
                    if(ext!="mp4"&&ext!="mkv"&&ext!="avi"&&ext!="mov"&&ext!="webm")continue;
                    Item v;v.name=ch.name;v.id=ch.id;v.url=ch.extension;v.poster=ch.logo;
                    v.group=ch.category_id;v.extension=ext;v.source="M3U";discovered.push_back(std::move(v));
                }
                std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                if(ok)m3u_movies=std::move(discovered);
                status.playlist.loading=false;status.playlist.cached=!ok&&!m3u_movies.empty();
                status.playlist.error=ok?"":N_("M3U refresh failed");update_status();
            } catch(...) {
                std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
                status.playlist.loading=false;status.playlist.error=N_("M3U refresh failed");update_status();
            }
        };
        std::vector<std::function<void()>> jobs;
        if(xtream){jobs.push_back([&]{section(false);});jobs.push_back([&]{section(true);});}
        if(!m3u.empty())jobs.push_back(playlist);
        emby5::run_io_jobs(jobs);
        std::lock_guard<std::mutex> g(mu);if(epoch!=ticket)return;
        refreshing=false;status.movies.loading=status.shows.loading=status.playlist.loading=false;
        last=std::chrono::steady_clock::now();update_status();
    },fail);
    if(!started)fail();
}
std::vector<Episode> episodes(const Item &series,std::string *error){
    if(error)error->clear();
    std::vector<Episode> result;if(!series.series||series.source!="Xtream")return result;
    iptv_xtream::Credentials c;if(!iptv_xtream::load_credentials(&c))return result;
    cJSON *doc=nullptr;
    if(!json(iptv_xtream::api_url(c,"get_series_info")+"&series_id="+encode(series.id),&doc)){
        if(error)*error=T("Could not load episodes. Press Circle to return and retry.");return result;}
    const cJSON *seasons=cJSON_GetObjectItemCaseSensitive(doc,"episodes");
    if(cJSON_IsObject(seasons))for(const cJSON *season=seasons->child;season;season=season->next){
        const int n=season->string?std::atoi(season->string):0;const cJSON *ep;
        cJSON_ArrayForEach(ep,season){Episode e;e.id=value(ep,"id");e.name=value(ep,"title");
            e.extension=value(ep,"container_extension");e.season=n;
            const auto num=value(ep,"episode_num");e.number=num.empty()?0:std::atoi(num.c_str());
            if(e.name.empty())e.name=TF("Episode {number}",{{"number",std::to_string(e.number)}});
            if(!e.id.empty())e.url=episode_url(c,e.id,e.extension);
            if(!e.url.empty()&&result.size()<3000)result.push_back(std::move(e));
        }
    }
    cJSON_Delete(doc);
    std::sort(result.begin(),result.end(),[](const Episode &a,const Episode &b){return a.season!=b.season?a.season<b.season:a.number<b.number;});
    return result;
}
#include "iptv_vod_view.inc"

}
