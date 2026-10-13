/* Album cover resolution shared by the music grid and regression checks.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include "jf/jf_client.h"
#include <chrono>
#include <functional>
#include <vector>

namespace ui {
inline double music_art_now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct MusicImage {
    std::string owner, tag, blurhash;
    bool same(const MusicImage &b) const { return owner==b.owner && tag==b.tag; }
};
inline MusicImage music_primary(const jf::Item &item) {
    return {item.primary_tag.empty() || item.primary_owner.empty()?item.id:item.primary_owner, item.primary_tag, item.primary_blurhash};
}
struct MusicArtwork {
    MusicImage listing, primary;
    std::vector<MusicImage> alternatives;
    bool detail_read=false, tracks_read=false;
    unsigned attempts=0;
    double retry_at=0;
    bool accepts(const jf::Item &item) const {
        const auto image=music_primary(item);
        // An abbreviated list has less metadata than item detail. Do not erase
        // resolved images on a watched-state/list refresh. An explicit NEW tag
        // does invalidate the old result so real cover replacements still work.
        return image.tag.empty() || image.same(listing) || image.same(primary);
    }
    MusicImage preferred(const jf::Item &item) const {
        return accepts(item) && !primary.tag.empty() ? primary : music_primary(item);
    }
    bool needs_fetch(const jf::Item &item, bool primary_failed) const {
        if(attempts>=3 || retry_at>music_art_now()) return false;
        return ((!detail_read) && (item.primary_tag.empty() || primary_failed)) ||
               (primary_failed && !tracks_read);
    }
};
inline std::string music_image_url(jf::Client &client,const MusicImage &image) {
    return client.image_url(image.owner,"Primary",image.tag,800,true);
}
inline jf::Item music_display_item(jf::Client &client, const jf::Item &listed,
                                  const MusicArtwork &art,
                                  const std::function<bool(const std::string&)> &failed) {
    jf::Item shown=listed;
    MusicImage image=art.preferred(listed);
    if(art.accepts(listed) && failed(music_image_url(client,image))) {
        // Album/tagged cover first. The track fallback is used only after the
        // preferred image actually failed, not merely while it is downloading.
        for(const auto &candidate:art.alternatives) {
            if(!failed(music_image_url(client,candidate))) { image=candidate; break; }
        }
    }
    shown.primary_owner=image.owner;
    shown.primary_tag=image.tag;
    if(!image.blurhash.empty()) shown.primary_blurhash=image.blurhash;
    return shown;
}
inline MusicArtwork fetch_music_artwork(jf::Client &client, const jf::Item &album,
                                       MusicArtwork result, bool primary_failed,
                                       const std::function<bool()> &current) {
    if(!result.accepts(album)) result=MusicArtwork{};
    if(result.attempts==0) { result.listing=music_primary(album); result.primary=result.listing; }
    ++result.attempts;
    bool failed=false;
    if(!current()) return result;
    if(!result.detail_read) {
        jf::Item detail;
        if(client.item(album.id,&detail) && detail.id==album.id) {
            result.detail_read=true;
            const auto image=music_primary(detail);
            if(!image.tag.empty()) result.primary=image;
        } else failed=true;
    }
    if(!current()) return result;
    if(!result.tracks_read && (primary_failed || result.primary.tag.empty())) {
        bool ok=false;
        const auto tracks=client.children(album.id,"SortName",8,&ok);
        if(ok) {
            result.tracks_read=true;
            result.alternatives.clear();
            const auto add=[&](MusicImage candidate) {
                if(candidate.owner.empty() || candidate.tag.empty() || candidate.same(result.primary)) return;
                for(const auto &existing:result.alternatives) if(existing.same(candidate)) return;
                result.alternatives.push_back(std::move(candidate));
            };
            // Album references from the server rank ahead of an individual track.
            for(const auto &track:tracks) add({track.album_id,track.album_primary_tag,{}});
            for(const auto &track:tracks) add(music_primary(track));
        } else failed=true;
    }
    result.retry_at=failed?music_art_now()+5.0*result.attempts:0;
    return result;
}
}
