/* Real Client and user-data coordinator with deterministic HTTP responses. */
#include "jf/jf_client.h"
#include "jf/jf_http.h"
#include "app/user_data_update.h"
#include "cJSON.h"
#include <cassert>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct Expected { std::string method, url, body; int status; };
struct Sent { std::string method, url, body; std::vector<std::string> headers; };
std::deque<Expected> queue;
std::vector<Sent> sent;
void expect(std::string method, std::string url, int status=200, std::string body="{}") {
    queue.push_back({method,url,body,status});
}
void empty() { assert(queue.empty()); }
bool has(const std::string &s,const std::string &p) { return s.find(p)!=std::string::npos; }
std::string jsonstr(const cJSON *o,const char *k) { const auto *s=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(o,k)); return s?s:""; }
const char *info = R"({"Id":"server-j","ServerName":"Emby named by its owner","ProductName":"Jellyfin Server","Version":"10.11.4"})";
const char *auth = R"({"AccessToken":"token-j","User":{"Id":"user-j","Name":"Viewer","PrimaryImageTag":"pic"}})";
const char *item = R"({"Id":"ep1","Type":"Episode","UserData":{"Played":true,"PlaybackPositionTicks":0,"PlayedPercentage":100,"IsFavorite":true}})";
const std::string base="https://test.local/jellyfin";
void probe(jf::Client &c) {
    expect("GET",base+"/emby/System/Info/Public",404);
    expect("GET",base+"/System/Info/Public",200,info);
    std::string name,version,id;
    assert(c.public_info(&name,&version,&id)); empty();
    assert(c.is_jellyfin() && id=="server-j" && version=="10.11.4");
    assert(name=="Emby named by its owner"); // detect ProductName, not display name
    assert(c.server()==base && c.server_id()==id);
    for (size_t i=sent.size()-2;i<sent.size();++i)
        assert(sent[i].headers==std::vector<std::string>{"Accept: application/json"});
}
}
namespace jf {
HttpResponse http_request(const std::string &method,const std::string &url,
    const std::vector<std::string>& headers,const std::string &body,int) {
    assert(!queue.empty()); auto e=queue.front(); queue.pop_front();
    if (e.method!=method || e.url!=url) {
        std::cerr<<"Expected "<<e.method<<" "<<e.url<<"\nGot "<<method<<" "<<url<<"\n";
        std::abort();
    }
    sent.push_back({method,url,body,headers});
    HttpResponse r; r.status=e.status; r.body=e.body; return r;
}
}
int main() {
    using jf::ServerKind; using namespace jf::server_url;
    assert(normalize("test.local")=="http://test.local:8096");
    assert(normalize("[::1]")=="http://[::1]:8096");
    assert(normalize(" HTTPS://test.local/jellyfin/ ")==base);
    assert(normalize("ftp://host").empty() && normalize("https://u:p@host").empty());
    assert(normalize("http://test.local/../private").empty());
    assert(normalize("http://test.local/?api_key=secret").empty());
    assert(join("http://host",ServerKind::Emby,"/Items")=="http://host/emby/Items");
    assert(join("http://host/emby",ServerKind::Emby,"/emby/Items")=="http://host/emby/Items");
    assert(join("http://host/proxy/emby",ServerKind::Emby,"/Items")=="http://host/proxy/emby/Items");
    assert(join(base,ServerKind::Jellyfin,"/Videos/v/stream")==base+"/Videos/v/stream");
    assert(join(base,ServerKind::Jellyfin,"/jellyfin/Videos/v/stream")==base+"/Videos/v/stream");
    assert(join(base,ServerKind::Jellyfin,base+"/Videos/v/stream")==base+"/Videos/v/stream");
    for (auto bad : {"https://elsewhere/video", "http://test.local/jellyfin/video", "//elsewhere/video",
                     "https://test.local/other/video", "/jellyfin/../other/video", "/jellyfin/%2e%2e/other/video", "/Videos/v\r\nEvil: bad"})
        assert(join(base,ServerKind::Jellyfin,bad).empty());
    assert(jellyfin_route("/Users/u/Views")=="/UserViews?userId=u");
    assert(jellyfin_route("/Users/u/Items/Resume?Limit=2")=="/UserItems/Resume?userId=u&Limit=2");
    assert(jellyfin_route("/Users/u/Items/Latest?ParentId=p")=="/Items/Latest?userId=u&ParentId=p");
    assert(jellyfin_route("/Users/u/PlayedItems/v")=="/UserPlayedItems/v?userId=u");
    assert(jellyfin_route("/Users/u/FavoriteItems/v")=="/UserFavoriteItems/v?userId=u");
    assert(jellyfin_route("/Users/u/Items/v/UserData")=="/UserItems/v/UserData?userId=u");
    assert(jellyfin_route("/Users/u/Configuration")=="/Users/Configuration?userId=u");
    assert(jellyfin_route("/Users/AuthenticateByName")=="/Users/AuthenticateByName");

    jf::Client emby("http://emby.local/emby","device","PS5");
    expect("GET","http://emby.local/emby/System/Info/Public",200,R"({"Id":"e","Version":"4.9.0","ServerName":"Jellyfin named by owner"})");
    assert(emby.public_info(nullptr,nullptr,nullptr) && !emby.is_jellyfin());
    assert(!emby.quick_connect_enabled()); // no Jellyfin-only request made to Emby
    emby.set_session("token-e","user-e","E");
    assert(has(emby.auth_header(),"X-Emby-Authorization: Emby "));
    assert(emby.websocket_url()=="http://emby.local/emby/socket?deviceId=device");
    assert(emby.user_image_url("u","pic",240)=="http://emby.local/emby/Users/u/Images/Primary?tag=pic&fillWidth=240");

    jf::Client c(base,"ps5-device","PlayStation 5");
    c.set_session("saved-secret","saved-user","old"); probe(c); // probe never carries old token
    c.set_session("","","");
    expect("POST",base+"/Users/AuthenticateByName",200,auth);
    assert(c.authenticate("Viewer","p\"w\\x")); empty();
    auto *j=cJSON_Parse(sent.back().body.c_str());
    assert(jsonstr(j,"Username")=="Viewer" && jsonstr(j,"Pw")=="p\"w\\x"); cJSON_Delete(j);
    assert(c.token()=="token-j" && c.user_id()=="user-j");
    assert(has(c.auth_header(),"Authorization: MediaBrowser Client=\"Emby5\""));
    assert(has(c.auth_header(),"Device=\"PlayStation%205\""));
    for(const auto &h:c.request_headers()) assert(!has(h,"X-Emby"));
    assert(c.websocket_url()==base+"/socket?deviceId=ps5-device");
    assert(c.user_image_url("user-j","pic",240)==base+"/UserImage?userId=user-j&tag=pic&fillWidth=240");
    expect("GET",base+"/Users/user-j",200,R"({"Id":"wrong-user","Name":"Other"})");
    assert(!c.validate() && c.user_id()=="user-j");
    expect("GET",base+"/Users/user-j",200,R"({"Id":"user-j","Name":"Viewer","Configuration":{"SubtitleMode":"Smart"}})");
    assert(c.validate());
    jf::UserPrefs prefs;
    expect("GET",base+"/Users/user-j",200,R"({"Configuration":{"SubtitleMode":"Smart"}})");
    assert(c.get_prefs(&prefs) && prefs.autoplay_next);
    prefs.autoplay_next=false;
    expect("GET",base+"/Users/user-j",200,R"({"Configuration":{"SubtitleMode":"Smart","RememberAudioSelections":true}})");
    expect("POST",base+"/Users/Configuration?userId=user-j",204,"");
    assert(c.set_prefs(prefs)); j=cJSON_Parse(sent.back().body.c_str());
    assert(cJSON_IsFalse(cJSON_GetObjectItem(j,"EnableNextEpisodeAutoPlay")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j,"RememberAudioSelections"))); cJSON_Delete(j);
    bool ok=false;
    // get_json tests the exact routing used by existing library and TV UI modules.
    std::string body;
    for(auto route:{"/Users/user-j/Views", "/Users/user-j/Items/Resume?Limit=2", "/Users/user-j/Items/Latest?Limit=2", "/Shows/s/Episodes?userId=user-j", "/Shows/NextUp?userId=user-j", "/LiveTv/Channels?UserId=user-j"}) {
        expect("GET",base+jellyfin_route(route),200,R"({"Items":[]})"); assert(c.get_json(route,&body));
    }
    emby5::UserDataRequest change; change.id="ep1"; change.played_set=true; change.played=true;
    expect("POST",base+"/UserPlayedItems/ep1?userId=user-j",204,"");
    auto result=emby5::update_user_data(c,change);
    assert(result.accepted && result.refreshed && result.matched && result.acknowledged_only);
    change.played=false;
    expect("DELETE",base+"/UserPlayedItems/ep1?userId=user-j",403,"");
    expect("GET",base+"/Items/ep1?userId=user-j&fields=Overview,Genres,MediaStreams,Taglines,People,Studios,ChildCount,ProductionLocations,SpecialFeatureCount,ProviderIds,RemoteTrailers",200,item);
    result=emby5::update_user_data(c,change); assert(!result.accepted && !result.matched && result.item.played);
    expect("POST",base+"/UserFavoriteItems/ep1?userId=user-j",204,""); assert(c.set_favorite("ep1",true));
    expect("DELETE",base+"/UserFavoriteItems/ep1?userId=user-j",204,""); assert(c.set_favorite("ep1",false));
    expect("GET",base+"/UserItems/ep1/UserData?userId=user-j",200,R"({"Played":true,"IsFavorite":true,"PlayCount":3,"PlaybackPositionTicks":100,"LastPlayedDate":"2026-01-01T12:00:00Z"})");
    expect("POST",base+"/UserItems/ep1/UserData?userId=user-j",200,"{}"); assert(c.clear_position("ep1"));
    j=cJSON_Parse(sent.back().body.c_str());
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j,"Played")) && cJSON_IsTrue(cJSON_GetObjectItem(j,"IsFavorite")));
    assert(cJSON_GetObjectItem(j,"PlaybackPositionTicks")->valueint==0 && cJSON_GetObjectItem(j,"PlayCount")->valueint==3);
    assert(jsonstr(j,"LastPlayedDate")=="2026-01-01T12:00:00Z"); cJSON_Delete(j);
    expect("GET",base+"/UserItems/ep1/UserData?userId=user-j",200,"{}"); assert(!c.clear_position("ep1")); empty();

    jf::Client qc(base,"ps5","PS5",ServerKind::Jellyfin);
    expect("GET",base+"/QuickConnect/Enabled",200,"false"); assert(!qc.quick_connect_enabled());
    expect("GET",base+"/QuickConnect/Enabled",200,"true"); assert(qc.quick_connect_enabled());
    expect("POST",base+"/QuickConnect/Initiate",200,R"({"Code":"123456","Secret":"s\"ecret"})");
    jf::QuickConnect request; assert(qc.quick_connect_start(&request));
    expect("GET",base+"/QuickConnect/Connect?secret=s%22ecret",200,R"({"Authenticated":false})");
    assert(qc.quick_connect_poll(request,&ok) && !ok);
    expect("GET",base+"/QuickConnect/Connect?secret=s%22ecret",200,R"({"Authenticated":true})");
    expect("POST",base+"/Users/AuthenticateWithQuickConnect",200,auth);
    assert(qc.quick_connect_poll(request,&ok) && ok);
    j=cJSON_Parse(sent.back().body.c_str()); assert(jsonstr(j,"Secret")=="s\"ecret"); cJSON_Delete(j);
    expect("POST",base+"/QuickConnect/Authorize?code=123456",200,"false"); assert(!qc.quick_connect_authorize("123456"));
    expect("POST",base+"/QuickConnect/Authorize?code=123456",200,"true"); assert(qc.quick_connect_authorize("123456"));
    jf::Playback pb;
    const std::string playpath=base+"/Items/ep1/PlaybackInfo?userId=user-j";
    expect("POST",playpath,200,R"({"PlaySessionId":"session","MediaSources":[{"Id":"media","Container":"mkv","SupportsDirectPlay":true,"MediaStreams":[{"Type":"Video","Height":1080},{"Type":"Subtitle","IsExternal":true,"DeliveryUrl":"/jellyfin/Videos/ep1/media/Subtitles/2/Stream.srt"}]}]})");
    assert(c.playback_info("ep1",123,-1,-1,&pb));
    assert(pb.play_method=="DirectPlay" && pb.url==base+"/Videos/ep1/stream?static=true&mediaSourceId=media&playSessionId=session&deviceId=ps5-device&api_key=token-j");
    assert(pb.streams.back().delivery_url==base+"/Videos/ep1/media/Subtitles/2/Stream.srt");
    expect("POST",base+"/Sessions/Playing",204,""); assert(c.report_start(pb,123));
    expect("POST",base+"/Sessions/Playing/Progress",204,""); assert(c.report_progress(pb,500,true,"Pause"));
    expect("POST",base+"/Sessions/Playing/Stopped",204,""); assert(c.report_stopped(pb,900));
    c.stop_encoding(pb); // direct file, no resource to close
    expect("POST",playpath,200,R"({"PlaySessionId":"session","MediaSources":[{"Id":"a","SupportsDirectPlay":true,"MediaStreams":[{"Type":"Audio"}]}]})");
    assert(c.playback_info("ep1",0,-1,-1,&pb) && has(pb.url,"/Audio/ep1/stream"));
    expect("POST",playpath,200,R"({"PlaySessionId":"session","MediaSources":[{"Id":"media","SupportsDirectStream":true,"DirectStreamUrl":"/Videos/ep1/stream.ts?static=false"}]})");
    assert(c.playback_info("ep1",0,-1,-1,&pb) && pb.url==base+"/Videos/ep1/stream.ts?static=false" && pb.play_method=="DirectStream");
    expect("POST",playpath,200,R"({"PlaySessionId":"session","MediaSources":[{"Id":"media","TranscodingUrl":"/jellyfin/Videos/ep1/master.m3u8?api_key=token-j"}]})");
    assert(c.playback_info("ep1",0,-1,-1,&pb) && pb.url==base+"/Videos/ep1/master.m3u8?api_key=token-j" && pb.play_method=="Transcode");
    expect("DELETE",base+"/Videos/ActiveEncodings?deviceId=ps5-device&playSessionId=session",204,""); c.stop_encoding(pb);
    expect("POST",playpath,200,R"({"MediaSources":[{"Id":"bad","TranscodingUrl":"https://elsewhere/steal"}]})");
    assert(!c.playback_info("ep1",0,-1,-1,&pb));
    expect("POST",playpath,200,R"({"PlaySessionId":"live","MediaSources":[{"Id":"tv","SupportsDirectPlay":true,"LiveStreamId":"stream-live","RequiresClosing":true,"MediaStreams":[{"Type":"Video"}]}]})");
    assert(c.playback_info("ep1",0,-1,-1,&pb) && has(pb.url,"&liveStreamId=stream-live"));
    expect("POST",base+"/Sessions/Playing/Stopped",204,""); assert(c.report_stopped(pb,42));
    j=cJSON_Parse(sent.back().body.c_str()); assert(jsonstr(j,"LiveStreamId")=="stream-live"); cJSON_Delete(j);
    expect("POST",base+"/LiveStreams/Close?liveStreamId=stream-live",204,""); c.stop_encoding(pb);
    // Separate provider client still carries its own token and unchanged Emby routes.
    expect("POST","http://emby.local/emby/Users/user-e/PlayedItems/ep1",204,""); assert(emby.set_played("ep1",true));
    assert(has(sent.back().headers[0],"token-e") && !has(sent.back().headers[0],"token-j"));
    c.set_server("https://other.local"); assert(!c.is_jellyfin() && c.token().empty() && c.user_id().empty());
    empty();
    std::cout<<"PASS: dual-backend URLs, discovery identity, auth, proxy paths, Quick Connect, watched-state, resume preservation, playback/subtitles/audio/live, reports, cleanup and isolation ("<<sent.size()<<" HTTP checks)\n";
}
