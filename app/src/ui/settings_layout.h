/* Shared Settings section/label model; used by input, rendering and host checks. */
#pragma once
#include "ui/settings_screen.h"
#include "app/i18n.h"
namespace ui { namespace settings_layout {
struct Tile { const char *label, *description; int section; SettingsScreen::Row first; };
inline constexpr Tile tiles[] = {
    {N_("Accounts & Servers"), N_("Accounts, sign out"), 0, SettingsScreen::SwitchUser},
    {N_("Playback"), N_("Audio, video, subtitles"), 1, SettingsScreen::Quality},
    {N_("Discover / Seerr"), N_("Seerr connection"), 2, SettingsScreen::SeerrOn},
    {N_("Live TV"), N_("Providers, groups and VOD settings"), 3, SettingsScreen::EmbyLive},
    {N_("System"), N_("Appearance, updates, server and storage"), 7, SettingsScreen::Appearance},
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
           r == SettingsScreen::SeerrAuth || r == SettingsScreen::VODInterval || r == SettingsScreen::CheckUpdates || r == SettingsScreen::Language ||
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
        T("Network buffer"),
        T("Video upscaling"),
        T("Undertekster"),
        T("Undertekststørrelse"),
        T("Undertekstbakgrunn"),
        T("Spill neste episode automatisk"),
        T("Are you still watching?"),
        T("Hopp over intro automatisk"),
        T("Lydforsinkelse"),
        T("Volume boost"),
        T("Nattmodus"),
        T("HDMI-bitstr\xC3\xB8m"),
        T("Temamusikk"),
        T("Bildefrekvens"),
        T("Seerr"),
        T("Adresse"),
        T("P\xC3\xA5logging"),
        T("Seerr-konto"),
        T("Test tilkoblingen"),
        T("Emby Live TV (automatic)"),
        T("Xtream server URL"),
        T("Xtream username"),
        T("Xtream password"),
        T("Xtream source status"),
        T("M3U / M3U8 playlist URL"),
        T("M3U TV guide (XMLTV) URL"),
        T("M3U Live TV status"),
        T("Unified Live TV"),
        T("Manage IPTV categories"),
        T("VOD catalogue status"),
        T("Xtream movies"),
        T("Xtream TV shows"),
        T("M3U movies"),
        T("VOD automatic refresh"),
        T("Refresh VOD now"),
        T("Theme preset"),
        T("Accent colour"),
        T("Background colour"),
        T("Background artwork"),
        T("Backdrop dimming"),
        T("Interface text size"),
        T("Poster and card size"),
        T("Buttons and panels"),
        T("Selection outline"),
        T("Selected-poster enlargement"),
        T("Reduced motion"),
        T("Description scrolling"),
        T("Content display"),
        T("Save custom theme"),
        T("Load selected theme"),
        T("Reset to preset defaults"),
        T("Apply appearance"),
        T("Cancel changes"),
        T("Se sammen"),
        T("Appearance"),
        T("Spr\xC3\xA5k"),
        T("What's New"),
        T("Check for updates"),
        T("Installed version"),
        T("Connected server"),
        T("Persistent storage"),
        T("Hide unwatched episode spoilers"),
        T("Detail-page title style"),
        T("Show detail-page poster"), T("Colour variant"), T("Button shape"), T("Preview scene"),
        T("Compare with last applied"), T("Custom theme slot"), T("Rename selected slot"), T("Restore last applied appearance")};
    static_assert(sizeof(labels)/sizeof(labels[0]) == SettingsScreen::RowCount, "Settings labels must match rows");
    return row >= 0 && row < SettingsScreen::RowCount ? labels[row] : "";
}
}}
