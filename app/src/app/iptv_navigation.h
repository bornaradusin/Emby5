/* Emby5 - GPL-3.0-or-later. Session-scoped Live TV navigation. */
#pragma once
#include "app/iptv_categories.h"
#include "app/iptv_xtream.h"
#include <memory>
#include <string>
#include <vector>
namespace iptv_navigation {
struct Group {
    std::string id, title;
    std::vector<iptv_xtream::Channel> channels;
};
using Groups = std::vector<Group>;
using Playlist = std::shared_ptr<const Groups>;
// Custom rows first, in saved order. Provider rows follow in catalogue order.
// Source-prefixed IDs prevent identically named rows merging across providers.
Groups build_groups(const std::vector<iptv_xtream::Channel> &channels,
                    const std::vector<iptv_categories::Category> &custom,
                    const std::string &search = {});
struct Selection {
    iptv_xtream::Channel channel;
    std::string group_id, group_name, token;
};
bool begin(Playlist groups, size_t group, size_t channel);
bool current(Selection *selection);
bool matches(const std::string &token, const std::string &channel_id);
// After a switch, require D-pad release across controller close/reopen.
bool input_ready(const std::string &token, bool directions_down);
// Exactly one axis and one step (+/-1). Queue at most one transition. The
// current selection changes only after the old player has stopped/reported.
bool request_move(const std::string &token, const std::string &channel_id,
                  int group_step, int channel_step);
bool advance();
void end();
}
