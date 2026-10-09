/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings, opened from the avatar in the top bar: account (switch user,
 * sign out), playback (quality cap, subtitle
 * mode, autoplay, automatic intro skipping), Seerr (on, its address, how it
 * signs in, the session, the network, a connection test), the server, and
 * about. A list in the Apple TV style: value on the right, Left/Right or
 * Cross changes it. Seerr's rows other than "on" show only when it is on.
 */
#pragma once

#include "ui/screen.h"
#include "app/iptv_xtream.h"
#include "app/iptv_categories.h"
#include <memory>
#include <mutex>

#include <string>
#include <vector>

namespace ui {

class SettingsScreen : public Screen {
public:
    enum Row {
        SwitchUser, SignOut,
        Quality, Upscaling, SubMode, SubSize, SubBackground, Autoplay, StillWatching, AutoSkip, AudioDelay, NightMode, Bitstream,
        ThemeMusic,
        SeerrOn, SeerrUrl, SeerrAuth, SeerrAccount, SeerrTest,
        EmbyLive, IPTVServer, IPTVUsername, IPTVPassword, XtreamLive, M3UUrl, IPTVSummary, VODStatus, VODInterval, VODRefresh, IPTVCategories,
        AppLanguage, Theme, Refresh, Together, ServerInfo, About, Updates, AutoDownload, CheckNow, InstallNow, UpdateNotes, RemindLater, UpdateStatus, AppVersion, RowCount
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
    bool animating() const override { return m_animating; }
    bool modal() const override { return m_iptv_page != IPTVRows || m_tiles || m_notes_open; }
    float nav_alpha() const override { return m_scroll.value < 1.f ? 1.f : 0.f; }

private:
    void change(Row r, int dir);
    std::string value(Row r) const;
    bool shown(int r) const;
    struct IPTVCatalogState {
        std::mutex mutex;
        iptv_xtream::Catalog catalog;
        std::string error;
        bool loading = false;
    };
    enum IPTVPage { IPTVRows, IPTVCategoryList, IPTVChannels };
    IPTVPage m_iptv_page = IPTVRows;
    iptv_xtream::Credentials m_iptv_creds;
    iptv_categories::Store m_iptv_store;
    std::shared_ptr<IPTVCatalogState> m_iptv_catalog = std::make_shared<IPTVCatalogState>();
    int m_iptv_category = 0, m_iptv_channel = 0;
    std::string m_iptv_search;
    std::vector<size_t> iptv_filtered_channels(const iptv_xtream::Catalog &catalog) const;
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
