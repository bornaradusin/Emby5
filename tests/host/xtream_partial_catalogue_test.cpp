/* Production catalogue workers/caches. No live provider or PS5 APIs. GPL-3.0-or-later */
#include "app/iptv_xtream.h"
#include "app/iptv_live.h"
#include "app/iptv_vod.h"
#include "app/iptv_m3u.h"
#include "app/catalogue_state.h"
#include "jf/jf_http.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <sys/stat.h>

static std::string root;
struct Scenario {
    bool fail_b=false,fail_series=false,empty_a=false,empty_all=false,malformed_a=false;
    bool remove_b=false,fail_all=false,throw_movies=false,whole_live=false,invalid_items=false;
    bool auth_fail=false,transport_error=false;
};
static std::mutex mock_mu;
static Scenario scenario;
static std::vector<std::string> requests;
static std::atomic<int> active{0},server_waits{0},series_waits{0};
static std::atomic<bool> block_server{false},block_series{false},with_playlist{false};
static std::string blocked_user;
static std::string url_query(const std::string &url,const std::string &name) {
    const auto p=url.find(name+"=");if(p==std::string::npos)return {};
    const auto start=p+name.size()+1,end=url.find('&',start);return url.substr(start,end-start);
}
static void set(Scenario s) {std::lock_guard<std::mutex> g(mock_mu);scenario=s;requests.clear();}
static void clear_requests() {std::lock_guard<std::mutex> g(mock_mu);requests.clear();}
static size_t hits(const std::string &text) {
    std::lock_guard<std::mutex> g(mock_mu);size_t n=0;for(const auto &r:requests)if(r.find(text)!=std::string::npos)++n;return n;
}
template<class F> static void wait(F f,const char *label) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while(!f()) {if(std::chrono::steady_clock::now()>until){std::cerr<<"Timeout: "<<label<<"\n";std::abort();}std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
extern "C" {
const char *evo_data_dir(){return root.c_str();}
const char *evo_data_path(const char *p){static thread_local std::string out;out=root+"/"+(p?p:"");return out.c_str();}
int evo_mkdir(const char *p){std::error_code e;std::filesystem::create_directories(p,e);return e?-1:0;}
}
namespace iptv_m3u {
std::string playlist_url(){return with_playlist?"http://playlist.test/channels.m3u":"";}
bool load_catalog(const std::string &,iptv_xtream::Catalog *out,std::string *) {
    out->channels.clear();
    iptv_xtream::Channel a;a.id="m3u:1";a.name="Playlist Live";a.source="M3U";a.extension="http://playlist.test/live.ts";
    out->channels.push_back(a);a.id="m3u:2";a.name="Playlist Movie";a.extension="http://playlist.test/movie.mp4";a.category_id="Playlist";
    out->channels.push_back(a);return true;
}
}
namespace jf {
HttpResponse http_request(const std::string &,const std::string &url,const std::vector<std::string>&,const std::string &,int seconds) {
    assert(seconds>0&&seconds<=30);++active;struct Done{~Done(){--active;}}done;
    Scenario s;std::string blocked;
    {std::lock_guard<std::mutex> g(mock_mu);s=scenario;blocked=blocked_user;requests.push_back(url);}
    HttpResponse r;r.status=200;
    if(url.find("/LiveTv/Channels?")!=std::string::npos) {
        if(block_server){++server_waits;wait([]{return !block_server;},"release server request");}
        r.body=R"({"Items":[{"Id":"server-one","Name":"Server channel"}]})";return r;
    }
    if(url.find("/LiveTv/Programs?")!=std::string::npos){r.body=R"({"Items":[]})";return r;}
    assert(url.rfind("http://provider.test/player_api.php?",0)==0);
    const auto action=url_query(url,"action"),cat=url_query(url,"category_id"),user=url_query(url,"username");
    if(action.empty()){r.body=s.auth_fail?R"({"user_info":{"auth":0}})":R"({"user_info":{"auth":1,"status":"Active"}})";return r;}
    const bool series=action.find("series")!=std::string::npos;
    if(series && action=="get_series" && block_series && (blocked.empty()||blocked==user)) {
        ++series_waits;wait([]{return !block_series;},"release series request");
    }
    if(s.auth_fail){r.status=401;return r;}
    if(s.fail_all||(s.fail_series&&series)){r.status=503;return r;}
    if(s.transport_error){r.status=200;r.error="provider.test SECRET-TOKEN";r.body="[]";return r;}
    if(s.throw_movies&&action.find("vod")!=std::string::npos)throw std::runtime_error("SECRET-TOKEN provider URL");
    if(action.find("categories")!=std::string::npos) {
        if(s.empty_all){r.body="[]";return r;}
        if(series){r.body=R"([{"category_id":"s","category_name":"Series"}])";return r;}
        r.body=s.remove_b?R"([{"category_id":"a","category_name":"Alpha"},{"category_id":"c","category_name":"Gamma"}])":R"([{"category_id":"a","category_name":"Alpha"},{"category_id":"b","category_name":"Beta"},{"category_id":"c","category_name":"Gamma"}])";
        return r;
    }
    if(s.empty_all){r.body="[]";return r;}
    if(action=="get_live_streams"&&cat.empty()) {
        if(s.whole_live){r.body=R"([{"stream_id":"whole","name":"Whole channel","category_id":"z"}])";return r;}
        r.status=0;r.error="Response too large";return r;
    }
    if(s.fail_b&&cat=="b"){r.status=503;return r;}
    if(s.empty_a&&cat=="a"){r.body="[]";return r;}
    if(s.malformed_a&&cat=="a"){r.body="[{\"stream_id\":";return r;}
    if(s.invalid_items&&cat=="a"){r.body=R"([{"stream_id":"bad","name":""}])";return r;}
    const auto group=series?"s":cat;
    const auto prefix=series?"show":action=="get_live_streams"?"live":"movie";
    const auto id=user+"-"+prefix+"-"+group;
    r.body="[{\""+std::string(series?"series_id":"stream_id")+"\":\""+id+"\",\"name\":\""+id+"\",\"category_id\":\""+group+"\",\"container_extension\":\"mp4\"}]";
    return r;
}
}
static iptv_xtream::Credentials creds(std::string user) {return {"http://provider.test",std::move(user),"secret"};}
static void account(const std::string &user){assert(iptv_xtream::save_credentials(creds(user)));}
static std::set<std::string> vod_ids(bool series) {
    std::set<std::string> ids;
    for(const auto &g:iptv_vod::groups(series))for(size_t i=0;i<g.count;i+=32)for(const auto &v:iptv_vod::page(series,g.name,i,32))assert(ids.insert(v.id).second);
    return ids;
}
static std::set<std::string> live_ids() {
    std::set<std::string> ids;for(const auto &c:iptv_live::snapshot().channels)assert(ids.insert(c.id).second);return ids;
}
static void done_vod(){wait([]{return !iptv_vod::snapshot().loading;},"VOD finish");}
static void done_live(){wait([]{return !iptv_live::summary().loading;},"Live finish");wait([]{return active==0;},"HTTP drained");}
int main() {
    char tmp[]="/tmp/emby5-partial-XXXXXX";assert(mkdtemp(tmp));root=tmp;
    jf::Client client("http://server.test","test","PS5");client.set_session("token","u","User");
    Scenario partial;partial.fail_b=true;
    account("first");set(partial);
    iptv_live::refresh(&client,true);done_live();auto l=iptv_live::summary();
    assert(l.emby_count==1&&l.xtream_count==2&&l.xtream.partial&&l.xtream.failed==1&&!l.xtream.loading);
    assert(live_ids().count("xtream:first-live-a")&&live_ids().count("xtream:first-live-c"));
    assert(hits("action=get_live_streams&category_id=c")==1); // failed middle category does not abort the rest
    iptv_vod::refresh(true);done_vod();auto v=iptv_vod::snapshot();
    assert(v.movie_count==2&&v.show_count==1&&v.movies.partial&&v.movies.failed==1&&v.shows.error.empty());
    assert(vod_ids(false)==std::set<std::string>({"first-movie-a","first-movie-c"}));
    // Retry only the failed categories. Completing a retry MUST keep previously
    // successful categories; `complete` does not mean this request contained all.
    set({});iptv_live::refresh(&client,true,true);done_live();
    assert(iptv_live::summary().xtream_count==3&&iptv_live::summary().xtream.error.empty());
    assert(hits("get_live_categories")==0&&hits("category_id=a")==0&&hits("category_id=c")==0&&hits("category_id=b")==1);
    clear_requests();iptv_vod::refresh(true,true);done_vod();v=iptv_vod::snapshot();
    assert(v.movie_count==3&&v.show_count==1&&v.error.empty());
    assert(hits("get_series")==0&&hits("get_vod_categories")==0&&hits("category_id=b")==1&&hits("category_id=a")==0);
    assert(vod_ids(false).size()==3&&live_ids().size()==4);
    // A valid empty category removes its old records; failed categories retain old
    // records, and later categories still update. Complete refresh prunes deleted groups.
    partial.empty_a=true;set(partial);
    iptv_live::refresh(&client,true);done_live();iptv_vod::refresh(true);done_vod();
    assert(iptv_live::summary().xtream_count==2&&iptv_vod::snapshot().movie_count==2);
    assert(!vod_ids(false).count("first-movie-a")&&vod_ids(false).count("first-movie-b"));
    Scenario removed;removed.remove_b=true;set(removed);
    iptv_live::refresh(&client,true);done_live();iptv_vod::refresh(true);done_vod();
    assert(iptv_live::summary().xtream_count==2&&iptv_vod::snapshot().movie_count==2);
    assert(!vod_ids(false).count("first-movie-b")&&!live_ids().count("xtream:first-live-b"));
    // Malformed/invalid successful HTTP responses do not erase a cached category.
    Scenario malformed;malformed.malformed_a=true;set(malformed);
    iptv_vod::refresh(true);done_vod();assert(vod_ids(false).count("first-movie-a"));
    assert(iptv_vod::snapshot().movies.error=="Invalid catalogue JSON");
    malformed.malformed_a=false;malformed.invalid_items=true;set(malformed);
    iptv_live::refresh(&client,true);done_live();assert(live_ids().count("xtream:first-live-a"));
    assert(iptv_live::summary().xtream.error=="Invalid catalogue item");
    // Movies are usable with a failing or blocked TV-series request (no prior cache).
    account("separate");Scenario series_fail;series_fail.fail_series=true;set(series_fail);
    iptv_vod::refresh(true);done_vod();v=iptv_vod::snapshot();
    assert(v.movie_count==3&&v.show_count==0&&v.movies.error.empty()&&!v.shows.error.empty());
    set({});iptv_vod::refresh(true,true);done_vod();
    assert(iptv_vod::snapshot().show_count==1&&hits("get_vod")==0&&vod_ids(false).size()==3);
    // Actual simultaneous work: Xtream and M3U publish while Emby is blocked.
    account("parallel");with_playlist=true;set({});block_server=true;
    iptv_live::refresh(&client,true);
    wait([]{return server_waits>0&&iptv_live::summary().xtream_count==3&&iptv_live::summary().m3u_count==1;},"other Live sources while server blocked");
    assert(iptv_live::summary().emby_loading&&!iptv_live::summary().xtream_loading);
    block_server=false;done_live();
    block_series=true;iptv_vod::refresh(true);
    wait([]{const auto s=iptv_vod::snapshot();return series_waits>0&&s.movie_count==4&&!s.movies.loading&&s.shows.loading;},"movies while series blocked");
    assert(vod_ids(false).size()==4);block_series=false;done_vod();with_playlist=false;
    // Persist a partial result, evict it from memory by switching identities, then
    // restore it. A fresh file mtime must NOT suppress incomplete-cache retries.
    account("persist");partial.empty_a=false;set(partial);iptv_vod::refresh(true);done_vod();
    assert(iptv_vod::snapshot().movie_count==2);
    account("other");set({});iptv_vod::refresh(true);done_vod();
    account("persist");clear_requests();iptv_vod::refresh(false);done_vod();
    assert(vod_ids(false)==std::set<std::string>({"persist-movie-a","persist-movie-b","persist-movie-c"}));
    assert(hits("get_vod_categories")==0&&hits("action=get_vod_streams&category_id=b")==1&&hits("action=get_vod_streams&category_id=a")==0);
    // Exceptions and nonempty transport errors are failures even with HTTP 200.
    Scenario error;error.throw_movies=true;set(error);iptv_vod::refresh(true);done_vod();
    assert(vod_ids(false).size()==3&&!iptv_vod::snapshot().movies.error.empty()&&iptv_vod::snapshot().shows.error.empty());
    assert(iptv_vod::snapshot().error.find("SECRET")==std::string::npos);
    error.throw_movies=false;error.transport_error=true;set(error);iptv_vod::refresh(true);done_vod();
    assert(vod_ids(false).size()==3&&!iptv_vod::snapshot().error.empty());
    // Authentication failure is not an authoritative empty catalogue.
    account("auth");set({});iptv_live::refresh(&client,true);done_live();
    error={};error.auth_fail=true;set(error);iptv_live::refresh(&client,true);done_live();
    assert(iptv_live::summary().xtream_count==3&&!iptv_live::summary().xtream.error.empty());
    // Empty 200 arrays ARE authoritative and remove stale catalogue entries.
    Scenario empty;empty.empty_all=true;set(empty);
    iptv_live::refresh(&client,true);done_live();iptv_vod::refresh(true);done_vod();
    assert(iptv_live::summary().xtream_count==0&&iptv_vod::snapshot().movie_count==0&&iptv_vod::snapshot().show_count==0);
    assert(iptv_live::summary().xtream.error.empty()&&iptv_vod::snapshot().error.empty());
    // An already-completed GET with direct whole-list data is authoritative.
    Scenario whole;whole.whole_live=true;set(whole);iptv_live::refresh(&client,true);done_live();
    assert(iptv_live::summary().xtream_count==1&&live_ids().count("xtream:whole"));
    // Account changes discard the result of an already-running old worker.
    account("stale");set({});{std::lock_guard<std::mutex> g(mock_mu);blocked_user="stale";}
    const int old_waits=series_waits;block_series=true;iptv_vod::refresh(true);
    wait([&]{return series_waits>old_waits;},"old provider blocked");
    account("current");iptv_vod::refresh(true);done_vod();
    assert(vod_ids(true)==std::set<std::string>({"current-show-s"}));
    block_series=false;wait([]{return active==0;},"old provider drained");
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    assert(vod_ids(true)==std::set<std::string>({"current-show-s"})&&vod_ids(false).count("current-movie-a"));
    // Deadline and cancellation do not start further requests or mutate the caller's cache.
    iptv_xtream::Catalog cat;std::string e;set({});assert(iptv_xtream::load_catalog(creds("budget"),&cat,&e,2));
    const auto old_count=cat.channels.size();clear_requests();
    assert(!iptv_xtream::load_catalog(creds("budget"),&cat,&e,0));assert(cat.channels.size()==old_count&&hits("player_api")==0);
    clear_requests();assert(!iptv_xtream::load_catalog(creds("budget"),&cat,&e,2,[]{return true;}));assert(hits("player_api")==0);
    bool cancel=false;clear_requests();
    assert(!iptv_xtream::load_catalog(creds("budget"),&cat,&e,2,[&]{return cancel;},false,
        [&](const iptv_catalogue::Scan &s){if(s.done==1)cancel=true;}));
    assert(cancel&&cat.channels.size()==old_count&&hits("category_id=b")==0&&hits("category_id=c")==0);
    auto state=iptv_vod::snapshot().movies;state.error="HTTP 503";state.partial=true;state.failed=1;
    const auto label=iptv_catalogue::describe(state,3,"movies");assert(label.find("partial")!=std::string::npos&&label.find("X to retry")!=std::string::npos);
    wait([]{return active==0;},"all network drained");std::this_thread::sleep_for(std::chrono::milliseconds(40));
    std::filesystem::remove_all(root);
    std::cout<<"PASS: first-load partial Live/VOD, failed-middle continuation, retry-only-missing with no data loss, empty/deleted groups, malformed data, independent sources, persisted partial retry, errors/auth/deadlines, provider isolation and status labels\n";
}
