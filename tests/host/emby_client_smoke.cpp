#include "jf/jf_client.h"
#include "jf/jf_http.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> g_urls;
std::vector<std::vector<std::string>> g_headers;
}

namespace jf {
HttpResponse http_request(const std::string &method, const std::string &url,
                          const std::vector<std::string> &headers, const std::string &body,
                          int)
{
    (void)method; (void)body;
    g_urls.push_back(url);
    g_headers.push_back(headers);
    HttpResponse r; r.status = 200;
    if (url.find("/emby/System/Info/Public") != std::string::npos)
        r.body = R"({"ServerName":"Lab","Version":"4.9.0","Id":"srv"})";
    else if (url.find("/emby/Users/AuthenticateByName") != std::string::npos)
        r.body = R"({"AccessToken":"tok","User":{"Id":"u1","Name":"Tester","PrimaryImageTag":"img"}})";
    else if (url.find("/emby/Users/u1/Views") != std::string::npos)
        r.body = R"({"Items":[{"Id":"movies","Name":"Movies","Type":"CollectionFolder","CollectionType":"movies"}],"TotalRecordCount":1})";
    else if (url.find("/emby/Shows/Upcoming") != std::string::npos)
        r.body = R"({"Items":[{"Id":"future1","Name":"New Episode","Type":"Episode","SeriesId":"s1","SeasonId":"season1","IndexNumber":5,"ParentIndexNumber":2,"PremiereDate":"2099-01-01T00:00:00Z"},{"Id":"old","Type":"Episode","SeriesId":"s1","PremiereDate":"2000-01-01T00:00:00Z"},{"Id":"other","Type":"Episode","SeriesId":"s2","PremiereDate":"2099-01-01T00:00:00Z"}]})";
    else if (url.find("/emby/Users/u1/Items/Resume") != std::string::npos)
        r.body = R"({"Items":[],"TotalRecordCount":0})";
    else if (url.find("/emby/Users/u1/Items/Latest") != std::string::npos)
        r.body = R"([])";
    else if (url.find("/emby/Items/movie1/PlaybackInfo") != std::string::npos)
        r.body = R"({"PlaySessionId":"ps1","MediaSources":[{"Id":"ms1","Name":"1080p","SupportsDirectPlay":false,"SupportsDirectStream":true,"DirectStreamUrl":"/emby/Videos/movie1/stream.mp4?MediaSourceId=ms1&PlaySessionId=ps1","Container":"mkv","DefaultAudioStreamIndex":1,"DefaultSubtitleStreamIndex":-1,"MediaStreams":[{"Type":"Video","Codec":"h264","Height":1080},{"Type":"Audio","Codec":"aac","Index":1}]}]})";
    else
        r.body = R"({})";
    return r;
}
}

static bool has_header(const std::vector<std::string>& hs, const std::string& prefix) {
    for (const auto& h : hs) if (h.rfind(prefix, 0) == 0) return true;
    return false;
}

int main()
{
    jf::Client c("http://127.0.0.1:8096/emby", "dev-id", "PlayStation 5");
    std::string name, version, id;
    assert(c.public_info(&name, &version, &id));
    assert(name == "Lab" && id == "srv");
    assert(c.authenticate("Tester", "secret"));
    assert(c.token() == "tok" && c.user_id() == "u1");
    auto views = c.views();
    assert(views.size() == 1 && views[0].id == "movies");
    (void)c.resume(10);
    (void)c.latest("movies", 10);
    const auto upcoming = c.upcoming_episodes("s1");
    assert(upcoming.size() == 1 && upcoming[0].id == "future1" && upcoming[0].upcoming);
    jf::Playback pb;
    assert(c.playback_info("movie1", 0, -1, -2, &pb));
    assert(pb.play_method == "DirectStream");
    assert(pb.url == "http://127.0.0.1:8096/emby/Videos/movie1/stream.mp4?MediaSourceId=ms1&PlaySessionId=ps1");

    for (const auto& u : g_urls) {
        assert(u.find("/emby/emby/") == std::string::npos);
        assert(u.find("/emby/") != std::string::npos);
    }
    assert(has_header(g_headers[1], "X-Emby-Authorization: Emby "));
    bool token_header_seen = false;
    for (size_t i = 0; i < g_headers.size(); ++i)
        token_header_seen = token_header_seen || has_header(g_headers[i], "X-Emby-Token: tok");
    assert(token_header_seen);

    std::cout << "Emby client smoke test passed (" << g_urls.size() << " mocked requests)\n";
    return 0;
}
