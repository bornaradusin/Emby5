/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/browse_state.h"
#include "app/stable_identity.h"
#include "app/io_worker.h"
#include "app/iptv_xtream.h"
#include "app/iptv_m3u.h"
#include "evo_data_path.h"
#include "evo_boot_trace.h"
extern "C" {
#include "cJSON.h"
}
#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <sys/stat.h>
namespace browse_state {
namespace {
std::atomic<int> user{0};
struct State {
    std::map<std::string,LibraryPrefs> libraries;
    std::vector<std::string> pins;
    std::vector<Progress> progress;
    unsigned revision=0;
    bool writing=false;
};
std::mutex mutex;
std::map<std::string,std::shared_ptr<State>> states;
std::string field(const cJSON *j,const char *key) {
    const auto *v=cJSON_GetObjectItemCaseSensitive(j,key);
    return cJSON_IsString(v)&&v->valuestring?std::string(v->valuestring).substr(0,512):std::string();
}
double number(const cJSON *j,const char *key,double def,double lo,double hi) {
    const auto *v=cJSON_GetObjectItemCaseSensitive(j,key);
    return cJSON_IsNumber(v)&&std::isfinite(v->valuedouble)?std::clamp(v->valuedouble,lo,hi):def;
}
bool yes(const cJSON *j,const char *key) {return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,key));}
bool id(const std::string &s) {return !s.empty()&&s.size()<=512&&s.find("://")==std::string::npos&&s.find_first_of("\r\n") == std::string::npos;}
std::string file(const std::string &scope) {return evo_data_path(("emby5/browse-"+opaque_key(scope)+".json").c_str());}
bool same(const Progress &a,const Progress &b) {return a.source==b.source&&a.id==b.id&&a.parent==b.parent&&a.episode==b.episode;}
std::shared_ptr<State> get(const std::string &scope) { // mutex held, one bounded local read per identity
    auto &s=states[scope];if(s)return s;s=std::make_shared<State>();
    std::string body;FILE *f=std::fopen(file(scope).c_str(),"rb");if(!f)return s;
    char b[4096];size_t n=0;while((n=std::fread(b,1,sizeof b,f))&&body.size()<(1u<<20))body.append(b,n);std::fclose(f);
    if(body.size()>=(1u<<20))return s;
    std::unique_ptr<cJSON,decltype(&cJSON_Delete)> root(cJSON_Parse(body.c_str()),cJSON_Delete);if(!root)return s;
    const cJSON *v;
    cJSON_ArrayForEach(v,cJSON_GetObjectItemCaseSensitive(root.get(),"libraries")) {
        auto k=field(v,"key");if(!id(k)||s->libraries.size()>=128)continue;
        LibraryPrefs p;p.sort=(int)number(v,"sort",0,0,8);p.decade=(int)number(v,"decade",0,0,20);
        p.unplayed=yes(v,"unplayed");p.favorites=yes(v,"favorites");p.genre=field(v,"genre");s->libraries[k]=p;
    }
    cJSON_ArrayForEach(v,cJSON_GetObjectItemCaseSensitive(root.get(),"pins")) {
        const char *x=cJSON_GetStringValue(v);if(x&&id(x)&&s->pins.size()<32)s->pins.emplace_back(x);
    }
    cJSON_ArrayForEach(v,cJSON_GetObjectItemCaseSensitive(root.get(),"progress")) {
        Progress p;p.id=field(v,"id");p.parent=field(v,"parent");p.source=field(v,"source");p.title=field(v,"title");p.episode=yes(v,"episode");
        p.position=number(v,"position",0,0,604800);p.duration=number(v,"duration",0,0,604800);
        if(id(p.id)&&(!p.episode||id(p.parent))&&(p.source=="Xtream"||p.source=="M3U")&&p.position>=10&&s->progress.size()<100)s->progress.push_back(p);
    }
    return s;
}
std::string encode(const State &s) {
    std::unique_ptr<cJSON,decltype(&cJSON_Delete)> root(cJSON_CreateObject(),cJSON_Delete);if(!root)return {};
    auto *libs=cJSON_AddArrayToObject(root.get(),"libraries");
    for(const auto &v:s.libraries){auto *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"key",v.first.c_str());cJSON_AddNumberToObject(o,"sort",v.second.sort);cJSON_AddNumberToObject(o,"decade",v.second.decade);cJSON_AddBoolToObject(o,"unplayed",v.second.unplayed);cJSON_AddBoolToObject(o,"favorites",v.second.favorites);cJSON_AddStringToObject(o,"genre",v.second.genre.c_str());cJSON_AddItemToArray(libs,o);}
    auto *pins=cJSON_AddArrayToObject(root.get(),"pins");for(const auto &v:s.pins)cJSON_AddItemToArray(pins,cJSON_CreateString(v.c_str()));
    auto *progress=cJSON_AddArrayToObject(root.get(),"progress");for(const auto &v:s.progress){auto *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"id",v.id.c_str());cJSON_AddStringToObject(o,"parent",v.parent.c_str());cJSON_AddStringToObject(o,"source",v.source.c_str());cJSON_AddStringToObject(o,"title",v.title.c_str());cJSON_AddBoolToObject(o,"episode",v.episode);cJSON_AddNumberToObject(o,"position",v.position);cJSON_AddNumberToObject(o,"duration",v.duration);cJSON_AddItemToArray(progress,o);}
    char *raw=cJSON_PrintUnformatted(root.get());std::string out=raw?raw:"";std::free(raw);return out;
}
void save(const std::string &scope,const std::shared_ptr<State> &s) { // held lock; serialized background writer
    ++s->revision;if(s->writing)return;s->writing=true;
    auto fail=[s]{std::lock_guard<std::mutex> g(mutex);s->writing=false;evo_bt("browse: preference write failed");};
    if(!emby5::spawn_io([scope,s]{for(;;){unsigned rev;std::string body;
        {std::lock_guard<std::mutex> g(mutex);rev=s->revision;body=encode(*s);}
        const std::string path=file(scope),tmp=path+".tmp";
        const std::string dir=evo_data_path("emby5");
        bool ok=!body.empty()&&evo_mkdir(evo_data_dir())==0&&evo_mkdir(dir.c_str())==0;
        FILE *f=ok?std::fopen(tmp.c_str(),"wb"):nullptr;
        if(f){::chmod(tmp.c_str(),0600);ok=std::fwrite(body.data(),1,body.size(),f)==body.size();ok=std::fclose(f)==0&&ok;if(ok)ok=std::rename(tmp.c_str(),path.c_str())==0;}else ok=false;
        if(!ok){std::remove(tmp.c_str());evo_bt("browse: preference write failed");}
        std::lock_guard<std::mutex> g(mutex);if(rev==s->revision||!ok){s->writing=false;break;}
    }},fail)) {s->writing=false;evo_bt("browse: preference worker unavailable");}
}
}
std::string opaque_key(const std::string &s){return emby5::identity_key(s);}
void set_user(int id){user=id;}
std::string scope(const jf::Client &c){return opaque_key(std::to_string(user.load())+"\n"+c.server_type()+"\n"+c.server_id()+"\n"+c.server()+"\n"+c.user_id());}
std::string provider_scope(const jf::Client &c){iptv_xtream::Credentials p;iptv_xtream::load_credentials(&p);return opaque_key(scope(c)+"\n"+p.server+"\n"+p.username+"\n"+iptv_m3u::playlist_url());}
LibraryPrefs library(const std::string &s,const std::string &k){std::lock_guard<std::mutex> g(mutex);auto state=get(s);const auto i=state->libraries.find(opaque_key(k));return i==state->libraries.end()?LibraryPrefs{}:i->second;}
void save_library(const std::string &s,const std::string &k,const LibraryPrefs &p){std::lock_guard<std::mutex> g(mutex);auto st=get(s);if(st->libraries.size()<128||st->libraries.count(opaque_key(k))){st->libraries[opaque_key(k)]=p;save(s,st);}}
std::vector<std::string> pins(const std::string &s){std::lock_guard<std::mutex> g(mutex);return get(s)->pins;}
void toggle_pin(const std::string &s,const std::string &v){if(!id(v))return;std::lock_guard<std::mutex> g(mutex);auto st=get(s);auto i=std::find(st->pins.begin(),st->pins.end(),v);if(i!=st->pins.end())st->pins.erase(i);else if(st->pins.size()<32)st->pins.push_back(v);save(s,st);}
std::vector<Progress> progress(const std::string &s){std::lock_guard<std::mutex> g(mutex);return get(s)->progress;}
double position(const std::string &s,const Progress &p){std::lock_guard<std::mutex> g(mutex);for(const auto &v:get(s)->progress)if(same(v,p))return v.position;return 0;}
void save_progress(const std::string &s,Progress p,bool ended){
    if(!id(p.id)||(!p.parent.empty()&&!id(p.parent))||(p.source!="Xtream"&&p.source!="M3U")||!std::isfinite(p.position)||!std::isfinite(p.duration))return;
    p.position=std::clamp(p.position,0.,604800.);p.duration=std::clamp(p.duration,0.,604800.);p.title=p.title.substr(0,512);
    std::lock_guard<std::mutex> g(mutex);auto st=get(s);auto &a=st->progress;
    a.erase(std::remove_if(a.begin(),a.end(),[&](const Progress &v){return same(v,p);}),a.end());
    if(!ended&&p.position>=10 && (p.duration<=0||p.position<p.duration-3))a.insert(a.begin(),p);
    if(a.size()>100)a.resize(100);save(s,st);
}
namespace {
std::mutex active_mutex;
std::string active_scope;
bool last_started=false;
Progress active_progress;
std::chrono::steady_clock::time_point last_saved{};
}
void begin_playback(const std::string &scope,const Progress &p){std::lock_guard<std::mutex> g(active_mutex);active_scope=scope;active_progress=p;last_saved=std::chrono::steady_clock::now();}
bool last_playback_started(){std::lock_guard<std::mutex> g(active_mutex);return last_started;}
void clear_playback(){std::lock_guard<std::mutex> g(active_mutex);active_scope.clear();last_started=false;}
void report_playback(double pos,double dur){
    std::lock_guard<std::mutex> g(active_mutex);if(active_scope.empty())return;
    if(std::chrono::steady_clock::now()-last_saved<std::chrono::seconds(15))return;
    active_progress.position=pos;active_progress.duration=dur;save_progress(active_scope,active_progress,false);last_saved=std::chrono::steady_clock::now();
}
void finish_playback(const char *json){
    std::lock_guard<std::mutex> g(active_mutex);
    std::unique_ptr<cJSON,decltype(&cJSON_Delete)> j(cJSON_Parse(json?json:""),cJSON_Delete);
    last_started=j&&yes(j.get(),"playbackStarted");
    if(!active_scope.empty()&&last_started){
        active_progress.position=number(j.get(),"position",0,0,604800);
        active_progress.duration=number(j.get(),"duration",0,0,604800);
        save_progress(active_scope,active_progress,field(j.get(),"state")=="ended");
    }
    active_scope.clear();
}

}
