/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings, opened from the avatar in the top bar: account (switch user,
 * sign out), playback (quality cap, subtitle
 * mode, autoplay, automatic intro skipping), Seerr (on, its address, how it
 * signs in, the session, the network, a connection test), the server, and
 * storage information. A list in the Apple TV style: value on the right, Left/Right or
 * Cross changes it. Seerr's rows other than "on" show only when it is on.
 */
#pragma once

#include "ui/screen.h"
#include "app/iptv_xtream.h"
#include "app/iptv_categories.h"
#include "app/appearance.h"
#include "ui/hold_navigation.h"
#include "ui/choice_sheet.h"
#include <memory>
#include <mutex>

#include <string>
#include <vector>

namespace ui {

class SettingsScreen : public Screen {
public:
    enum Row {
        SwitchUser, SignOut,
        Quality, NetBuffer, Upscaling, SubMode, SubSize, SubBackground, Autoplay, StillWatching, AutoSkip, AudioDelay,
        VolumeBoost, NightMode, Bitstream,
        ThemeMusic, Refresh,
        SeerrOn, SeerrUrl, SeerrAuth, SeerrAccount, SeerrTest,
        EmbyLive, IPTVServer, IPTVUsername, IPTVPassword, XtreamLive, M3UUrl, M3UGuide, M3UStatus, IPTVSummary, IPTVCategories,
        VODStatus, VODMoviesStatus, VODShowsStatus, VODM3UStatus, VODInterval, VODRefresh,
        Theme, Accent, Background, BackgroundArt, BackdropDim, UiTextSize, PosterSize,
        ControlStyle, FocusOutline, PosterEnlarge, ReducedMotion, DescriptionScroll,
        ContentDisplay, SaveMyTheme, LoadMyTheme, ResetAppearance, ApplyAppearance, CancelAppearance, Together,
        Appearance, Language, UpdateNotes, CheckUpdates, AppVersion, ServerInfo, StorageInfo,
        SpoilerProtection, TitleStyle, DetailPoster, ColourVariant, ButtonShape,
        PreviewScene, CompareAppearance, CustomSlot, RenameMyTheme, RestoreAppearance, RowCount
    };
    explicit SettingsScreen(jf::Client &client) : m_client(client) {}
    ~SettingsScreen() override;   /* the keyboard's callback points here */

    void set_server_info(const std::string &name, const std::string &version)
    {
        m_server_name = name;
        m_server_version = version;
    }
    void activate() override;
    Action input(uint32_t pressed) override;
    void draw(double now, float dt) override;
    bool animating() const override { return m_animating || m_iptv_page!=IPTVRows; }
    void update_hold(uint32_t held, double now);
    bool accelerating(double now) const {return (m_iptv_page==IPTVChannels || m_iptv_page==IPTVProviderGroups) && m_channel_hold.accelerating(now);}
    bool modal() const override { return m_iptv_page != IPTVRows || m_tiles || m_notes_open || m_appearance.active; }
    float nav_alpha() const override { return m_scroll.value < 1.f ? 1.f : 0.f; }

private:
    friend struct SettingsRegressionAccess;
    settings::AppearanceEditor m_appearance;
    HoldNavigation m_channel_hold;
    std::string m_appearance_note;
    void begin_appearance();
    void end_appearance(bool apply);
    void draw_appearance(float dt);
    Action input_appearance(uint32_t pressed);
    std::vector<Row> appearance_rows() const;
    std::string appearance_help(Row r) const;
    void draw_appearance_preview();
    void draw_appearance_confirmation();
    int m_appearance_page=-1, m_appearance_index=0, m_appearance_root_index=0;
    int m_preview_scene=0;
    bool m_compare_appearance=false;
    enum AppearanceConfirm { ConfirmNone, ConfirmExit, ConfirmOverwrite, ConfirmReset };
    AppearanceConfirm m_appearance_confirm=ConfirmNone;
    int m_confirmation_choice=0;
    void change(Row r, int dir);
    std::string value(Row r) const;
    bool shown(int r) const;
    struct IPTVCatalogState {
        std::mutex mutex;
        std::shared_ptr<const iptv_xtream::Catalog> catalog;
        std::string error;
        bool loading = false;
    };
    enum IPTVPage { IPTVRows, IPTVCategoryList, IPTVChannels, IPTVProviderGroups };
    uint64_t m_iptv_generation=~uint64_t(0),m_iptv_content_generation=~uint64_t(0);
    struct ProviderGroup {std::string id,name;std::vector<std::string> channels;bool partial=false;};
    std::vector<ProviderGroup> m_provider_groups;
    std::vector<size_t> m_provider_filtered;
    std::string m_provider_query,m_group_note,m_import_destination;
    int m_provider_index=0;
    std::vector<std::string> m_import_channels;
    ChoiceSheet m_group_menu;
    bool m_import_confirm=false;
    uint64_t m_import_generation=0;
    void iptv_filter_groups();
    void iptv_open_group_actions();
    IPTVPage m_iptv_page = IPTVRows;
    iptv_xtream::Credentials m_iptv_creds;
    iptv_categories::Store m_iptv_store;
    std::shared_ptr<IPTVCatalogState> m_iptv_catalog = std::make_shared<IPTVCatalogState>();
    int m_iptv_category = 0, m_iptv_channel = 0;
    std::string m_iptv_search;
    std::shared_ptr<const iptv_xtream::Catalog> m_visible_catalog;
    std::string m_filtered_query;
    std::vector<size_t> m_filtered_channels;
    std::vector<std::string> m_channel_search_names;
    void iptv_sync_catalog();
    void iptv_move_channel(int delta);
    void iptv_edit(Row r);
    void iptv_open_categories();
    void iptv_draw_categories();
    void iptv_category_input(uint32_t pressed);
    void seerr_account();            /* Cross on the Seerr account: sign in or out */

    std::string m_seerr_email;       /* a local Seerr account: the e-mail, then the password */
    bool m_want_password = false;
    bool m_notes_open = false;
    int m_notes_scroll = 0;
    bool m_signout_armed = false;    /* Seerr's account row: the first ✕ asks, the second signs out */

    jf::Client &m_client;
    int m_row = 0;
    bool m_tiles = true;
    int m_tile = 0;
    int m_section = -1;
    Anim m_scroll;
    Lifts m_lifts;
    Drop m_drop;                        /* the focus */
    bool m_animating = false;
    std::string m_server_name, m_server_version;
};

} // namespace ui
