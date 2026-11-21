/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * The collections (BoxSets) a title is in: a film's page shows them under
 * "Included in" (Settings › Playback, off by default). Emby and Jellyfin can
 * list a collection's titles but not a title's collections, so the account's
 * collections and their titles are read once and kept for ten minutes.
 */
#pragma once

#include "jf/jf_client.h"

#include <string>
#include <vector>

namespace collections_index {
/* Never waits: false when there is no current index for this account yet. */
bool lookup(jf::Client &client, const std::string &item_id, std::vector<jf::Item> *out);
/* Reads the index when it is missing or old (seconds on a large server). IO workers only. */
bool build(jf::Client &client);
} // namespace collections_index
