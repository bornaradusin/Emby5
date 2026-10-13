/* Exercise the real playback-request builder and the real native parser. */
#include "../../app/src/emby5_playback.cpp"
#include "nuvio_session.h"
#include "jf/jf_http.h"
#include <cassert>
#include <cstdarg>
#include <iostream>
const char *T(const char *s) { return s; }
namespace settings { All get() { return {}; } }
// The header-builder test never downloads a subtitle or opens a connection.
// Fail-fast definitions also keep the ASan-linked test self-contained.
extern "C" int nuvio_subs_add_external(const char *, const char *, const char *, const char *) { std::abort(); }
namespace jf {
HttpResponse http_request(const std::string &, const std::string &, const std::vector<std::string> &, const std::string &, int) { std::abort(); }
}
int main() {
    for (auto kind : {jf::ServerKind::Emby,jf::ServerKind::Jellyfin}) {
        jf::Client client("https://local.test/base","ps5","PS5",kind);
        client.set_session("my-token","user","Viewer");
        jf::Item item; item.id="one"; item.type="Episode"; item.name="Episode One";
        jf::Playback pb; pb.url=client.resource_url("/Videos/one/stream.ts"); pb.play_method="DirectStream";
        jf::Version ver; ver.url=pb.url; ver.id="source"; pb.versions.push_back(ver);
        jf::MediaStream sub; sub.type="Subtitle"; sub.is_external=true; sub.delivery_url=client.resource_url("/Videos/one/sub.srt"); pb.streams.push_back(sub);
        Extras extras;
        const std::string payload=request_json(client,item,pb,{},extras);
        NuvioRequest request;
        assert(nuvio_request_parse(payload.c_str(),request));
        assert(request.headers.find(client.auth_header()+"\r\n")!=std::string::npos);
        assert(request.sources.size()==1 && request.sources[0].headers==request.headers);
        assert(request.subtitles.size()==1 && request.subtitles[0].headers==request.headers);
        cJSON *j=cJSON_Parse(payload.c_str());
        const auto *stream=cJSON_GetObjectItem(j,"stream");
        assert(std::string(cJSON_GetObjectItem(stream,"addon")->valuestring)==client.server_product());
        cJSON_Delete(j);
        assert(request.live_navigation_token.empty());
        iptv_xtream::Channel channel;channel.id="server:live1";channel.emby_id="live1";
        channel.name="Live One";channel.source=client.server_product();
        auto groups=std::make_shared<const iptv_navigation::Groups>(
            iptv_navigation::Groups{{"provider:live","Live",{channel}}});
        assert(iptv_navigation::begin(groups,0,0));
        item.id="live1";item.type="LiveTvChannel";
        const auto live_payload=request_json(client,item,pb,{},extras);
        assert(nuvio_request_parse(live_payload.c_str(),request));
        assert(request.live_channel_id==channel.id && request.live_group_name=="Live");
        assert(iptv_navigation::matches(request.live_navigation_token,request.live_channel_id));
        assert(request.headers.find(client.auth_header()+"\r\n")!=std::string::npos);
        item.id="other";
        assert(nuvio_request_parse(request_json(client,item,pb,{},extras).c_str(),request));
        assert(request.live_navigation_token.empty());
        item.id="live1";item.type="Movie";
        assert(nuvio_request_parse(request_json(client,item,pb,{},extras).c_str(),request));
        assert(request.live_navigation_token.empty());iptv_navigation::end();
    }
    std::cout<<"PASS: actual playback request/parser pass Emby and Jellyfin authentication to video, alternate sources and external subtitles; Live TV identity isolated from movies and wrong channels\n";
}
