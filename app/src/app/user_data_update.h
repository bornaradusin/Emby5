/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "jf/jf_client.h"
namespace emby5 {
struct UserDataRequest {
    std::string id;
    bool favorite_set = false, favorite = false;
    bool played_set = false, played = false;
    bool resume_cleared = false;
};
struct UserDataResult {
    bool accepted = false, refreshed = false, matched = false;
    // True when only HTTP success was returned. item is then a field-masked
    // acknowledgement, NOT a fabricated full item or child-episode snapshot.
    bool acknowledged_only = false;
    jf::Item item;
};
/* Exactly one mutation attempt: retrying a played POST can increment PlayCount
 * twice. On ambiguous transport failure, re-read rather than guess or replay. */
UserDataResult update_user_data(jf::Client &client, const UserDataRequest &request);
/* Copy only server-returned user data, never invent child episode states. */
inline void reconcile_user_data(jf::Item &destination, const jf::Item &fresh) {
    if (!fresh.user_data_present || fresh.id.empty() || destination.id != fresh.id) return;
    destination.user_data_present = true;
    const unsigned fields = fresh.user_data_fields;
    if (fields & jf::Item::PlayedField) destination.played = fresh.played;
    if (fields & jf::Item::FavoriteField) destination.favorite = fresh.favorite;
    if (fields & jf::Item::PositionField) destination.position_ticks = fresh.position_ticks;
    if (fields & jf::Item::PercentField) destination.played_percent = fresh.played_percent;
    if (fields & jf::Item::UnplayedField) destination.unplayed = fresh.unplayed;
}
}
