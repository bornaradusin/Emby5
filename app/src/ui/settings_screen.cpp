#include <cctype>
/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/settings_screen.h"
#include "app/iptv_live.h"
#include "app/iptv_vod.h"
#include "app/iptv_m3u.h"
#include "ui/theme_presets.h"
#include "evo_audio_out.h"

#include "app/settings.h"
#include "app/seerr_service.h"
#include "evo_agc_runtime.h"
#include "app/syncplay.h"
#include "app/i18n.h"
#include "nuvio_input.h"
#include "platform/ime.h"

#include <algorithm>
#include <thread>

#ifndef EMBY5_VERSION
#define EMBY5_VERSION "0.0.1"
#endif

namespace ui {
namespace {

const int kQualities[] = {0, 120, 80, 60, 40, 20, 10, 8, 4};
constexpr int kNumQualities = 9;

struct Mode {
    const char *code, *name;
};
const Mode kModes[] = {{"Default", "Standard"},
                       {"Smart", "Smart"},
                       {"Always", "Alltid"},
                       {"OnlyForced", "Bare tvungne"},
                       {"HearingImpaired", "Hearing impaired (SDH)"},
                       {"None", "Av"}};
constexpr int kNumModes = sizeof(kModes) / sizeof(kModes[0]);

const char *kHeaders[] = {"Konto", "Avspilling", "Seerr", "Live TV", "Generelt", "About", "VOD"};

/* Its card: 0 account, 1 playback, 2 Seerr, 3 general, 4 about (no header). */
int section_of(int row)
{
    return row <= SettingsScreen::SignOut      ? 0
           : row <= SettingsScreen::ThemeMusic ? 1
           : row <= SettingsScreen::SeerrTest  ? 2
           : row >= SettingsScreen::VODStatus && row <= SettingsScreen::VODRefresh ? 6
           : row <= SettingsScreen::IPTVCategories ? 3
           : row == SettingsScreen::About      ? 4
                                               : 4;
}

/* Left/Right changes it (the rest act on Cross). */
bool adjustable(int r)
{
    return (r >= SettingsScreen::Quality && r <= SettingsScreen::ThemeMusic) || r == SettingsScreen::SeerrOn ||
           r == SettingsScreen::SeerrAuth || r == SettingsScreen::VODInterval ||
           (r >= SettingsScreen::AppLanguage && r <= SettingsScreen::Updates);
}

const char *auth_name(seerr_service::Auth a)
{
    switch (a) {
    case seerr_service::Auth::EmbyPassword: return T("Emby-passord");
    case seerr_service::Auth::Local: return T("Seerr-konto (e-post)");
    default: return T("Automatisk (Quick Connect)");
    }
}

const char *label_of(int row)
{
    const char *const labels[] = {T("Bytt bruker eller server"),
                                         T("Logg ut"),
                                         T("Maks kvalitet"),
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
                                         "Unified Live TV",
                                         "VOD catalogue status",
                                         "VOD automatic refresh",
                                         "Refresh VOD now",
                                         "Manage IPTV categories",
                                         T("Språk"),
                                         "Theme",
                                         T("Bildefrekvens"),
                                         T("Se etter oppdateringer"),
                                         T("Se sammen"),
                                         "Server",
                                         T("Om Emby5")};
    return labels[row];
}

template <class T, size_t N> int index_of(const T (&list)[N], const std::string &code)
{
    for (size_t i = 0; i < N; i++)
        if (code == list[i].code)
            return (int)i;
    return 0;
}

} // namespace

SettingsScreen::~SettingsScreen() { ime::cancel(); }

void SettingsScreen::activate()
{
    m_row = 0;
    m_tiles = true;
    m_tile = 0;
    m_section = -1;
    m_scroll.snap(0);
    ime::init();
    m_iptv_page = IPTVRows;
    m_iptv_creds = {};
    iptv_xtream::load_credentials(&m_iptv_creds);
    m_iptv_store.load(m_iptv_creds.server + "|" + m_iptv_creds.username);
}

bool SettingsScreen::shown(int r) const
{
    /* SyncPlay is Jellyfin-specific and remains hidden in the Emby port.
     * Seerr is supported by current Seerr releases for Emby, so expose the
     * existing native Seerr settings and request/discovery UI. */
    if (r == Together)
        return false;
    return true;
}

void SettingsScreen::seerr_account()
{
    using namespace seerr_service;
    const Snapshot s = snapshot();
    if (s.state == State::Connecting)
        return;
    if (s.state == State::Ready) {   /* signing out also ends automatic sign-in: ask first */
        if (!m_signout_armed) {
            m_signout_armed = true;
            return;
        }
        m_signout_armed = false;
        sign_out();
        return;
    }
    switch (config().auth) {
    case Auth::QuickConnect:   /* ✕ here approves Quick Connect for the address shown */
        if (s.why == Why::NeedApproval)
            approve_quick_connect();
        else
            reconnect();
        break;
    case Auth::EmbyPassword:
        ime::request(ime::Kind::Password, T("Emby-passord for ") + m_client.user_name(), "",
                     [](const std::string &pw) { seerr_service::sign_in("", pw); });
        break;
    case Auth::Local:   /* the e-mail first; the password once the keyboard has closed */
        ime::request(ime::Kind::Text, T("E-post for Seerr-kontoen"), m_seerr_email, [this](const std::string &e) {
            m_seerr_email = e;
            m_want_password = !e.empty();
        });
        break;
    default:
        break;
    }
}

std::string SettingsScreen::value(Row r) const
{
    const settings::All s = settings::get();
    switch (r) {
    case SwitchUser: return m_client.user_name();
    case EmbyLive: {
        const auto live=iptv_live::snapshot();
        if (live.loading && !live.emby_count) return "Checking Emby Live TV...";
        return live.emby_count ? std::to_string(live.emby_count)+" channels - connected" : "No channels available";
    }
    case XtreamLive: {
        const auto live=iptv_live::snapshot();
        if (live.loading && !live.xtream_count) return "Checking Xtream...";
        return live.xtream_count ? std::to_string(live.xtream_count)+" channels - connected" : "No channels available";
    }
    case IPTVSummary: {
        const auto live=iptv_live::snapshot();
        const unsigned count=live.emby_count+live.xtream_count+live.m3u_count;
        return count ? std::to_string(count)+" channels - Live TV visible" : "No channels - Live TV hidden";
    }
    case VODInterval: {
        const int hours=iptv_vod::refresh_interval_hours();
        return hours==0 ? "Manual only" : "Every "+std::to_string(hours)+" hours";
    }
    case VODRefresh: return "X to refresh catalogue";
    case VODStatus: {
        const auto vod=iptv_vod::snapshot();
        const std::string counts=std::to_string(vod.movie_count)+" movies, "+std::to_string(vod.show_count)+" TV shows";
        return vod.loading ? counts+" - loading..." : counts+(vod.movie_count==0&&vod.show_count==0?" - VOD hidden":" - ready");
    }
    case M3UUrl: {const auto url=iptv_m3u::playlist_url();return url.empty()?"X to enter":url;}
    case IPTVServer: return m_iptv_creds.server.empty() ? "X to enter" : m_iptv_creds.server;
    case IPTVUsername: return m_iptv_creds.username.empty() ? "X to enter" : m_iptv_creds.username;
    case IPTVPassword: return m_iptv_creds.password.empty() ? "X to enter" : "********";
    case IPTVCategories: return std::to_string(m_iptv_store.categories().size()) + " categories - X to manage";
    case Quality:
        return s.local.max_mbps == 0 ? T("Automatisk (maks)") : std::to_string(s.local.max_mbps) + " Mbit/s";
    case SubMode: return T(kModes[index_of(kModes, s.server.subtitle_mode)].name);
    case Theme: return theme_name(s.local.theme);
    case AppLanguage: {   /* each language in its own name */
        if (s.local.language > i18n::Auto)
            return i18n::choice_name(s.local.language);
        return std::string(T("Automatisk")) + " (" + i18n::choice_name((int)i18n::lang() + 1) + ")";
    }
    case SubSize: return std::to_string(s.local.sub_size) + " %";
    case SubBackground:
        return s.local.sub_background < 0.05f ? std::string(T("Av"))
                                              : std::to_string((int)(s.local.sub_background * 100 + 0.5f)) + " %";
    case Autoplay: return s.server.autoplay_next ? T("På") : T("Av");
    case StillWatching: return s.local.still_watching == 1 ? "After 3 episodes" :
                              s.local.still_watching == 2 ? "After 2 hours" : "Off";
    case AutoSkip: return s.local.auto_skip_intro ? T("På") : T("Av");
    case NightMode: return s.local.night_mode ? T("På") : T("Av");
    case Bitstream:   /* night mode needs the sound decoded here, so it wins */
        return !s.local.hdmi_bitstream ? T("Av") : s.local.night_mode ? T("Av med nattmodus") : T("På");
    case ThemeMusic: return s.local.theme_music ? T("På") : T("Av");
    case Updates: return s.local.check_updates ? T("På") : T("Av");
    case AudioDelay:
        return s.local.audio_delay_ms == 0 ? std::string(T("Ingen"))
                                           : (s.local.audio_delay_ms > 0 ? "+" : "") + std::to_string(s.local.audio_delay_ms) + " ms";
    case Refresh:
        if (!evo_agc_runtime_supports_120hz())
            return T("60 Hz (TV-en har ikke 120 Hz)");
        return s.local.refresh_120 ? "120 Hz" : "60 Hz";
    case Together: return syncplay::active() ? syncplay::group_name() : std::string(T("Av"));
    case ServerInfo: return m_server_name.empty() ? m_client.server() : m_server_name + "  \xC2\xB7  " + m_server_version;
    case About: return std::string(T("Versjon ")) + EMBY5_VERSION;
    case SeerrOn: return seerr_service::config().enabled ? T("P\xC3\xA5") : T("Av");
    case SeerrUrl: {
        const std::string u = seerr_service::config().url;
        return u.empty() ? std::string(T("Ikke angitt")) : u;
    }
    case SeerrAuth: return auth_name(seerr_service::config().auth);
    case SeerrAccount: {
        using namespace seerr_service;
        const Snapshot sn = snapshot();
        switch (sn.state) {
        case State::Connecting: return T("Kobler til \xE2\x80\xA6");
        case State::Ready:
            return m_signout_armed ? std::string(T("Trykk \xE2\x9C\x95 igjen for å logge ut"))
                                   : sn.user.name + "  \xC2\xB7  " + T("\xE2\x9C\x95 logg ut");
        case State::Unreachable: return T("Svarer ikke \xE2\x80\x93 pr\xC3\xB8ver igjen");
        case State::SignedOut:
            if (sn.why == Why::WrongPassword)
                return T("Feil brukernavn eller passord");
            if (sn.why == Why::MethodOff)
                return T("Seerr tillater ikke denne påloggingen \xE2\x80\x93 velg en annen");
            if (sn.why == Why::NotInSeerr)
                return T("Brukeren finnes ikke i Seerr \xE2\x80\x93 be administratoren importere den");
            if (sn.why == Why::NeedApproval)
                return T("\xE2\x9C\x95 godkjenn Quick Connect for denne adressen");
            if (sn.why == Why::AutoFailed)
                return T("Automatisk p\xC3\xA5logging mislyktes \xE2\x80\x93 velg passord");
            return T("Ikke p\xC3\xA5logget \xE2\x80\x93 \xE2\x9C\x95 for \xC3\xA5 logge p\xC3\xA5");
        default: return std::string();
        }
    }
    case SeerrTest: {
        const seerr_service::Snapshot sn = seerr_service::snapshot();
        if (sn.testing)
            return T("Tester \xE2\x80\xA6");
        return sn.test.empty() ? std::string(T("\xE2\x9C\x95 for \xC3\xA5 teste")) : sn.test;
    }
    default: return std::string();
    }
}

void SettingsScreen::change(Row r, int dir)
{
    settings::All s = settings::get();
    auto cycle = [dir](int i, int n) { return ((i + dir) % n + n) % n; };
    switch (r) {
    case VODInterval: {
        const int intervals[]={0,12,24,48,168};
        int i=2;
        for(int j=0;j<5;++j)if(intervals[j]==iptv_vod::refresh_interval_hours())i=j;
        iptv_vod::set_refresh_interval_hours(intervals[cycle(i,5)]);
        break;
    }
    case Quality: {
        int i = 0;
        for (int k = 0; k < kNumQualities; k++)
            if (kQualities[k] == s.local.max_mbps)
                i = k;
        s.local.max_mbps = kQualities[cycle(i, kNumQualities)];
        settings::set_local(s.local);
        break;
    }
    case StillWatching:
        s.local.still_watching = cycle(s.local.still_watching, 3);
        settings::set_local(s.local);
        break;
    case AutoSkip:
        s.local.auto_skip_intro = !s.local.auto_skip_intro;
        settings::set_local(s.local);
        break;
    case NightMode:
        s.local.night_mode = !s.local.night_mode;
        settings::set_local(s.local);
        evo_audio_set_night(s.local.night_mode);
        break;
    case Bitstream:   /* from the next playback */
        s.local.hdmi_bitstream = !s.local.hdmi_bitstream;
        settings::set_local(s.local);
        break;
    case ThemeMusic:
        s.local.theme_music = !s.local.theme_music;
        settings::set_local(s.local);
        break;
    case Updates:
        s.local.check_updates = !s.local.check_updates;
        settings::set_local(s.local);
        break;
    case AudioDelay:   /* 20 ms steps: a soundbar's delay is typically 40-200 ms */
        s.local.audio_delay_ms = std::max(-500, std::min(500, s.local.audio_delay_ms + dir * 20));
        settings::set_local(s.local);
        break;
    case SubSize:
        s.local.sub_size = std::max(50, std::min(200, s.local.sub_size + dir * 10));
        settings::set_local(s.local);
        break;
    case SubBackground: {   /* Av, 25, 50, 75 % */
        const int i = (int)(s.local.sub_background * 4 + 0.5f);
        s.local.sub_background = (float)cycle(i, 4) / 4.f;
        settings::set_local(s.local);
        break;
    }
    case Refresh:
        if (!evo_agc_runtime_supports_120hz())
            break;
        s.local.refresh_120 = !s.local.refresh_120;
        settings::set_local(s.local);
        evo_agc_runtime_set_120hz(s.local.refresh_120 ? 1 : 0);
        break;
    case Theme:
        s.local.theme = cycle(s.local.theme, kThemeCount);
        settings::set_local(s.local);
        break;
    case AppLanguage:
        s.local.language = cycle(s.local.language, i18n::ChoiceCount);   /* Automatisk, then each language */
        settings::set_local(s.local);
        i18n::set_choice(s.local.language);
        break;
    case SubMode:
        s.server.subtitle_mode = kModes[cycle(index_of(kModes, s.server.subtitle_mode), kNumModes)].code;
        settings::set_server(m_client, s.server);
        break;
    case Autoplay:
        s.server.autoplay_next = !s.server.autoplay_next;
        settings::set_server(m_client, s.server);
        break;
    case SeerrOn: {   /* turned on with no address yet: the suggestion */
        seerr_service::Config c = seerr_service::config();
        c.enabled = !c.enabled;
        if (c.enabled && c.url.empty())
            c.url = seerr_service::suggested_url();
        seerr_service::set_config(c);
        break;
    }
    case SeerrAuth: {
        seerr_service::Config c = seerr_service::config();
        c.auth = (seerr_service::Auth)cycle((int)c.auth, (int)seerr_service::Auth::Count);
        seerr_service::set_config(c);
        break;
    }
    default:
        break;
    }
}

Action SettingsScreen::input(uint32_t p)
{
    if (ime::active()) return {};
    if (m_iptv_page != IPTVRows) {
        iptv_category_input(p);
        return {};
    }
    Action a;
    if (m_tiles) {
        if(p & NUVIO_BTN_LEFT) m_tile=std::max(0,m_tile-1);
        if(p & NUVIO_BTN_RIGHT) m_tile=std::min(5,m_tile+1);
        if(p & NUVIO_BTN_UP) {if(m_tile>=2)m_tile-=2; else a.kind=Action::ToNav;}
        if(p & NUVIO_BTN_DOWN) m_tile=std::min(5,m_tile+2);
        if(p & NUVIO_BTN_CIRCLE) a.kind=Action::ToNav;
        if(p & NUVIO_BTN_CROSS) {
            const int first[]={SwitchUser,Quality,SeerrOn,EmbyLive,VODStatus,AppLanguage};
            m_section=m_tile==4?6:(m_tile==5?4:m_tile);
            m_row=first[m_tile];m_scroll.snap(0);m_tiles=false;
        }
        return a;
    }
    if (!(p & NUVIO_BTN_CROSS))
        m_signout_armed = false;   /* moved on: the account row asks again */
    if (p & NUVIO_BTN_DOWN) {
        int r = m_row + 1;
        while (r < RowCount && (!shown(r) || section_of(r)!=m_section))
            r++;
        if (r < RowCount && section_of(r)==m_section)
            m_row = r;
    } else if (p & NUVIO_BTN_UP) {
        if (m_row == 0 || section_of(m_row-1)!=m_section)
            m_tiles=true;
        else
            do
                m_row--;
            while (m_row > 0 && !shown(m_row));
    } else if (p & NUVIO_BTN_CIRCLE) {
        /* Back, as elsewhere: to the top of the list first, then up to the tabs. */
        m_tiles=true;
    } else if (p & (NUVIO_BTN_LEFT | NUVIO_BTN_RIGHT)) {
        change((Row)m_row, (p & NUVIO_BTN_RIGHT) ? 1 : -1);
    } else if (p & NUVIO_BTN_CROSS) {
        if (m_row == SwitchUser)
            a.kind = Action::SwitchUser;
        else if (m_row == SignOut)
            a.kind = Action::SignOut;
        else if (m_row == Together) {   /* the groups page */
            a.kind = Action::Open;
            a.item.type = "SyncPlay";
        }
        else if (m_row == SeerrUrl) {
            const seerr_service::Config c = seerr_service::config();
            ime::request(ime::Kind::Url, T("Seerr-adresse"), c.url.empty() ? seerr_service::suggested_url() : c.url,
                         [](const std::string &t) {
                             seerr_service::Config n = seerr_service::config();
                             n.url = t;
                             seerr_service::set_config(n);
                         });
        }
        else if(m_row==M3UUrl) {
            ime::request(ime::Kind::Url,"M3U playlist URL (empty to remove)",iptv_m3u::playlist_url(),
                         [this](const std::string &url) {
                             if(iptv_m3u::save_playlist_url(url)) iptv_live::refresh(&m_client,true);
                         });
        }
        else if (m_row == IPTVServer || m_row == IPTVUsername || m_row == IPTVPassword)
            iptv_edit((Row)m_row);
        else if (m_row == VODRefresh) iptv_vod::refresh(true);
        else if (m_row == IPTVCategories)
            iptv_open_categories();
        else if (m_row == SeerrAccount)
            seerr_account();
        else if (m_row == SeerrTest)
            seerr_service::test();
        else
            change((Row)m_row, 1);
    }
    return a;
}

void SettingsScreen::iptv_edit(Row r)
{
    const ime::Kind kind = r == IPTVServer ? ime::Kind::Url :
                           r == IPTVPassword ? ime::Kind::Password : ime::Kind::Text;
    const std::string initial = r == IPTVServer ? m_iptv_creds.server :
                                r == IPTVUsername ? m_iptv_creds.username : "";
    const char *title = r == IPTVServer ? "Xtream server URL" :
                        r == IPTVUsername ? "Xtream username" : "Xtream password";
    ime::request(kind, title, initial, [this,r](const std::string &v) {
        if (v.empty()) return;
        if (r == IPTVServer) m_iptv_creds.server = v;
        else if (r == IPTVUsername) m_iptv_creds.username = v;
        else m_iptv_creds.password = v;
        // Persist only complete credentials. No keyboard chaining: each row is edited separately.
        if (iptv_xtream::save_credentials(m_iptv_creds))
            m_iptv_store.load(m_iptv_creds.server + "|" + m_iptv_creds.username);
    });
}

void SettingsScreen::iptv_open_categories()
{
    m_iptv_store.load(m_iptv_creds.server + "|" + m_iptv_creds.username);
    m_iptv_page = IPTVCategoryList;
    m_iptv_category = 0;
    m_iptv_channel = 0;
    m_iptv_search.clear();
    m_iptv_catalog = std::make_shared<IPTVCatalogState>();
    if (m_iptv_creds.server.empty() || m_iptv_creds.username.empty() || m_iptv_creds.password.empty()) return;
    auto data = m_iptv_catalog;
    auto creds = m_iptv_creds;
    { std::lock_guard<std::mutex> lk(data->mutex); data->loading = true; }
    std::thread([data,creds] {
        iptv_xtream::Catalog catalog;
        std::string error;
        const bool ok = iptv_xtream::authenticate(creds, &error) &&
                        iptv_xtream::load_catalog(creds, &catalog, &error);
        std::lock_guard<std::mutex> lk(data->mutex);
        if (ok) data->catalog = std::move(catalog);
        else data->error = error.empty() ? "Unable to load channels" : error;
        data->loading = false;
    }).detach();
}

std::vector<size_t> SettingsScreen::iptv_filtered_channels(const iptv_xtream::Catalog &catalog) const
{
    std::vector<size_t> result;
    std::string needle = m_iptv_search;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    for (size_t i = 0; i < catalog.channels.size(); ++i) {
        std::string name = catalog.channels[i].name;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        if (needle.empty() || name.find(needle) != std::string::npos) result.push_back(i);
    }
    return result;
}

void SettingsScreen::iptv_category_input(uint32_t p)
{
    const auto &cats = m_iptv_store.categories();
    if (p & NUVIO_BTN_CIRCLE) {
        if (m_iptv_page == IPTVChannels) {m_iptv_page = IPTVCategoryList;m_iptv_search.clear();}
        else m_iptv_page = IPTVRows;
        return;
    }
    if (m_iptv_page == IPTVCategoryList) {
        const int n = (int)cats.size();
        if (p & NUVIO_BTN_UP) m_iptv_category = std::max(0,m_iptv_category-1);
        if (p & NUVIO_BTN_DOWN) m_iptv_category = std::min(n,m_iptv_category+1);
        if ((p & NUVIO_BTN_CROSS) && m_iptv_category == n) {
            ime::request(ime::Kind::Text,"New IPTV category","",[this](const std::string &name){
                if (m_iptv_store.create(name)) m_iptv_category=(int)m_iptv_store.categories().size()-1;
            });
        } else if (m_iptv_category < n) {
            const auto id = cats[(size_t)m_iptv_category].id;
            if (p & NUVIO_BTN_CROSS)
                ime::request(ime::Kind::Text,"Rename IPTV category",cats[(size_t)m_iptv_category].name,
                             [this,id](const std::string &name){m_iptv_store.rename(id,name);});
            if (p & NUVIO_BTN_SQUARE) {
                m_iptv_store.erase(id);
                m_iptv_category=std::min(m_iptv_category,(int)m_iptv_store.categories().size());
            }
            if (p & NUVIO_BTN_L2) {if(m_iptv_store.move(id,-1)) --m_iptv_category;}
            if (p & NUVIO_BTN_R2) {if(m_iptv_store.move(id,1)) ++m_iptv_category;}
            if (p & NUVIO_BTN_TRIANGLE) {m_iptv_page=IPTVChannels;m_iptv_channel=0;m_iptv_search.clear();}
        }
        return;
    }
    if (m_iptv_category >= (int)cats.size()) {m_iptv_page=IPTVCategoryList;return;}
    iptv_xtream::Catalog catalog;
    { std::lock_guard<std::mutex> lk(m_iptv_catalog->mutex); catalog=m_iptv_catalog->catalog; }
    if (p & NUVIO_BTN_TRIANGLE) {
        ime::request(ime::Kind::Text,"Search IPTV channels",m_iptv_search,
                     [this](const std::string &query){m_iptv_search=query;m_iptv_channel=0;});
        return;
    }
    if (p & NUVIO_BTN_SQUARE) {m_iptv_search.clear();m_iptv_channel=0;return;}
    const auto filtered=iptv_filtered_channels(catalog);
    const int n=(int)filtered.size();
    if (p & NUVIO_BTN_UP) m_iptv_channel=std::max(0,m_iptv_channel-1);
    if (p & NUVIO_BTN_DOWN) m_iptv_channel=std::min(std::max(0,n-1),m_iptv_channel+1);
    if (m_iptv_channel>=n) return;
    const auto id = cats[(size_t)m_iptv_category].id;
    const auto channel_id=catalog.channels[filtered[(size_t)m_iptv_channel]].id;
    const auto &assigned = cats[(size_t)m_iptv_category].channels;
    const bool is_assigned=std::find(assigned.begin(),assigned.end(),channel_id)!=assigned.end();
    if (p & NUVIO_BTN_CROSS) m_iptv_store.assign(id,channel_id,!is_assigned);
    if (p & NUVIO_BTN_L2) m_iptv_store.move_channel(id,channel_id,-1);
    if (p & NUVIO_BTN_R2) m_iptv_store.move_channel(id,channel_id,1);
}

void SettingsScreen::iptv_draw_categories()
{
    gfx::fill({0,0,gfx::W,gfx::H},kBg);
    gfx::text(180,150,"IPTV category management",{gfx::Bold,54},kText);
    const auto &cats=m_iptv_store.categories();
    const bool channels=m_iptv_page==IPTVChannels;
    gfx::text(180,212,channels ? "Triangle: Search  Square: Clear  X: Assign/unassign  Circle: Back" :
              "X: Rename/Create  Square: Delete  L2/R2: Reorder  Triangle: Assign channels  Circle: Back",
              {gfx::Medium,23},kText2);
    iptv_xtream::Catalog catalog; std::string error; bool loading;
    { std::lock_guard<std::mutex> lk(m_iptv_catalog->mutex);
      catalog=m_iptv_catalog->catalog; error=m_iptv_catalog->error; loading=m_iptv_catalog->loading; }
    if (channels && m_iptv_category >= (int)cats.size()) return;
    const auto filtered=channels ? iptv_filtered_channels(catalog) : std::vector<size_t>{};
    if (channels) gfx::text(180,255,"Search: " + (m_iptv_search.empty()?std::string("All channels (Triangle to search)"):m_iptv_search),{gfx::Medium,23,1400},kText2);
    const int count=channels ? (int)filtered.size() : (int)cats.size()+1;
    const int selected=channels ? m_iptv_channel : m_iptv_category;
    const int first=std::max(0,selected-7);
    for (int i=first;i<count && i<first+14;i++) {
        const float y=295.f+(i-first)*52.f;
        if (i==selected) glass_panel({165,y-31,1500,48},14,1.f,false);
        std::string name;
        if (!channels) name=i<(int)cats.size()?cats[(size_t)i].name:"+ Create category";
        else {
            const auto &ch=catalog.channels[filtered[(size_t)i]];
            const auto &assigned=cats[(size_t)m_iptv_category].channels;
            const bool checked=std::find(assigned.begin(),assigned.end(),ch.id)!=assigned.end();
            name=std::string(checked?"[X] ":"[ ] ")+ch.name;
        }
        gfx::text(190,y,name,{gfx::SemiBold,26,1360},i==selected?kText:kText2);
    }
    if (channels && loading) gfx::text(190,1025,"Loading channels...",{gfx::Medium,24},kText2);
    else if(channels && !error.empty()) gfx::text(190,1025,error,{gfx::Medium,24},kText2);
    else if(channels && count==0) gfx::text(190,400,m_iptv_search.empty()?"No channels loaded; check IPTV credentials":"No channels match your search",{gfx::Medium,24},kText2);
}

void SettingsScreen::draw(double, float dt)
{
    if (m_iptv_page != IPTVRows) {
        iptv_draw_categories();
        return;
    }
    m_animating = false;
    if (m_want_password && !ime::active()) {   /* a local Seerr account: its password, after the e-mail */
        m_want_password = false;
        const std::string email = m_seerr_email;
        ime::request(ime::Kind::Password, T("Passord for Seerr-kontoen"), "",
                     [email](const std::string &pw) { seerr_service::sign_in(email, pw); });
    }
    const seerr_service::State seerr_state = seerr_service::snapshot().state;
    if (seerr_state == seerr_service::State::Connecting || seerr_service::snapshot().testing)
        m_animating = true;   /* the values change on their own */
    if (!shown(m_row))
        m_row = m_section==6?VODStatus:SeerrOn;      /* turned off under the focus */
    gfx::fill({0, 0, gfx::W, gfx::H}, kBg);
    gfx::fill_vgradient({0, 0, gfx::W, 500}, 0x33302048u, 0x00000000u);
    if (m_tiles) {
        gfx::text(300,205,"Settings",{gfx::Bold,64},kText);
        const char *names[]={"Accounts & Servers","Playback","Discover / Seerr","Live TV","VOD","Appearance & About"};
        const char *sub[]={"Accounts, sign out","Audio, video, subtitles","Seerr connection","Xtream, M3U, groups","Cache, refresh, loading","Themes, display, language"};
        for(int i=0;i<6;++i){
            const int col=i%2,row=i/2;
            const gfx::Rect r{300.f+col*670.f,280.f+row*220.f,630.f,186.f};
            glass_panel(r,24,1.f,false);
            if(m_focused && i==m_tile) glass_panel({r.x+4,r.y+4,r.w-8,r.h-8},22,1.f,true);
            gfx::text(r.x+32,r.y+74,names[i],{gfx::Bold,31,580},kText);
            gfx::text(r.x+32,r.y+125,sub[i],{gfx::Medium,23,580},kText2);
        }
        return;
    }

    const float row_h = 84, head_h = 70, left = 360, width = gfx::W - 2 * left;
    /* Layout: each section's header, then its rows. */
    float y = 260;
    float ys[RowCount];
    int last_section = -1;
    for (int r = 0; r < RowCount; r++) {
        ys[r] = y;
        if (!shown(r) || section_of(r)!=m_section)
            continue;
        const int sec = section_of(r);
        if (sec != last_section) {
            y += last_section < 0 ? 0 : 30;
            y += head_h;
            last_section = sec;
        }
        ys[r] = y;
        y += row_h + 8;
    }
    m_scroll.to(std::max(0.f, ys[m_row] - 700));
    if (m_scroll.step(dt, 11.f))
        m_animating = true;
    const float off = m_scroll.value;

    gfx::text(left, 200 - off, T("Innstillinger"), {gfx::Bold, 64}, kText);
    /* Each section is one glass card (a grouped list); the focus is the drop. */
    for (int r0 = 0; r0 < RowCount;) {
        if (!shown(r0) || section_of(r0)!=m_section) {
            r0++;
            continue;
        }
        const int sec = section_of(r0);
        int r1 = r0;
        while (r1 + 1 < RowCount && section_of(r1 + 1) == sec && shown(r1 + 1))
            r1++;
        const gfx::Rect card{left - 8, ys[r0] - off - 8, width + 16, ys[r1] + row_h - ys[r0] + 16};
        if (card.y < gfx::H && card.y + card.h > 0)
            glass_panel(card, 24, 1.f, false);
        r0 = r1 + 1;
    }
    if (m_focused)
        m_drop.to({left, ys[m_row] - off, width, row_h}, m_row, 0, -off);
    else
        m_drop.hide();
    m_drop.draw(dt, 1.f, &m_animating, 16);
    last_section = -1;
    for (int r = 0; r < RowCount; r++) {
        if (!shown(r) || section_of(r)!=m_section)
            continue;
        const int sec = section_of(r);
        if (sec != last_section) {
            last_section = sec;
            if (sec < 7)
                gfx::text(left + 8, ys[r] - 22 - off, (sec == 2 || sec == 3) ? kHeaders[sec] : T(kHeaders[sec]), {gfx::Bold, 22}, kText3);
        }
        const bool focus = r == m_row;
        const gfx::Rect rr{left, ys[r] - off, width, row_h};
        const uint32_t fg = kText, fg2 = focus ? kText : kText2;
        const float cy = rr.y + rr.h / 2 + 9;
        gfx::text(rr.x + 32, cy, label_of(r), {focus ? gfx::Bold : gfx::SemiBold, 26}, r == SignOut ? 0xffff7a7au : fg);
        const std::string v = value((Row)r);
        const bool adj = adjustable(r);
        const float vx = rr.x + rr.w - 32 - (adj && focus ? 30 : 0);
        gfx::text(vx, cy, v, {gfx::Medium, 24, 760}, fg2, 2);
        if (adj && focus) {
            gfx::text(rr.x + rr.w - 30, cy, "\xE2\x80\xBA", {gfx::Bold, 30}, fg2, 2);
            gfx::text(vx - gfx::text_width(v, {gfx::Medium, 24, 760}) - 14, cy, "\xE2\x80\xB9", {gfx::Bold, 30}, fg2, 2);   /* as the value's own width */
        }
    }
    gfx::text(left, y + 40 - off,
              T("Lyd, undertekster og autoavspilling lagres på Emby-kontoen din og gjelder i alle Emby-apper."),
              {gfx::Regular, 20, width}, kText3);
    gfx::text(left, y + 72 - off, T("Språk følger PS5-en, eller velg her."), {gfx::Regular, 20, width}, kText3);
    gfx::text(left, y + 104 - off, T("Emby5 er fri programvare (GPL-3.0) og bygger på EVO Player og Nuvio PS5."),
              {gfx::Regular, 20, width}, kText3);
    if (seerr_service::config().enabled)   /* only where it means something */
        gfx::text(left, y + 136 - off,
                  T("Seerr henter alt fra TMDB selv: PS5-en snakker bare med Emby og Seerr."),
                  {gfx::Regular, 20, width}, kText3);
}

} // namespace ui
