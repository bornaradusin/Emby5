/* Real Xtream/VOD/Live workers and disk caches with mocked HTTP and M3U. */
#include "app/iptv_xtream.h"
#include "app/iptv_vod.h"
#include "app/iptv_live.h"
#include "app/iptv_m3u.h"
#include "app/request_budget.h"
#include "jf/jf_http.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <sys/stat.h>
static std::string root;
static std::atomic<int> phase{0},calls{0},active{0};
extern "C" {
const char *evo_data_dir(){return root.c_str();}
const char *evo_data_path(const char *leaf){static thread_local std::string p;p=root+"/"+(leaf?leaf:"");return p.c_str();}
int evo_mkdir(const char *p){std::error_code e;std::filesystem::create_directories(p,e);return e?-1:0;}
}
namespace iptv_m3u {
std::string playlist_url(){return {};}
bool load_catalog(const std::string&,iptv_xtream::Catalog*,std::string*){assert(false);return false;}
}
namespace jf {
HttpResponse http_request(const std::string&,const std::string &url,const std::vector<std::string>&,const std::string&,int timeout) {
    assert(timeout>0 && timeout<=30);++calls;++active;
    struct Done{~Done(){--active;}} done;
    const int p=phase.load();HttpResponse r;r.status=200;
    if(url.find("/LiveTv/Channels?")!=std::string::npos){
        if(p==2){r.status=500;return r;}
        r.body=R"({"Items":[{"Id":"e1","Name":"Emby channel"}]})";return r;
    }
    if(url.find("/LiveTv/Programs?")!=std::string::npos){r.body=R"({"Items":[]})";return r;}
    assert(url.find("http://provider.test/player_api.php?")==0);
    assert(url.find("password=p%2Fword")!=std::string::npos);
    if(url.find("action=")==std::string::npos){r.body=R"({"user_info":{"auth":1,"status":"Active"}})";return r;}
    if(p==3 && url.find("get_vod_categories")!=std::string::npos)throw std::runtime_error("mock network allocation failure");
    if(url.find("get_live_categories")!=std::string::npos){r.body=R"([{"category_id":"a","category_name":"News"},{"category_id":"b","category_name":"Sports"}])";return r;}
    if(url.find("get_live_streams")!=std::string::npos){
        if(url.find("category_id=")==std::string::npos){r.status=0;r.error="large response";return r;}
        if(url.find("category_id=b")!=std::string::npos&&p==2){r.status=503;return r;}
        r.body=url.find("category_id=a")!=std::string::npos ? R"([{"stream_id":"1","name":"One","category_id":"a"}])" : R"([{"stream_id":"1","name":"One","category_id":"a"},{"stream_id":"2","name":"Two","category_id":"b"}])";
        return r;
    }
    if(url.find("get_vod_categories")!=std::string::npos){r.body=p==4?"[]":R"([{"category_id":"a","category_name":"Movies A"},{"category_id":"b","category_name":"Movies B"}])";return r;}
    if(url.find("get_series_categories")!=std::string::npos){r.body=p==4?"[]":R"([{"category_id":"s","category_name":"Series"}])";return r;}
    if(url.find("get_vod_streams")!=std::string::npos){
        if(p==4){r.body="[]";return r;}
        if(p==3&&url.find("category_id=")==std::string::npos){r.status=503;return r;}
        assert(url.find("category_id=")!=std::string::npos);
        if(url.find("category_id=b")!=std::string::npos&&p==2){r.status=503;return r;}
        r.body=url.find("category_id=a")!=std::string::npos ? R"([{"stream_id":"10","name":"Movie A","category_id":"a","container_extension":"mkv"}])" : R"([{"stream_id":"20","name":"Movie B","category_id":"b","container_extension":"mp4"}])";
        return r;
    }
    if(url.find("get_series_info")!=std::string::npos){r.body=R"({"episodes":{"1":[{"id":"201","title":"Episode one","episode_num":1,"container_extension":"mkv"}]}})";return r;}
    if(url.find("get_series")!=std::string::npos){r.body=p==4?"[]":R"([{"series_id":"30","name":"Show","category_id":"s"}])";return r;}
    std::cerr<<"Unexpected HTTP route\n";assert(false);return r;
}
}
template<class Fn> void wait_for(Fn ready){
    for(int i=0;i<4000;++i){if(ready())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    assert(false && "worker did not finish");
}
int main(){
    char temp[]="/tmp/emby5-refresh-XXXXXX";assert(mkdtemp(temp));root=temp;
    iptv_xtream::Credentials creds{"http://provider.test","test","p/word"};assert(iptv_xtream::save_credentials(creds));
    std::string error;iptv_xtream::Catalog cat;
    assert(iptv_xtream::load_catalog(creds,&cat,&error,3));assert(cat.channels.size()==2&&cat.categories.size()==2&&error.empty());
    phase=2;assert(!iptv_xtream::load_catalog(creds,&cat,&error,3));assert(cat.channels.size()==2&&!error.empty());
    const int before=calls;assert(!iptv_xtream::load_catalog(creds,&cat,&error,3,[]{return true;}));assert(calls==before);
    emby5::RequestBudget budget(0);assert(budget.expired()&&budget.timeout()==0);
    phase=0;iptv_vod::refresh(true);wait_for([]{return !iptv_vod::snapshot().loading;});
    auto vod=iptv_vod::snapshot();assert(vod.movie_count==2&&vod.show_count==1&&vod.error.empty());
    assert(vod.categories_done==3&&vod.categories_total==3);
    auto group=iptv_vod::groups(false);assert(group.size()==2);
    auto movie=iptv_vod::page(false,group[0].name,0,1);assert(movie.size()==1);
    assert(movie[0].url.find("/movie/test/p%2Fword/")!=std::string::npos);
    auto sg=iptv_vod::groups(true);assert(sg.size()==1);
    auto show=iptv_vod::page(true,sg[0].name,0,10);auto eps=iptv_vod::episodes(show.at(0),&error);
    assert(eps.size()==1&&eps[0].url=="http://provider.test/series/test/p%2Fword/201.mkv");
    phase=2;iptv_vod::refresh(true);wait_for([]{return !iptv_vod::snapshot().loading;});
    vod=iptv_vod::snapshot();assert(vod.movie_count==2&&vod.show_count==1&&!vod.error.empty());
    const auto failed_calls=calls.load();for(int i=0;i<10;++i)iptv_vod::refresh(false);assert(calls==failed_calls);
    phase=3;iptv_vod::refresh(true);wait_for([]{return !iptv_vod::snapshot().loading;});
    assert(iptv_vod::snapshot().movie_count==2&&!iptv_vod::snapshot().error.empty()); // exception cleanup
    phase=0;iptv_vod::refresh(true);wait_for([]{return !iptv_vod::snapshot().loading;});assert(iptv_vod::snapshot().error.empty());
    // Live publishes independent successful sources and preserves them on failure.
    jf::Client client("http://server.test","test","PS5");client.set_session("token","u","u");
    iptv_live::refresh(&client,true);wait_for([]{return !iptv_live::summary().loading&&active==0;});
    auto live=iptv_live::summary();assert(live.emby_count==1&&live.xtream_count==2&&live.channels.empty());
    assert(!live.emby_loading&&!live.xtream_loading&&live.xtream_error.empty());
    phase=2;iptv_live::refresh(&client,true);wait_for([]{return !iptv_live::summary().loading&&active==0;});
    live=iptv_live::summary();assert(live.emby_count==1&&live.xtream_count==2&&!live.emby_error.empty()&&!live.xtream_error.empty());
    phase=0;iptv_live::refresh(&client,true);wait_for([]{return !iptv_live::summary().loading&&active==0;});
    assert(iptv_live::summary().xtream_error.empty());
    phase=4;iptv_vod::refresh(true);wait_for([]{return !iptv_vod::snapshot().loading;});
    assert(iptv_vod::snapshot().movie_count==0&&iptv_vod::snapshot().show_count==0&&iptv_vod::snapshot().error.empty());
    wait_for([]{return active==0;});std::this_thread::sleep_for(std::chrono::milliseconds(40));
    std::filesystem::remove_all(root);
    std::cout<<"PASS: actual Xtream category fallback/dedup/cancel, atomic VOD disk-cache refresh, error/exception recovery, cooldown, independent Live sources and playable VOD URLs\n";
}
