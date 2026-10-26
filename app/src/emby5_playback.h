/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Playback of a Emby item on the native player (the Nuvio Player on the
 * EVO engine): negotiates the stream with PlaybackInfo, hands the player a
 * request built from the item, reports start/progress/stop to Emby, and
 * follows "next episode" from one episode (or album track) to the next.
 */
#pragma once

#include "jf/jf_client.h"

#include <vector>

/* Blocks for the whole playback (and any episodes played after it). Returns
 * false with *error when the item could not be started. A music track plays
 * on through its album; shuffle: the album in random order from this track. */
bool emby5_play(jf::Client &client, const jf::Item &item, std::string *error, bool shuffle = false);
/* Compatibility wait for callers. Stop reporting now completes before the
 * playback chain returns; there is no detached final-report worker. */
void emby5_wait_reports(int max_ms);
/* A theme song, quietly and unreported (headless: the caller sets that). */
bool emby5_play_theme(jf::Client &client, const jf::Item &song);
/* The sound and stream settings for a playback (Settings › Playback): full
 * volume, normal speed, night mode, volume boost, HDMI bitstream and the
 * network buffer. Every playback calls it before it opens. */
void emby5_playback_prepare();

/* The music queue (Now playing's queue sheet). upcoming/indices: what plays after
 * the current track, in order, and each one's index in the queue. */
void emby5_music_state(std::vector<jf::Item> *upcoming, std::vector<int> *indices, int *current, bool *shuffle,
                        int *repeat);
void emby5_music_set_shuffle(bool on);
void emby5_music_set_repeat(int mode);   /* 0 off, 1 all, 2 one */
void emby5_music_jump(int queue_index);  /* plays next; then send Next to end the current one */
/* A queue (a playlist, an Instant Mix, what a phone sent): plays from queue[start]
 * through the rest. */
bool emby5_play_queue(jf::Client &client, const std::vector<jf::Item> &queue, size_t start, std::string *error);

/* Subtitle search while something plays (the player's "Søk etter undertekster"):
 * Emby's subtitle plugins search, the server downloads the choice, and it
 * is added to the player as an external track. All of it runs off the
 * player's thread; the player polls the state. */
namespace emby5_subs {
enum State { Idle, Busy, Done, Failed };
bool available();                                 /* this title, this account, this server */
void search(const std::string &language);         /* three-letter code */
State results(std::vector<jf::RemoteSubtitle> *out, std::string *language);
void download(const jf::RemoteSubtitle &s);
/* Done: *track is the player's new subtitle track (to select). */
State download_state(int *track);
} // namespace emby5_subs

extern "C" {
/* Called by the player through the bridge stand-ins (emby5_bridge.cpp). */
void emby5_playback_progress(double position, double duration);
void emby5_playback_state(double position, double duration, bool paused, bool started);
void emby5_playback_finished(const char *result_json);
}
