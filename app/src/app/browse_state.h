/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "jf/jf_client.h"
#include "app/iptv_xtream.h"
#include "app/stable_identity.h"
#include <string>
#include <vector>
namespace browse_state {
// Scopes are opaque hashes. No tokens or stream URLs are stored in this file.
void set_user(int user);
std::string scope(const jf::Client &client);
std::string provider_scope(const jf::Client &client);
std::string opaque_key(const std::string &value);
struct LibraryPrefs {
    int sort=0, decade=0;
    bool unplayed=false, favorites=false;
    std::string genre;
};
LibraryPrefs library(const std::string &scope,const std::string &key);
void save_library(const std::string &scope,const std::string &key,const LibraryPrefs &prefs);
inline std::string channel_key(const iptv_xtream::Channel &c){return c.source=="M3U"?emby5::playlist_item_key(c.extension):c.id;}
std::vector<std::string> pins(const std::string &scope);
void toggle_pin(const std::string &scope,const std::string &group);
struct Progress {
    std::string source,id,parent,title;
    bool episode=false;
    double position=0, duration=0;
};
std::vector<Progress> progress(const std::string &scope);
double position(const std::string &scope,const Progress &item);
void save_progress(const std::string &scope,Progress item,bool ended);
void begin_playback(const std::string &scope,const Progress &item);
void report_playback(double position,double duration);
void finish_playback(const char *json);
void clear_playback();
bool last_playback_started();
}
