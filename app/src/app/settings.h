/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings. Subtitle mode and autoplay live on the Emby account;
 * console-side preferences (quality
 * cap, automatic intro skipping, language, subtitle look) live in
 * the Emby5 persistent data store.
 */
#pragma once

#include "jf/jf_client.h"

namespace settings {

struct Local {
    int upscale_mode = 0;       /* 0 Off, 1 Auto, 2 FSR 1, 3 Anime4K */
    int max_mbps = 0;            /* 0 = no cap (direct play whatever the network allows) */
    bool auto_skip_intro = false;
    bool autoplay_next_override_valid = false;
    bool autoplay_next_override = false;
    int theme = 0;             /* Glass or one of 30 native AGC palette presets */
    int still_watching = 0;  /* 0 off, 1 after 3 autoplayed episodes, 2 after 2 hours */
    int language = 0;            /* i18n::Choice: 0 follow the PS5, 1 Norsk, 2 English */
    /* How text subtitles look (set in Innstillinger or in the player). */
    int sub_size = 100;          /* % */
    float sub_offset = 0;        /* % of the height, lift from the bottom */
    float sub_background = 0;    /* 0..1 box behind the text */
    bool sub_outline = true;
    int sub_color = 0xffffff;  /* subtitle text colour */
    bool refresh_120 = true;
    int audio_delay_ms = 0;      /* the sound system's delay: the picture waits this long (A/V sync) */
    bool night_mode = false;     /* compress loud and quiet together, dialogue lifted */
    bool hdmi_bitstream = false; /* Dolby Digital (Plus) and DTS to the TV/receiver undecoded (jelly5_bitstream) */
    bool theme_music = true;     /* a title's theme song, quietly, on its page */
    bool auto_download_updates = false; /* requires update checking to be enabled */
    bool check_updates = false;  /* opt-in: ask GitHub for a newer release at start */     /* the display at 120 Hz when it can (smoother menus, 24p without judder) */
};

struct All {
    Local local;
    jf::UserPrefs server;
};

/* The current settings (thread-safe copy). */
All get();
void load_local();
void set_local(const Local &l);
/* Reads the account's preferences (after sign-in). */
void load_server(jf::Client &c);
/* Changes the account's preferences (written in the background). */
void set_server(jf::Client &c, const jf::UserPrefs &p);

} // namespace settings
