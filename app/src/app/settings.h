/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings. Subtitle mode and autoplay live on the Emby account;
 * console-side preferences (quality
 * cap, automatic intro skipping, subtitle look) live in
 * the Emby5 persistent data store.
 */
#pragma once

#include "jf/jf_client.h"
#include <array>

namespace settings {
constexpr int kThemePresetCount = 32;
constexpr int kDefaultTheme = 31; // Emby5; IDs 0..30 are unchanged.
constexpr int kThemeBackgroundCount = 13; // existing 0..3 plus nine colour backgrounds


// Appearance is stored separately from playback/account preferences. The editor
// previews this small value object without writing settings or changing a server.
struct Appearance {
    int theme = kDefaultTheme;
    int colour_variant = 0;     // 0 preset, 1 neutral, 2 blue, 3 teal, 4 purple, 5 amber
    int button_shape = 0;       // 0 preset, 1 square, 2 subtle, 3 round, 4 pill, 5 chamfer
    int theme_accent = 0;
    int theme_background = 0;
    int ui_text_percent = 100;
    int poster_size = 1;
    bool background_artwork = true;
    int backdrop_dim = 0;       // 0 = preset/default, 1..4 = 25/50/75/90 percent
    int control_style = 0;      // 0 preset, 1 solid, 2 glass
    bool strong_focus = false;
    bool poster_enlarge = true;
    bool reduced_motion = false;
    int description_scroll = 2; // 0 off, 1 slow, 2 normal
    bool spoiler_protection = false;
    bool title_as_text = false;
    bool detail_poster = false; // preserve the previous backdrop-only detail layout
};

struct SavedAppearance {
    bool valid = false;
    std::string name;
    Appearance appearance;
};
constexpr int kCustomThemes = 3;

struct Local : Appearance {
    std::array<SavedAppearance, kCustomThemes> custom_themes{};
    int upscale_mode = 0;       /* 0 Off, 1 Auto, 2 FSR 1, 3 Anime4K */
    int max_mbps = 0;            /* 0 = no cap (direct play whatever the network allows) */
    bool auto_skip_intro = false;
    bool autoplay_next_override_valid = false;
    bool autoplay_next_override = false;
    bool my_theme_valid = false;
    Appearance my_theme;        // saved slot survives switching built-in presets
    int still_watching = 0;  /* 0 off, 1 after 3 autoplayed episodes, 2 after 2 hours */
    /* How text subtitles look (set in Innstillinger or in the player). */
    int sub_size = 100;          /* % */
    float sub_offset = 0;        /* % of the height, lift from the bottom */
    float sub_background = 0;    /* 0..1 box behind the text */
    bool sub_outline = true;
    int sub_color = 0xffffff;  /* subtitle text colour */
    bool refresh_120 = true;
    int audio_delay_ms = 0;      /* the sound system's delay: the picture waits this long (A/V sync) */
    bool night_mode = false;     /* compress loud and quiet together, dialogue lifted */
    int volume_boost = 0;        /* 0 off; 1-4: +50, +100, +200, +300 % (volume_boost_factor) */
    bool playback_info = false;  /* the player's L3 panel stays as it was left, for the next video */
    int net_buffer = 0;          /* read-ahead of a network stream; 0-3: 30 s, 1, 2, 5 min (net_buffer_seconds) */
    bool hdmi_bitstream = false; /* Dolby Digital (Plus) and DTS to the TV/receiver undecoded (emby5_bitstream) */
    bool check_updates = true;  /* automatic release checks/notifications; never installation */
    std::string language;        /* "" follows the PS5's language, else an i18n::language_code */
    bool theme_music = true;     /* a title's theme song, quietly, on its page */
};

struct All {
    Local local;
    jf::UserPrefs server;
};

/* The current settings (thread-safe copy). */
All get();
// Rendering reads the preview when an Appearance editor is open. It is never
// persisted by this function; nullptr restores the saved appearance immediately.
Appearance get_appearance();
void preview_appearance(const Appearance *appearance);
void load_local();
void set_local(const Local &l);
/* Reads the account's preferences (after sign-in). */
void load_server(jf::Client &c);
/* Changes the account's preferences (written in the background). */
void set_server(jf::Client &c, const jf::UserPrefs &p);

/* Volume boost as a gain: Off, +50, +100, +200, +300 %. */
inline float volume_boost_factor(int level)
{
    static const float k[] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
    return level >= 0 && level <= 4 ? k[level] : 1.0f;
}
/* The network buffer setting in seconds: 30 s, 1, 2 or 5 minutes. */
inline int net_buffer_seconds(int level)
{
    static const int k[] = {30, 60, 120, 300};
    return level >= 0 && level <= 3 ? k[level] : 30;
}

} // namespace settings
