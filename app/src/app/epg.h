/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The TV guide's programmes, for every Live TV source:
 *  - Emby / Jellyfin: the server's guide (/LiveTv/Programs), a few channels a request;
 *  - Xtream: the provider's guide for one channel at a time (get_short_epg);
 *  - M3U: an XMLTV file, the one entered in Settings or the playlist's own
 *    (url-tvg), downloaded in the background, read as it arrives and kept on
 *    disk for the day.
 * Only what the guide is showing is fetched, nearest first. Nothing here
 * blocks the caller: what isn't there yet is reported as loading. */
#pragma once
#include "app/iptv_xtream.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace jf { class Client; }

namespace epg {

struct Programme {
    int64_t start = 0, end = 0;   /* Unix seconds */
    std::string title, description;
};
using List = std::shared_ptr<const std::vector<Programme>>;

struct Row {
    List programmes;        /* sorted by start; null while nothing is known */
    bool loading = false;   /* still coming */
    bool none = false;      /* asked, and the source has nothing for this channel */
};

/* What the guide knows for this channel now. */
Row get(const iptv_xtream::Channel &channel);
/* The guide is showing these channels (the focused one first): fetch what is
 * missing or out of date. Cheap to call every frame. */
void want(jf::Client *client, const std::vector<iptv_xtream::Channel> &channels);
/* What is on now and next, for the cards and the player, when the source's
 * own now/next is empty (an M3U channel, say). */
bool now_next(const iptv_xtream::Channel &channel, std::string *now, std::string *next,
              int64_t *now_start = nullptr, int64_t *now_end = nullptr);
/* Bumped whenever something arrives: the guide redraws. */
unsigned generation();
/* Another server, user or provider: forget everything. */
void reset();

/* The M3U guide's state, for the guide's footer. */
struct Status {
    bool configured = false, loading = false;
    size_t channels = 0, programmes = 0;
    std::string error;   /* a key for T(), empty when fine */
};
Status m3u_status();

} // namespace epg
