/* Actual trailer parsing/QR encoder; HTTP and rendering are test doubles. */
#include "ui/trailer_qr.h"
#include "jf/jf_client.h"
#include "jf/jf_http.h"
#include <cassert>
#include <iostream>
namespace jf {
HttpResponse http_request(const std::string &method,const std::string &url,const std::vector<std::string>&,const std::string&,int) {
    assert(method=="GET");
    assert(url.find("RemoteTrailers")!=std::string::npos);
    if(url.find("/emby/")!=std::string::npos) assert(url.find("/Users/u/Items/film?")!=std::string::npos);
    else assert(url.find("/Items/film?userId=u")!=std::string::npos);
    HttpResponse r;r.status=200;
    r.body=R"({"Id":"film","Type":"Movie","Name":"Test","RemoteTrailers":[{"Url":"http://127.0.0.1/private"},{"Url":"https://www.youtube.com/watch?v=abcd1234"}],"UserData":{"Played":false}})";
    return r;
}
}
int main() {
    for (auto s:{"https://www.youtube.com/watch?v=abc_123-4", "https://youtu.be/abcd1234", "https://vimeo.com/123456"}) {
        assert(ui::public_trailer_url(s));ui::TrailerQr qr;
        assert(qr.open(s)&&qr.active());qr.input(NUVIO_BTN_CROSS);assert(qr.active());
        qr.input(NUVIO_BTN_CIRCLE);assert(!qr.active());
    }
    for (auto s:{"", "javascript:alert(1)", "file:///data/Emby5/config", "http://127.0.0.1/path", "http://192.168.1.1/film", "http://server.local/film", "http://localhost/film", "https://user:pass@example.com/film", "https://example.com/video?api_key=SECRET", "https://example.com/video?X-Emby-Token=SECRET", "https://example.com/video?%74oken=SECRET", "https://example.com/video?password=SECRET", "https://example.com/video\n"}) {
        assert(!ui::public_trailer_url(s));ui::TrailerQr qr;assert(!qr.open(s)&&!qr.active());
    }
    assert(!ui::public_trailer_url(std::string(1025,'x')));
    assert(ui::first_trailer_url({}).empty());
    for (auto kind:{jf::ServerKind::Emby,jf::ServerKind::Jellyfin}) {
        jf::Client c("https://server.example","test","PS5",kind);c.set_session("token","u","user");
        jf::Item item;assert(c.item("film",&item));assert(item.remote_trailers.size()==2);
        assert(ui::first_trailer_url(item.remote_trailers)=="https://www.youtube.com/watch?v=abcd1234");
    }
    std::cout<<"PASS: actual Emby/Jellyfin RemoteTrailers parsing, safe external link selection, QR encode and Circle close; no Seerr needed\n";
}
