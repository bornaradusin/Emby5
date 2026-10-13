/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "user_data_update.h"
namespace emby5 {
UserDataResult update_user_data(jf::Client &c, const UserDataRequest &r)
{
    UserDataResult out;
    if (r.id.empty() || (!r.favorite_set && !r.played_set && !r.resume_cleared)) return out;
    out.item.id = r.id;
    out.item.user_data_fields = 0;
    bool ok = true;
    auto accept = [&](jf::Item dto, unsigned field) {
        // Apply returned state, even when it differs from the requested value.
        // Missing fields on HTTP success acknowledge ONLY the requested change.
        if (!(dto.user_data_fields & field)) {
            dto.id = r.id; dto.user_data_present = true;
            dto.user_data_fields |= field;
            if (field == jf::Item::FavoriteField) dto.favorite = r.favorite;
            if (field == jf::Item::PlayedField) dto.played = r.played;
            out.acknowledged_only = true;
        }
        reconcile_user_data(out.item, dto);
        out.item.user_data_fields |= dto.user_data_fields;
    };
    if (r.favorite_set) {
        jf::Item dto;
        ok = c.set_favorite(r.id, r.favorite, &dto);
        if (ok) accept(dto, jf::Item::FavoriteField);
    }
    if (ok && r.played_set) {
        jf::Item dto;
        ok = c.set_played(r.id, r.played, &dto);
        if (ok) accept(dto, jf::Item::PlayedField);
    }
    if (ok && r.resume_cleared) {
        ok = c.clear_position(r.id);
        if (ok) {
            out.item.user_data_present = true;
            out.item.user_data_fields |= jf::Item::PositionField;
            out.item.position_ticks = 0;
            out.acknowledged_only = true;
        }
    }
    out.accepted = ok;
    if (!ok) {
        // Never retry a write whose response may have been lost. Read the user's
        // item to reconcile; an error must not be promoted to a local success.
        jf::Item fresh;
        if (c.item(r.id, &fresh) && fresh.id == r.id && fresh.user_data_present) out.item = fresh;
    }
    out.refreshed = out.item.user_data_present;
    if (out.refreshed) {
        const auto f = out.item.user_data_fields;
        out.matched = (!r.favorite_set || ((f & jf::Item::FavoriteField) && out.item.favorite == r.favorite)) &&
                      (!r.played_set || ((f & jf::Item::PlayedField) && out.item.played == r.played)) &&
                      (!r.resume_cleared || ((f & jf::Item::PositionField) && out.item.position_ticks == 0));
    }
    return out;
}
}
