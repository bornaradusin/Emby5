/* Production AVIO/client against real host FFmpeg + local HTTP fixtures.
 * Not a live Emby/Jellyfin server or a PS5 test. */
#include "jf/jf_client.h"
#include "jf/jf_http.h"
#include "app/user_data_update.h"
#include <cassert>
#include <chrono>
#include <iostream>
extern "C" void av_log_set_level(int);
int main(int argc,char **argv){
    assert(argc==2);av_log_set_level(-8);std::string base=argv[1];
    for(auto path:{"/json","/noclength","/empty","/no-content","/chunked","/m3u"}){
        auto r=jf::http_request("GET",base+path,{},"",3);assert(r.ok());
        if(std::string(path)=="/empty"||std::string(path)=="/no-content")assert(r.body.empty());
        else assert(!r.body.empty());
    }
    for(auto path:{"/truncated","/badlencomplete","/denied"}){
        auto r=jf::http_request("GET",base+path,{},"",3);assert(!r.ok());
    }
    const auto start=std::chrono::steady_clock::now();
    auto slow=jf::http_request("GET",base+"/slow",{},"",1);assert(!slow.ok());
    assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(4));
    jf::Client c(base,"test","PS5");c.set_session("test-token","u","user");
    for(int i=0;i<40;++i){
        emby5::UserDataRequest r;r.id="film";r.played_set=true;r.played=i%2==0;
        auto played=emby5::update_user_data(c,r);assert(played.accepted&&played.refreshed&&played.matched);
        r.played_set=false;r.favorite_set=true;r.favorite=i%2==0;
        auto fav=emby5::update_user_data(c,r);assert(fav.accepted&&fav.refreshed&&fav.matched);
        jf::Item item;assert(c.item("film",&item));assert(item.played==(i%2==0)&&item.favorite==(i%2==0));
    }
    std::cout<<"PASS: real host FFmpeg/local HTTP: 200/204, absent Content-Length, chunked, M3U, truncation, 403, slow-drip deadline and 80 played/favourite mutations with read-back\n";
}
