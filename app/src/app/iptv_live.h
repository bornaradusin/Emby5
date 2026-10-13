#pragma once
#include "app/iptv_xtream.h"
#include "jf/jf_client.h"
#include <cstdint>
#include <vector>
namespace iptv_live {
struct Snapshot {
    std::vector<iptv_xtream::Channel> channels;
    unsigned emby_count=0, xtream_count=0, m3u_count=0;
    bool loading=false, emby_loading=false, xtream_loading=false;
    std::string emby_error,xtream_error,m3u_error;
    uint64_t generation=0, catalogue_generation=0;
    iptv_catalogue::State server, xtream, playlist;
    iptv_catalogue::Scan xtream_scan; // Metadata only; no credential-bearing URLs.
};
void refresh(jf::Client *client, bool force=false, bool retry_failed=false);
Snapshot snapshot();
Snapshot summary(); // Counts/status only: do not copy thousands of channels every Settings frame.
// Remember which channel launched playback, for its player EPG overlay.
void set_playing_channel(const std::string &id);
std::string playing_channel();
bool channel_by_id(const std::string &id, iptv_xtream::Channel *channel);
void request_guide(jf::Client *client, const iptv_xtream::Channel &channel);
}
