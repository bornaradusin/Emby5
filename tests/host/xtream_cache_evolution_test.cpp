/* Old-cache upgrade and independent atomic publication using actual workers. GPL-3.0-or-later */
#include "app/iptv_vod.h"
#include "app/iptv_m3u.h"
#include "jf/jf_http.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <sstream>
static std::string root;
static std::atomic<int> mode{0},calls{0};
extern "C" {
const char *evo_data_dir(){return root.c_str();}
const char *evo_data_path(const char *leaf){static thread_local std::string s;s=root+"/"+(leaf?leaf:"");return s.c_str();}
int evo_mkdir(const char *p){std::error_code e;std::filesystem::create_directories(p,e);return e?-1:0;}
}
namespace iptv_m3u {
std::string playlist_url(){return {};}
bool load_catalog(const std::string&,iptv_xtream::Catalog*,std::string*){assert(false);return false;}
}
namespace jf {
HttpResponse http_request(const std::string&,const std::string &url,const std::vector<std::string>&,const std::string&,int seconds) {
    assert(seconds>0&&seconds<=25);++calls;HttpResponse r;r.status=200;
    const auto p=mode.load();const bool series=url.find("series")!=std::string::npos;
    if(p==2 || (p==1&&(series||url.find("category_id=b")!=std::string::npos))) {r.status=503;return r;}
    if(url.find("categories")!=std::string::npos) {
        r.body=series?R"([{"category_id":"s","category_name":"Shows"}])":R"([{"category_id":"a","category_name":"Alpha"},{"category_id":"b","category_name":"Beta"},{"category_id":"c","category_name":"Gamma"}])";
        return r;
    }
    const std::string id=series?"show":url.find("category_id=a")!=std::string::npos?"a":url.find("category_id=b")!=std::string::npos?"b":"c";
    r.body="[{\""+std::string(series?"series_id":"stream_id")+"\":\"new-"+id+"\",\"name\":\"New "+id+"\",\"category_id\":\""+(series?"s":id)+"\"}]";
    return r;
}
}
static iptv_xtream::Credentials credentials(const std::string &u){return {"http://provider.test",u,"private-pass"};}
static void account(const std::string &u){assert(iptv_xtream::save_credentials(credentials(u)));}
static std::string cache(const std::string &user) {
    const auto c=credentials(user);const auto key=c.server+"|"+c.username+"|"+c.password+"|";
    uint64_t h=1469598103934665603ULL;for(unsigned char ch:key)h=(h^ch)*1099511628211ULL;
    char suffix[64];std::snprintf(suffix,sizeof suffix,"/emby5/vod-catalog-%016llx.ndjson",(unsigned long long)h);
    return root+suffix;
}
static std::string section(const std::string &user,bool series) {
    auto s=cache(user);return s.substr(0,s.size()-7)+(series?"-series.ndjson":"-movies.ndjson");
}
static std::string contents(const std::string &p){std::ifstream f(p);return std::string(std::istreambuf_iterator<char>(f),{});}
static void refresh(bool force=true,bool retry=false) {
    iptv_vod::refresh(force,retry);
    for(int i=0;i<4000;++i) {
        if(!iptv_vod::snapshot().loading)return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(false&&"worker stuck");
}
static std::set<std::string> ids(bool series) {
    std::set<std::string> out;for(const auto &g:iptv_vod::groups(series))for(const auto &v:iptv_vod::page(series,g.name,0,32))assert(out.insert(v.id).second);return out;
}
int main() {
    char dir[]="/tmp/emby5-cache-evolution-XXXXXX";assert(mkdtemp(dir));root=dir;account("legacy");
    const std::string legacy=R"({"id":"old-a","name":"Old A","group":"Alpha","series":false,"source":"Xtream","extension":"mp4"})" "\n"
        R"({"id":"old-b","name":"Old B","group":"Beta","series":false,"source":"Xtream","extension":"mp4"})" "\n"
        R"({"id":"old-show","name":"Old Show","group":"Shows","series":true,"source":"Xtream"})" "\n";
    {std::ofstream f(cache("legacy"));f<<legacy;}
    mode=1;refresh();auto s=iptv_vod::snapshot();
    assert(s.movie_count==4&&s.show_count==1&&s.movies.partial&&s.movies.cached&&!s.shows.error.empty());
    assert(ids(false).count("new-a")&&ids(false).count("new-c")&&ids(false).count("old-b"));
    assert(contents(cache("legacy"))==legacy); // no destructive migration of the known old cache
    for(bool series:{false,true}) {
        const auto body=contents(section("legacy",series));
        assert(body.find("_emby5_catalogue")!=std::string::npos);
        assert(body.find("\"complete\":false")!=std::string::npos);
        assert(body.find("private-pass")==std::string::npos&&body.find("http://provider.test")==std::string::npos);
    }
    // Manual-only keeps a cached partial catalogue until explicit retry.
    iptv_vod::set_refresh_interval_hours(0);
    account("other");mode=0;refresh();account("legacy");calls=0;refresh(false);
    assert(calls==0&&iptv_vod::snapshot().movie_count==4&&!iptv_vod::snapshot().movies.error.empty());
    iptv_vod::set_refresh_interval_hours(24);
    account("other");refresh(false);account("legacy");calls=0;refresh(false);
    // Only the one failed movie category and the unreadable series catalogue retry.
    assert(calls==3&&ids(false)==std::set<std::string>({"new-a","new-b","new-c"}));
    assert(ids(true)==std::set<std::string>({"new-show"})&&iptv_vod::snapshot().error.empty());
    // Complete split caches are reusable without contacting a now-offline provider.
    account("other");refresh(false);account("legacy");mode=2;calls=0;refresh(false);
    assert(calls==0&&ids(false).size()==3&&ids(true).size()==1);
    assert(iptv_vod::snapshot().movies.cached&&iptv_vod::snapshot().error.empty());
    // A forced failed refresh keeps data and writes incomplete metadata, so the
    // successful file's old timestamp cannot suppress subsequent recovery.
    refresh(true);assert(!iptv_vod::snapshot().error.empty()&&ids(false).size()==3);
    assert(contents(section("legacy",false)).find("\"complete\":false")!=std::string::npos);
    mode=0;refresh(true,true);assert(iptv_vod::snapshot().error.empty());
    // Simulate a rename failure for ONE section. The other section publishes,
    // the old index stays selected, all flags clear, and temporary files clean up.
    const auto series=section("legacy",true),backup=series+".backup";
    std::filesystem::rename(series,backup);std::filesystem::create_directory(series);
    refresh();s=iptv_vod::snapshot();
    assert(!s.loading&&s.movies.error.empty()&&!s.shows.error.empty()&&s.show_count==1);
    std::filesystem::remove(series);std::filesystem::rename(backup,series);
    assert(ids(true)==std::set<std::string>({"new-show"}));
    for(const auto &e:std::filesystem::directory_iterator(root+"/emby5"))assert(e.path().string().find(".next-")==std::string::npos);
    refresh(true,true);assert(iptv_vod::snapshot().error.empty());
    // A truncated split cache is not accepted as a fresh complete catalogue.
    {std::ofstream f(section("legacy",false));f<<"{broken";}
    account("other");refresh(false);account("legacy");mode=2;calls=0;refresh(false);
    assert(calls>0&&!iptv_vod::snapshot().movies.error.empty());
    assert(ids(false)==std::set<std::string>({"old-a","old-b"}));
    assert(ids(true)==std::set<std::string>({"new-show"}));
    mode=0;refresh();assert(ids(false).size()==3&&iptv_vod::snapshot().error.empty());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));std::filesystem::remove_all(root);
    std::cout<<"PASS: legacy mixed-cache preservation, split-cache persistence, incomplete metadata retry, complete-cache offline reuse, independent disk-error recovery, malformed cache fallback and temporary-file cleanup\n";
}
