#include "app/user_data_update.h"
#include "jf/jf_http.h"
#include "nuvio_session.h"
extern "C" {
#include "cJSON.h"
}
#include <cassert>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
namespace {
struct Expected { std::string method, path, body; int status; };
struct Recorded { std::string method, url, body; int timeout; };
std::deque<Expected> expected;
std::vector<Recorded> sent;
void expect(const char *method, const char *path, int status, const char *body="") {
    expected.push_back({method,path,body,status});
}
void empty() { assert(expected.empty()); }
const char *watched = R"({"Id":"ep1","Type":"Episode","UserData":{"Played":true,"PlaybackPositionTicks":0,"PlayedPercentage":100,"UnplayedItemCount":0,"IsFavorite":false}})";
const char *unwatched = R"({"Id":"ep1","Type":"Episode","UserData":{"Played":false,"PlaybackPositionTicks":300000000,"PlayedPercentage":10,"UnplayedItemCount":1,"IsFavorite":false}})";
}
namespace jf {
HttpResponse http_request(const std::string &method, const std::string &url,
                          const std::vector<std::string> &, const std::string &body, int timeout) {
    assert(!expected.empty());
    Expected e = expected.front(); expected.pop_front();
    if (method!=e.method || url.find(e.path)==std::string::npos) {
        std::cerr << "Unexpected " << method << " " << url << ", expected " << e.method << " " << e.path << "\n";
        std::abort();
    }
    sent.push_back({method,url,body,timeout});
    HttpResponse r; r.status=e.status; r.body=e.body; return r;
}
}
int main() {
    jf::Client a("http://server-a:8096", "device", "PS5"); a.set_session("token-a","user-a","A");
    jf::Client b("http://server-b:8096", "device", "PS5"); b.set_session("token-b","user-b","B");
    emby5::UserDataRequest request; request.id="ep1"; request.played_set=true; request.played=true;
    // Real Emby successful mutation returns UserItemDataDto, not a whole item.
    expect("POST","/Users/user-a/PlayedItems/ep1",200,R"({"ItemId":"ep1","Played":true,"PlaybackPositionTicks":0,"PlayedPercentage":100,"UnplayedItemCount":0,"IsFavorite":false})");
    auto r=emby5::update_user_data(a,request); empty();
    assert(r.accepted && r.refreshed && r.matched && !r.acknowledged_only);
    jf::Item visible; visible.id="ep1"; visible.position_ticks=100; visible.favorite=true;
    emby5::reconcile_user_data(visible,r.item);
    assert(visible.played && visible.position_ticks==0 && visible.played_percent==100 && !visible.favorite);
    request.played=false;
    expect("DELETE","/Users/user-a/PlayedItems/ep1",403);
    expect("GET","/Users/user-a/Items/ep1?fields=",200,watched);
    r=emby5::update_user_data(a,request); empty();
    assert(!r.accepted && r.refreshed && !r.matched && r.item.played);
    expect("DELETE","/Users/user-a/PlayedItems/ep1",0);
    expect("GET","/Users/user-a/Items/ep1?fields=",200,unwatched);
    r=emby5::update_user_data(a,request); empty();
    assert(!r.accepted && r.refreshed && r.matched); // lost reply: read back, never retry mutation
    const size_t count=sent.size();
    visible.favorite=true;visible.position_ticks=54321;visible.unplayed=8;
    expect("DELETE","/Users/user-a/PlayedItems/ep1",204);
    r=emby5::update_user_data(a,request); empty();
    assert(r.accepted && r.refreshed && r.matched && r.acknowledged_only && sent.size()==count+1);
    assert(r.item.user_data_fields==jf::Item::PlayedField);
    emby5::reconcile_user_data(visible,r.item);
    assert(!visible.played && visible.favorite && visible.position_ticks==54321 && visible.unplayed==8);
    // Missing data on a failed write is not an authoritative unplayed item.
    expect("DELETE","/Users/user-a/PlayedItems/ep1",500);
    expect("GET","/Users/user-a/Items/ep1?fields=",200,R"({"Id":"ep1","Type":"Episode"})");
    r=emby5::update_user_data(a,request);empty();assert(!r.accepted&&!r.refreshed&&!r.matched);
    // An explicit response which disagrees wins over the requested flag.
    expect("DELETE","/Users/user-a/PlayedItems/ep1",200,R"({"Played":true})");
    r=emby5::update_user_data(a,request);empty();assert(r.accepted&&r.refreshed&&!r.matched&&r.item.played);
    // Repeated played/unplayed and favourite/unfavourite work beyond first use.
    for(int i=0;i<100;++i){
        request={};request.id="ep1";request.played_set=true;request.played=i%2==0;
        const auto before=sent.size();
        expect(request.played?"POST":"DELETE","/Users/user-a/PlayedItems/ep1",204);
        r=emby5::update_user_data(a,request);empty();assert(r.matched&&sent.size()==before+1);
        emby5::reconcile_user_data(visible,r.item);assert(visible.played==request.played);
        request={};request.id="ep1";request.favorite_set=true;request.favorite=i%2==0;
        expect(request.favorite?"POST":"DELETE","/Users/user-a/FavoriteItems/ep1",200,
               request.favorite?R"({"IsFavorite":true})":R"({"IsFavorite":false})");
        r=emby5::update_user_data(a,request);empty();assert(r.matched);
        emby5::reconcile_user_data(visible,r.item);
        assert(visible.favorite==request.favorite&&visible.position_ticks==54321);
    }
    request={};request.id="ep1";request.played_set=true;request.played=true;
    expect("POST","/Users/user-b/PlayedItems/ep1",204);
    r=emby5::update_user_data(b,request); empty();assert(r.matched);
    assert(sent.back().url.find("server-b")!=std::string::npos);
    expect("POST","/Users/user-a/PlayedItems/ep1",204);
    r=emby5::update_user_data(a,request);empty();assert(r.matched);
    assert(sent.back().url.find("server-a")!=std::string::npos);
    request.id="series1";
    expect("POST","/Users/user-a/PlayedItems/series1",200,R"({"ItemId":"series1","Played":true,"UnplayedItemCount":0})");
    r=emby5::update_user_data(a,request); empty(); assert(r.matched && r.item.unplayed==0);
    jf::Item episode;episode.id="ep1";episode.played=false;
    emby5::reconcile_user_data(episode,r.item);assert(!episode.played); // no invented child state
    request={}; request.id="ep1"; request.resume_cleared=true;
    expect("POST","/Users/user-a/Items/ep1/UserData",204);
    r=emby5::update_user_data(a,request);empty();assert(r.matched);
    assert(sent.back().body=="{\"PlaybackPositionTicks\":0}");
    visible.played=true;visible.favorite=true;
    emby5::reconcile_user_data(visible,r.item);assert(visible.position_ticks==0&&visible.played&&visible.favorite);
    request={};request.id="ep1";request.favorite_set=true;request.favorite=false;
    expect("DELETE","/Users/user-a/FavoriteItems/ep1",200,R"({"UserData":{"IsFavorite":false}})");
    r=emby5::update_user_data(a,request);empty();assert(r.matched);
    jf::Item parsed;
    expect("GET","/Users/user-a/Items/ep1?fields=",200,R"({"Id":"ep1","UserData":[{"Played":true}]})");
    assert(a.item("ep1",&parsed)&&parsed.user_data_present&&parsed.played);empty();
    expect("GET","/Users/user-a/Items/ep1?fields=",200,R"({"Id":"ep1","UserData":[{"Played":true},{"Played":false}]})");
    assert(a.item("ep1",&parsed)&&!parsed.user_data_present);empty(); // don't pick another user's data
    jf::Playback pb; pb.item_id="ep1\"quoted"; pb.media_source_id="source\\test"; pb.play_session_id="session1"; pb.play_method="DirectStream";
    expect("POST","/Sessions/Playing",204); assert(a.report_start(pb,42));
    expect("POST","/Sessions/Playing/Progress",500); assert(!a.report_progress(pb,100,true,"Pause"));
    expect("POST","/Sessions/Playing/Progress",204); assert(a.report_progress(pb,123,true,"Pause"));
    cJSON *j=cJSON_Parse(sent.back().body.c_str()); assert(j);
    assert(std::string(cJSON_GetObjectItem(j,"ItemId")->valuestring)==pb.item_id);
    assert(cJSON_GetObjectItem(j,"PositionTicks")->valuedouble==123);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j,"IsPaused")));
    assert(std::string(cJSON_GetObjectItem(j,"EventName")->valuestring)=="Pause"); cJSON_Delete(j);
    expect("POST","/Sessions/Playing/Stopped",204); assert(a.report_stopped(pb,456)); empty();
    assert(sent.back().timeout>0 && sent.back().timeout<=5);
    bool ok=false;
    expect("GET","/Items/Resume",200,R"({"Items":[]})"); assert(a.resume(20,"",&ok).empty() && ok);
    expect("GET","/Items/Resume",500); assert(a.resume(20,"",&ok).empty() && !ok);
    expect("GET","/Items/Resume",200,R"({"Items":[)"); assert(a.resume(20,"",&ok).empty() && !ok);
    expect("GET","/Items/Latest",200,"[]"); assert(a.latest("movies",20,&ok).empty() && ok);
    expect("GET","/Items?",200,R"({"Items":[],"TotalRecordCount":0})");
    assert(a.library("","Movie","SortName",false,0,20).ok);
    expect("GET","/Items?",500); assert(!a.library("","Movie","SortName",false,0,20).ok); empty();
    NuvioRequest req; assert(nuvio_request_parse(R"({"url":"http://media/video","autoplayCount":2,"unattendedSeconds":7000,"stillWatchingMode":1,"prefs":{"autoplayNext":true}})",req));
    assert(req.autoplay_count==2 && req.prefs.unattended_seconds==7000 && req.prefs.still_watching_mode==1);
    NuvioResult result; result.automatic_next=true; result.completed_episodes=2; result.watched_seconds=7001;
    j=cJSON_Parse(nuvio_result_json(req,result).c_str()); assert(j);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j,"automaticNext")));
    assert(cJSON_GetObjectItem(j,"completedEpisodes")->valueint==2);
    assert(cJSON_GetObjectItem(j,"watchedSeconds")->valuedouble==7001); cJSON_Delete(j);
    std::cout << "PASS: server-confirmed watched/unwatched, failed/ambiguous writes, no mutation retry, series, resume, favorite, accounts, report JSON/results, empty-vs-failed lists, guard serialization (" << sent.size() << " HTTP checks)\n";
}
