/* Shared Settings section/label model; used by input, rendering and host checks. */
#pragma once
#include "ui/settings_screen.h"
#include "app/i18n.h"
namespace ui { namespace settings_layout {
struct Tile { const char *label, *description; int section; SettingsScreen::Row first; };
inline constexpr Tile tiles[] = {
    {"Accounts & Servers", "Accounts, sign out", 0, SettingsScreen::SwitchUser},
    {"Playback", "Audio, video, subtitles", 1, SettingsScreen::Quality},
    {"Discover / Seerr", "Seerr connection", 2, SettingsScreen::SeerrOn},
    {"Live TV", "Providers, groups and VOD settings", 3, SettingsScreen::EmbyLive},
    {"System", "Appearance, updates, server and storage", 7, SettingsScreen::Appearance},
};
inline constexpr int tile_count = (int)(sizeof(tiles)/sizeof(tiles[0]));
/* Section ids remain stable. Appearance is the only System child page. */
inline int section_of(int row)
{
    if (row <= SettingsScreen::SignOut) return 0;
    if (row <= SettingsScreen::Refresh) return 1;
    if (row <= SettingsScreen::SeerrTest) return 2;
    if (row <= SettingsScreen::VODRefresh) return 3;
    if (row <= SettingsScreen::Together) return 4;
    if (row <= SettingsScreen::StorageInfo) return 7;
    if(row<=SettingsScreen::DetailPoster)return 8; // Content display child
    return 4; // grouped Appearance editor rows

}

/* Left/Right changes it (the rest act on Cross). */
inline bool adjustable(int r)
{
    return (r >= SettingsScreen::Quality && r <= SettingsScreen::Refresh) || r == SettingsScreen::SeerrOn ||
           r == SettingsScreen::SeerrAuth || r == SettingsScreen::VODInterval || r == SettingsScreen::CheckUpdates ||
           (r >= SettingsScreen::Theme && r <= SettingsScreen::DescriptionScroll) ||
           (r >= SettingsScreen::SpoilerProtection && r <= SettingsScreen::DetailPoster) ||
           (r >= SettingsScreen::ColourVariant && r <= SettingsScreen::CustomSlot);
}

inline const char *label_of(int row)
{
    const char *const labels[] = {
        T("Bytt bruker eller server"),
        T("Logg ut"),
        T("Maks kvalitet"),
        "Video upscaling",
        T("Undertekster"),
        T("Undertekststørrelse"),
        T("Undertekstbakgrunn"),
        T("Spill neste episode automatisk"),
        "Are you still watching?",
        T("Hopp over intro automatisk"),
        T("Lydforsinkelse"),
        T("Nattmodus"),
        T("HDMI-bitstr\xC3\xB8m"),
        T("Temamusikk"),
        T("Bildefrekvens"),
        "Seerr",
        T("Adresse"),
        T("P\xC3\xA5logging"),
        T("Seerr-konto"),
        T("Test tilkoblingen"),
        "Emby Live TV (automatic)",
        "Xtream server URL",
        "Xtream username",
        "Xtream password",
        "Xtream source status",
        "M3U / M3U8 playlist URL",
        "M3U Live TV status",
        "Unified Live TV",
        "Manage IPTV categories",
        "VOD catalogue status",
        "Xtream movies",
        "Xtream TV shows",
        "M3U movies",
        "VOD automatic refresh",
        "Refresh VOD now",
        "Theme preset",
        "Accent colour",
        "Background colour",
        "Background artwork",
        "Backdrop dimming",
        "Interface text size",
        "Poster and card size",
        "Buttons and panels",
        "Selection outline",
        "Selected-poster enlargement",
        "Reduced motion",
        "Description scrolling",
        "Content display",
        "Save custom theme",
        "Load selected theme",
        "Reset to preset defaults",
        "Apply appearance",
        "Cancel changes",
        T("Se sammen"),
        "Appearance",
        "What's New",
        "Check for updates",
        "Installed version",
        "Connected server",
        "Persistent storage",
        "Hide unwatched episode spoilers",
        "Detail-page title style",
        "Show detail-page poster", "Colour variant", "Button shape", "Preview scene",
        "Compare with last applied", "Custom theme slot", "Rename selected slot", "Restore last applied appearance"};
    static_assert(sizeof(labels)/sizeof(labels[0]) == SettingsScreen::RowCount, "Settings labels must match rows");
    return row >= 0 && row < SettingsScreen::RowCount ? labels[row] : "";
}
}}
