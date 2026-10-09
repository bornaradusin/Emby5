#pragma once
#include "app/iptv_xtream.h"
#include "jf/jf_client.h"
#include <cstdint>
#include <vector>
namespace iptv_live {
struct Snapshot {
    std::vector<iptv_xtream::Channel> channels;
    unsigned emby_count=0, xtream_count=0, m3u_count=0;
    bool loading=false;
    uint64_t generation=0;
};
void refresh(jf::Client *client, bool force=false);
Snapshot snapshot();
// Remember which channel launched playback, for its player EPG overlay.
void set_playing_channel(const std::string &id);
std::string playing_channel();
void request_guide(jf::Client *client, const iptv_xtream::Channel &channel);
}
