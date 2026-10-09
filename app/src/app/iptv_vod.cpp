/* Disk-indexed Xtream and M3U VOD catalogue. SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/iptv_vod.h"
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
struct Ref { uint64_t offset; std::string name; };
struct Index { std::vector<std::string> order; std::unordered_map<std::string,std::vector<Ref>> by_group; size_t count=0; };
std::mutex mu;
Snapshot status;
Index movies,shows;
std::vector<Item> m3u_movies;
std::string identity,filepath;
std::chrono::steady_clock::time_point last{};
int interval_hours=-1;
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
// Each line is an independent JSON object; no monolithic JSON parse or file-size ceiling.
bool write_item(FILE *f,const Item &v,Index &m,Index &s) {
    if(!f||v.id.empty()||v.name.empty())return false;
    cJSON *obj=cJSON_CreateObject();if(!obj)return false;
    cJSON_AddStringToObject(obj,"id",v.id.c_str());cJSON_AddStringToObject(obj,"name",v.name.c_str());
    cJSON_AddStringToObject(obj,"poster",v.poster.c_str());cJSON_AddStringToObject(obj,"group",v.group.c_str());
    cJSON_AddStringToObject(obj,"extension",v.extension.c_str());cJSON_AddStringToObject(obj,"source",v.source.c_str());
    cJSON_AddBoolToObject(obj,"series",v.series);
    // M3U URLs can contain credentials/tokens. Do not persist them.
    if(v.source=="M3U") {cJSON_Delete(obj);return false;}
    char *line=cJSON_PrintUnformatted(obj);cJSON_Delete(obj);if(!line)return false;
    const long pos=std::ftell(f);
    bool ok=pos>=0 && std::fputs(line,f)>=0 && std::fputc('\n',f)!=EOF;
    std::free(line);
    if(ok){auto &index=v.series?s:m;auto &refs=index.by_group[v.group];
        if(refs.empty())index.order.push_back(v.group);
        refs.push_back({(uint64_t)pos,lower(v.name)});++index.count;}
    return ok;
}
bool read_item(FILE *f,Item &v) {
    std::string line;char part[4096];
    while(std::fgets(part,sizeof part,f)){line+=part;if(!line.empty()&&line.back()=='\n')break;if(line.size()>1048576)return false;}
    if(line.empty())return false;
    cJSON *obj=cJSON_Parse(line.c_str());if(!obj)return false;
    v.id=value(obj,"id");v.name=value(obj,"name");v.poster=value(obj,"poster");v.group=value(obj,"group");
    v.extension=value(obj,"extension");v.source=value(obj,"source");
    v.series=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(obj,"series"));
    cJSON_Delete(obj);return !v.id.empty()&&!v.name.empty();
}
void load_cache(const std::string &path,Index &m,Index &s) {
    FILE *f=std::fopen(path.c_str(),"rb");if(!f)return;
    for(;;){const long pos=std::ftell(f);if(pos<0)break;Item v;
        if(!read_item(f,v))break;
        auto &index=v.series?s:m;auto &refs=index.by_group[v.group];
        if(refs.empty())index.order.push_back(v.group);
        refs.push_back({(uint64_t)pos,lower(v.name)});++index.count;
    }
    std::fclose(f);
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
    const Index &idx=series?shows:movies;std::vector<Group> result;std::string needle=lower(search);
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
    std::lock_guard<std::mutex> g(mu);
    std::vector<Item> result;if(limit==0||limit>32)limit=32;
    const Index &idx=series?shows:movies;
    auto it=idx.by_group.find(group);
    FILE *f=std::fopen(filepath.c_str(),"rb");
    const auto needle=lower(search);size_t match=0;
    iptv_xtream::Credentials creds;const bool has_creds=limit==1&&iptv_xtream::load_credentials(&creds);
    if(it!=idx.by_group.end())for(const auto &ref:it->second) {
        if(!needle.empty()&&ref.name.find(needle)==std::string::npos)continue;
        if(match++<first)continue;
        if(!f||std::fseek(f,(long)ref.offset,SEEK_SET)!=0)break;
        Item v;if(read_item(f,v)) {
            if(!v.series&&v.source=="Xtream"&&has_creds)v.url=movie_url(creds,v.id,v.extension);
            result.push_back(std::move(v));
        }
        if(result.size()>=limit)break;
    }
    if(f)std::fclose(f);
    if(!series&&result.size()<limit)for(const auto &v:m3u_movies){
        if(v.group!=group||(!needle.empty()&&lower(v.name).find(needle)==std::string::npos))continue;
        if(match++<first)continue;
        result.push_back(v);if(result.size()>=limit)break;
    }
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
void refresh(bool force){
    iptv_xtream::Credentials c;const bool xtream=iptv_xtream::load_credentials(&c);
    const std::string m3u=iptv_m3u::playlist_url();
    const std::string key=(xtream?c.server+"|"+c.username+"|"+c.password:"")+"|"+m3u;
    {
        std::lock_guard<std::mutex> g(mu);
        if(identity!=key){identity=key;filepath=cache_file(key);movies=Index{};shows=Index{};m3u_movies.clear();
            // Index loading happens off the UI thread below; do not deserialize 223k titles on Home.
            status=Snapshot{};++status.generation;last={};
        }
        if(status.loading)return;
        if(!force){const int hours=interval_impl();
            if(hours==0&&(status.movie_count||status.show_count))return;
            if(last.time_since_epoch().count()&&std::chrono::steady_clock::now()-last<std::chrono::minutes(5))return;
        }
        status.loading=true;last=std::chrono::steady_clock::now();++status.generation;
    }
    std::thread([key,c,xtream,m3u,force] {
        const std::string path=cache_file(key);
        Index cached_m,cached_s;load_cache(path,cached_m,cached_s);
        {
            std::lock_guard<std::mutex> g(mu);
            if(identity!=key)return;
            movies=std::move(cached_m);shows=std::move(cached_s);
            status.movie_count=movies.count;status.show_count=shows.count;++status.generation;
        }
        struct stat st{};bool fresh=false;
        if(!force&&stat(path.c_str(),&st)==0){const time_t now=std::time(nullptr);
            int hours=24;{std::lock_guard<std::mutex> g(mu);hours=interval_impl();}
            fresh=(hours==0||(now>=st.st_mtime&&now-st.st_mtime<(time_t)hours*3600));
        }
        if(!m3u.empty()){
            iptv_xtream::Catalog catalog;std::string err;std::vector<Item> discovered;
            if(iptv_m3u::load_catalog(m3u,&catalog,&err))for(const auto &ch:catalog.channels){
                const auto ext=ext_of(ch.extension);
                if(ext!="mp4"&&ext!="mkv"&&ext!="avi"&&ext!="mov"&&ext!="webm")continue;
                Item v;v.name=ch.name;v.id=ch.id;v.url=ch.extension;v.poster=ch.logo;
                v.group=ch.category_id;v.extension=ext;v.source="M3U";
                discovered.push_back(std::move(v));
            }
            std::lock_guard<std::mutex> g(mu);
            if(identity==key){m3u_movies=std::move(discovered);status.movie_count=movies.count+m3u_movies.size();++status.generation;}
        }
        if(!fresh){
            evo_mkdir(evo_data_dir());evo_mkdir(evo_data_path("emby5"));
            const std::string tmp=path+".next";FILE *f=std::fopen(tmp.c_str(),"wb");
            if(f){Index nm,ns;size_t loaded=0;
                bool publishing=false;
                // Expose a flushed, readable partial catalogue while discovery runs.
                // Readers use filepath under mu, so a batch is never shown before flush.
                auto publish=[&]() {
                    if (!loaded || std::fflush(f)!=0) return;
                    std::lock_guard<std::mutex> g(mu);
                    if (identity!=key) return;
                    if (!publishing) {
                        movies=Index{};shows=Index{};
                        filepath=tmp;
                        publishing=true;
                    }
                    for (const auto &name:nm.order) {
                        auto &dst=movies.by_group[name];
                        if (dst.empty())movies.order.push_back(name);
                        auto &src=nm.by_group[name];
                        dst.insert(dst.end(),std::make_move_iterator(src.begin()),std::make_move_iterator(src.end()));
                    }
                    for (const auto &name:ns.order) {
                        auto &dst=shows.by_group[name];
                        if (dst.empty())shows.order.push_back(name);
                        auto &src=ns.by_group[name];
                        dst.insert(dst.end(),std::make_move_iterator(src.begin()),std::make_move_iterator(src.end()));
                    }
                    movies.count+=nm.count;shows.count+=ns.count;
                    nm=Index{};ns=Index{};
                    status.movie_count=movies.count+m3u_movies.size();
                    status.show_count=shows.count;
                    ++status.generation;
                };
                if(xtream)for(int kind=0;kind<2;++kind){
                    const std::string prefix=kind?"series":"vod",action=kind?"get_series":"get_vod_streams";
                    std::unordered_map<std::string,std::string> labels;std::vector<std::string> ids;
                    cJSON *cats=nullptr;
                    if(json(iptv_xtream::api_url(c,"get_"+prefix+"_categories"),&cats)){
                        const cJSON *v;cJSON_ArrayForEach(v,cats){const auto id=value(v,"category_id");
                            if(!id.empty()&&labels.emplace(id,value(v,"category_name")).second)ids.push_back(id);}
                    }cJSON_Delete(cats);
                    std::unordered_set<std::string> seen;
                    auto collect=[&](const cJSON *array){const cJSON *v;cJSON_ArrayForEach(v,array){
                        Item item;item.series=kind!=0;item.id=value(v,kind?"series_id":"stream_id");
                        item.name=value(v,"name");item.poster=value(v,kind?"cover":"stream_icon");
                        item.extension=value(v,"container_extension");item.group=labels[value(v,"category_id")];
                        if(item.group.empty())item.group=kind?"TV Shows":"Movies";
                        item.source="Xtream";
                        if(item.id.empty()||item.name.empty()||!seen.insert(item.id).second)continue;
                        if(write_item(f,item,nm,ns)) {
                            ++loaded;
                            if (loaded%256==0) publish();
                        }
                    }};
                    cJSON *all=nullptr;
                    const bool ok=json(iptv_xtream::api_url(c,action),&all)&&cJSON_IsArray(all)&&cJSON_GetArraySize(all)>0;
                    if(ok)collect(all);cJSON_Delete(all);
                    if(!ok)for(const auto &id:ids){cJSON *part=nullptr;
                        if(json(iptv_xtream::api_url(c,action)+"&category_id="+encode(id),&part)&&cJSON_IsArray(part))collect(part);
                        cJSON_Delete(part);
                    }
                }
                publish();
                // M3U VOD URLs are not stored on disk (they may embed access tokens).
                // They remain a live-source-only catalogue until a safe credential store is available.
                const bool flushed=std::fflush(f)==0;
                const bool closed=std::fclose(f)==0;const bool saved=flushed&&closed;
                if(saved&&loaded>0){
                    std::lock_guard<std::mutex> g(mu);
                    if (identity==key) {
                        // A rename cannot race an in-flight page read: page() takes mu.
                        if (std::rename(tmp.c_str(),path.c_str())==0) {
                            filepath=path;
                            ++status.generation;
                        }
                    }
                } else {
                    // An incomplete refresh must not replace the last valid catalogue.
                    Index oldm,olds;
                    load_cache(path,oldm,olds);
                    std::lock_guard<std::mutex> g(mu);
                    if(identity==key){movies=std::move(oldm);shows=std::move(olds);
                        filepath=path;status.movie_count=movies.count+m3u_movies.size();
                        status.show_count=shows.count;++status.generation;}
                    std::remove(tmp.c_str());
                }
            }
        }
        std::lock_guard<std::mutex> g(mu);
        if(identity==key){status.loading=false;++status.generation;}
    }).detach();
}
std::vector<Episode> episodes(const Item &series){
    std::vector<Episode> result;if(!series.series||series.source!="Xtream")return result;
    iptv_xtream::Credentials c;if(!iptv_xtream::load_credentials(&c))return result;
    cJSON *doc=nullptr;
    if(!json(iptv_xtream::api_url(c,"get_series_info")+"&series_id="+encode(series.id),&doc))return result;
    const cJSON *seasons=cJSON_GetObjectItemCaseSensitive(doc,"episodes");
    if(cJSON_IsObject(seasons))for(const cJSON *season=seasons->child;season;season=season->next){
        const int n=season->string?std::atoi(season->string):0;const cJSON *ep;
        cJSON_ArrayForEach(ep,season){Episode e;e.id=value(ep,"id");e.name=value(ep,"title");
            e.extension=value(ep,"container_extension");e.season=n;
            const auto num=value(ep,"episode_num");e.number=num.empty()?0:std::atoi(num.c_str());
            if(e.name.empty())e.name="Episode "+std::to_string(e.number);
            if(!e.id.empty())e.url=episode_url(c,e.id,e.extension);
            if(!e.url.empty()&&result.size()<3000)result.push_back(std::move(e));
        }
    }
    cJSON_Delete(doc);
    std::sort(result.begin(),result.end(),[](const Episode &a,const Episode &b){return a.season!=b.season?a.season<b.season:a.number<b.number;});
    return result;
}
}
