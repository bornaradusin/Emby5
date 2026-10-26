#include <cctype>
/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/settings_screen.h"
#include "ui/settings_layout.h"
#include "app/io_worker.h"
#include "app/iptv_live.h"
#include <unordered_map>
#include <unordered_set>
#include "app/iptv_vod.h"
#include "app/iptv_m3u.h"
#include "ui/theme_presets.h"
#include "evo_audio_out.h"

#include "app/settings.h"
#include "app/update_service.h"
#include "app/seerr_service.h"
#include "evo_agc_runtime.h"
#include "app/syncplay.h"
#include "app/i18n.h"
#include "nuvio_input.h"
#include "platform/ime.h"

#include <algorithm>
#include <cstdio>
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
const Mode kModes[] = {{"Default", N_("Standard")},
                       {"Smart", N_("Smart")},
                       {"Always", N_("Alltid")},
                       {"OnlyForced", N_("Bare tvungne")},
                       {"HearingImpaired", N_("Hearing impaired (SDH)")},
                       {"None", N_("Av")}};
constexpr int kNumModes = sizeof(kModes) / sizeof(kModes[0]);

const char *kHeaders[] = {N_("Konto"), N_("Avspilling"), N_("Seerr"), N_("Live TV"), N_("Appearance"), "", N_("VOD"), N_("System"), N_("Content display")};

const char *auth_name(seerr_service::Auth a)
{
    switch (a) {
    case seerr_service::Auth::EmbyPassword: return T("Emby-passord");
    case seerr_service::Auth::Local: return T("Seerr-konto (e-post)");
    default: return T("Automatisk (Quick Connect)");
    }
}

using settings_layout::section_of;
using settings_layout::adjustable;
using settings_layout::label_of;
using settings_layout::tiles;
using settings_layout::tile_count;


template <class T, size_t N> int index_of(const T (&list)[N], const std::string &code)
{
    for (size_t i = 0; i < N; i++)
        if (code == list[i].code)
            return (int)i;
    return 0;
}

} // namespace

SettingsScreen::~SettingsScreen() { if(m_appearance.active)settings::preview_appearance(nullptr); ime::cancel(); }

void SettingsScreen::activate()
{
    if(m_appearance.active)end_appearance(false);
    m_channel_hold.reset();
    m_row = 0;
    m_tiles = true;
    m_notes_open = false;
    m_notes_scroll = 0;
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
    if (r == Together) return false;
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
    settings::All s = settings::get();
    if(m_appearance.active)static_cast<settings::Appearance&>(s.local)=m_appearance.draft;
    const auto yesno=[](bool v){return v?T("P\xC3\xA5"):T("Av");};
    switch (r) {
    case SwitchUser: return m_client.user_name();
    case EmbyLive: {const auto s=iptv_live::summary();return iptv_catalogue::describe(s.server,s.emby_count,iptv_catalogue::Unit::Channels);}
    case XtreamLive: {const auto s=iptv_live::summary();return iptv_catalogue::describe(s.xtream,s.xtream_count,iptv_catalogue::Unit::Channels);}
    case M3UStatus: {const auto s=iptv_live::summary();return iptv_catalogue::describe(s.playlist,s.m3u_count,iptv_catalogue::Unit::Channels);}
    case IPTVSummary: {
        const auto live=iptv_live::summary();
        const unsigned count=live.emby_count+live.xtream_count+live.m3u_count;
        return TF("{count} channels - {server} server / {xtream} Xtream / {m3u} M3U",
                  {{"count", std::to_string(count)}, {"server", std::to_string(live.emby_count)},
                   {"xtream", std::to_string(live.xtream_count)}, {"m3u", std::to_string(live.m3u_count)}});
    }
    case VODInterval: {
        const int hours=iptv_vod::refresh_interval_hours();
        return hours==0 ? std::string(T("Manual only")) : TF("Every {hours} hours",{{"hours",std::to_string(hours)}});
    }
    case VODRefresh: return iptv_vod::snapshot().loading?T("Refresh in progress"):T("X to refresh full catalogue");
    case VODStatus: {
        const auto vod=iptv_vod::snapshot();
        const auto counts=TF("{movies} movies, {shows} TV shows",{{"movies",std::to_string(vod.movie_count)},{"shows",std::to_string(vod.show_count)}});
        if(vod.loading)return counts+T(" - refreshing; available titles can be used");
        return counts+(vod.error.empty()?T(" - ready"):T(" - some sources need retry below"));
    }
    case VODMoviesStatus: {const auto s=iptv_vod::snapshot();return iptv_catalogue::describe(s.movies,s.movie_count-s.m3u_count,iptv_catalogue::Unit::Movies);}
    case VODShowsStatus: {const auto s=iptv_vod::snapshot();return iptv_catalogue::describe(s.shows,s.show_count,iptv_catalogue::Unit::Shows);}
    case VODM3UStatus: {const auto s=iptv_vod::snapshot();return iptv_catalogue::describe(s.playlist,s.m3u_count,iptv_catalogue::Unit::Movies);}
    case M3UUrl: {const auto url=iptv_m3u::playlist_url();return url.empty()?std::string(T("X to enter")):url;}
    case IPTVServer: return m_iptv_creds.server.empty() ? std::string(T("X to enter")) : m_iptv_creds.server;
    case IPTVUsername: return m_iptv_creds.username.empty() ? std::string(T("X to enter")) : m_iptv_creds.username;
    case IPTVPassword: return m_iptv_creds.password.empty() ? T("X to enter") : "********";
    case IPTVCategories: return TF("{count} categories - X to manage",{{"count",std::to_string(m_iptv_store.categories().size())}});
    case Upscaling: {
        const char *const modes[] = {T("Av"), T("Auto"), "FSR 1", "Anime4K"};
        return modes[std::max(0, std::min(3, s.local.upscale_mode))];
    }
    case Quality:
        return s.local.max_mbps == 0 ? T("Automatisk (maks)") : std::to_string(s.local.max_mbps) + " Mbit/s";
    case SubMode: return T(kModes[index_of(kModes, s.server.subtitle_mode)].name);
    case Theme: return std::string(theme_name(s.local.theme))+(settings::appearance_modified(s.local)?T(" - Modified"):T(" - Default"));
    case BackgroundArt: return s.local.background_artwork?T("Artwork"):T("Plain background");
    case BackdropDim: {const char *v[]={T("Theme default"),"25%","50%","75%","90%"};return v[s.local.backdrop_dim];}
    case ControlStyle: {const char *v[]={T("Theme default"),T("Solid"),T("Glass")};return v[s.local.control_style];}
    case FocusOutline: return s.local.strong_focus?T("Strong"):T("Normal");
    case PosterEnlarge: return yesno(s.local.poster_enlarge);
    case ReducedMotion: return yesno(s.local.reduced_motion);
    case DescriptionScroll: {const char *v[]={T("Off (Read more available)"),T("Slow"),T("Normal")};return s.local.reduced_motion?T("Off while Reduced motion is on"):v[s.local.description_scroll];}
    case ContentDisplay: return T("Spoilers, title logos and posters");
    case ColourVariant:return theme_variant_name(s.local.colour_variant);
    case ButtonShape:return theme_shape_name(s.local.button_shape);
    case PreviewScene:{const char *v[]={T("Buttons & navigation"),T("Library grid"),T("Movie / series details"),T("Playback controls")};return v[m_preview_scene];}
    case CompareAppearance:return m_compare_appearance?T("Last applied appearance"):T("Current draft");
    case CustomSlot:{const auto &slot=m_appearance.slots[m_appearance.selected_slot];return std::to_string(m_appearance.selected_slot+1)+" / 3 - "+(slot.name.empty()?std::string(T("Unnamed")):slot.name=="My theme"?std::string(T("My theme")):slot.name)+(slot.valid?"":T(" (empty)"));}
    case RenameMyTheme:return T("Open keyboard for the selected slot");
    case RestoreAppearance:return T("Restore colours and layout from before editing");
    case SaveMyTheme: return T("Save current draft to the selected slot");
    case LoadMyTheme: return m_appearance.saved_valid?T("Load selected slot into preview"):T("This slot is empty");
    case ApplyAppearance: return T("Save and return to System");
    case CancelAppearance: return T("Discard preview and return to System");
    case SpoilerProtection: return yesno(s.local.spoiler_protection);
    case TitleStyle: return s.local.title_as_text?T("Text title"):T("Logo (text fallback)");
    case DetailPoster: return yesno(s.local.detail_poster);
    case Accent: return theme_accent_name(s.local.theme_accent);
    case Background: return theme_background_name(s.local.theme_background);
    case UiTextSize: return std::to_string(s.local.ui_text_percent) + "%";
    case PosterSize: return s.local.poster_size==0?T("Compact"):s.local.poster_size==2?T("Large"):T("Standard");
    case ResetAppearance: return T("X to restore the selected preset's defaults");
    case Appearance: return T("Themes, colours, text and cards");
    case StorageInfo: return "/data/Emby5/";
    case CheckUpdates: return s.local.check_updates ? T("P\xC3\xA5") : T("Av");
    case Language:
        if (s.local.language.empty())
            return TF("PS5 ({language})", {{"language", i18n::language_name(i18n::ps5_language())}});
        return i18n::language_name(i18n::lang());
    case SubSize: return std::to_string(s.local.sub_size) + " %";
    case SubBackground:
        return s.local.sub_background < 0.05f ? std::string(T("Av"))
                                              : std::to_string((int)(s.local.sub_background * 100 + 0.5f)) + " %";
    case Autoplay: return s.server.autoplay_next ? T("På") : T("Av");
    case StillWatching: return s.local.still_watching == 1 ? T("After 3 episodes") :
                              s.local.still_watching == 2 ? T("After 2 hours") : T("Av");
    case AutoSkip: return s.local.auto_skip_intro ? T("På") : T("Av");
    case NightMode: return s.local.night_mode ? T("På") : T("Av");
    case Bitstream:   /* night mode needs the sound decoded here, so it wins */
        return !s.local.hdmi_bitstream ? T("Av") : s.local.night_mode ? T("Av med nattmodus") : T("På");
    case ThemeMusic: return s.local.theme_music ? T("På") : T("Av");
    case UpdateNotes: { const auto u=update_service::snapshot(); return u.checking ? T("Checking release notes …") : T("X to read release notes"); }
    case AppVersion: {
        int major = 0, minor = 0, patch = 0;
        if (std::sscanf(EMBY5_VERSION, "%d.%d.%d", &major, &minor, &patch) == 3)
            return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        return EMBY5_VERSION;
    }
    case AudioDelay:
        return s.local.audio_delay_ms == 0 ? std::string(T("Ingen"))
                                           : (s.local.audio_delay_ms > 0 ? "+" : "") + std::to_string(s.local.audio_delay_ms) + " ms";
    case Refresh:
        if (!evo_agc_runtime_supports_120hz())
            return T("60 Hz (TV-en har ikke 120 Hz)");
        return s.local.refresh_120 ? "120 Hz" : "60 Hz";
    case Together: return syncplay::active() ? syncplay::group_name() : std::string(T("Av"));
    case ServerInfo: {
        // Client::server() is a normalised base URL, never a playback URL/token.
        std::string name = m_server_name.empty() ? m_client.server() : m_server_name;
        const size_t secrets = name.find_first_of("?#");
        if (secrets != std::string::npos) name.resize(secrets);
        const size_t scheme = name.find("://"), at = name.find('@');
        if (scheme != std::string::npos && at != std::string::npos && at > scheme + 3)
            name.erase(scheme + 3, at - scheme - 2);
        return std::string(m_client.server_product()) + " - " + name +
               (m_server_version.empty() ? "" : "  " + m_server_version);
    }
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
    if(m_appearance.active) {
        auto &a=m_appearance.draft;
        bool changed=true;
        switch(r) {
        case Theme: m_appearance.choose_preset(cycle(a.theme,kThemeCount)); break;
        case ColourVariant:a.colour_variant=cycle(a.colour_variant,6);break;
        case ButtonShape:a.button_shape=cycle(a.button_shape,6);break;
        case PreviewScene:m_preview_scene=cycle(m_preview_scene,4);break;
        case CompareAppearance:m_compare_appearance=!m_compare_appearance;break;
        case CustomSlot:m_appearance.select_slot(cycle(m_appearance.selected_slot,settings::kCustomThemes));break;
        case Accent: a.theme_accent=cycle(a.theme_accent,9);break;
        case Background: a.theme_background=cycle(a.theme_background,kBackgroundCount);break;
        case BackgroundArt: a.background_artwork=!a.background_artwork;break;
        case BackdropDim: a.backdrop_dim=cycle(a.backdrop_dim,5);break;
        case UiTextSize: a.ui_text_percent=90+10*cycle((a.ui_text_percent-90)/10,4);break;
        case PosterSize: a.poster_size=cycle(a.poster_size,3);break;
        case ControlStyle: a.control_style=cycle(a.control_style,3);break;
        case FocusOutline: a.strong_focus=!a.strong_focus;break;
        case PosterEnlarge: a.poster_enlarge=!a.poster_enlarge;break;
        case ReducedMotion: a.reduced_motion=!a.reduced_motion;break;
        case DescriptionScroll: a.description_scroll=cycle(a.description_scroll,3);break;
        case SpoilerProtection: a.spoiler_protection=!a.spoiler_protection;break;
        case TitleStyle: a.title_as_text=!a.title_as_text;break;
        case DetailPoster: a.detail_poster=!a.detail_poster;break;
        default: changed=false;break;
        }
        if(changed) {settings::preview_appearance(&a);m_appearance_note.clear();}
        return;
    }

    switch (r) {
    case Language: {   /* the PS5's, then each language in turn */
        int i = -1;
        for (int k = 0; k < i18n::kLangCount; k++)
            if (s.local.language == i18n::language_code((i18n::Lang)k))
                i = k;
        i = cycle(i + 1, i18n::kLangCount + 1) - 1;
        s.local.language = i < 0 ? std::string() : std::string(i18n::language_code((i18n::Lang)i));
        settings::set_local(s.local);
        i18n::use_language(s.local.language);   /* the screens rebuild their text (main loop) */
        return;
    }
    case CheckUpdates:
        s.local.check_updates = !s.local.check_updates;
        settings::set_local(s.local);
        update_service::set_automatic_enabled(s.local.check_updates);
        return;
    case VODInterval: {
        const int intervals[]={0,12,24,48,168};
        int i=2;
        for(int j=0;j<5;++j)if(intervals[j]==iptv_vod::refresh_interval_hours())i=j;
        iptv_vod::set_refresh_interval_hours(intervals[cycle(i,5)]);
        break;
    }
    case Upscaling:
        s.local.upscale_mode = cycle(s.local.upscale_mode, 4);
        settings::set_local(s.local);
        evo_agc_upscale_set_mode(s.local.upscale_mode == 0 ? EVO_AGC_UPSCALE_OFF
            : s.local.upscale_mode == 1 ? EVO_AGC_UPSCALE_AUTO
            : s.local.upscale_mode == 2 ? EVO_AGC_UPSCALE_SHARP : EVO_AGC_UPSCALE_AI);
        break;
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
    if(m_appearance.active)return input_appearance(p);
    if (m_notes_open) {
        if (p & (NUVIO_BTN_CIRCLE | NUVIO_BTN_CROSS)) m_notes_open = false;
        if (p & NUVIO_BTN_DOWN) m_notes_scroll += 5;
        if (p & NUVIO_BTN_UP) m_notes_scroll = std::max(0, m_notes_scroll - 5);
        return {};
    }
    if (m_iptv_page != IPTVRows) {
        iptv_category_input(p);
        return {};
    }
    Action a;
    const auto back_to_parent = [&] {
        if(m_section==8) {m_section=4;m_row=ContentDisplay;m_scroll.snap(0);}
        else if(m_section==4) end_appearance(false);
        else m_tiles=true;
    };
    if (m_tiles) {
        if(p & NUVIO_BTN_LEFT) m_tile=std::max(0,m_tile-1);
        if(p & NUVIO_BTN_RIGHT) m_tile=std::min(tile_count-1,m_tile+1);
        if(p & NUVIO_BTN_UP) {if(m_tile>=2)m_tile-=2; else a.kind=Action::ToNav;}
        if(p & NUVIO_BTN_DOWN) m_tile=std::min(tile_count-1,m_tile+2);
        if(p & NUVIO_BTN_CIRCLE) a.kind=Action::ToNav;
        if(p & NUVIO_BTN_CROSS) {
            m_section=tiles[m_tile].section;
            m_row=tiles[m_tile].first;m_scroll.snap(0);m_tiles=false;
        }
        return a;
    }
    if (!(p & NUVIO_BTN_CROSS))
        m_signout_armed = false;

    if (p & NUVIO_BTN_DOWN) {
        int r = m_row + 1;
        while (r < RowCount && (!shown(r) || section_of(r)!=m_section))
            r++;
        if (r < RowCount && section_of(r)==m_section)
            m_row = r;
    } else if (p & NUVIO_BTN_UP) {
        int previous=m_row-1;
        while (previous>=0 && (!shown(previous) || section_of(previous)!=m_section)) --previous;
        if (previous>=0) m_row=previous; else back_to_parent();
    } else if (p & NUVIO_BTN_CIRCLE) {
        back_to_parent();
    } else if (p & (NUVIO_BTN_LEFT | NUVIO_BTN_RIGHT)) {
        change((Row)m_row, (p & NUVIO_BTN_RIGHT) ? 1 : -1);
    } else if (p & NUVIO_BTN_CROSS) {
        if (m_row == Appearance) begin_appearance();
        else if (m_row == SwitchUser)
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
            ime::request(ime::Kind::Url,T("M3U playlist URL (empty to remove)"),iptv_m3u::playlist_url(),
                         [this](const std::string &url) {
                             if(iptv_m3u::save_playlist_url(url)) {iptv_live::refresh(&m_client,true);iptv_vod::refresh(true);}
                         });
        }
        else if (m_row == IPTVServer || m_row == IPTVUsername || m_row == IPTVPassword)
            iptv_edit((Row)m_row);
        else if (m_row == XtreamLive || m_row == EmbyLive || m_row == M3UStatus)
            iptv_live::refresh(&m_client,true,true);
        else if (m_row == IPTVSummary) iptv_live::refresh(&m_client,true);
        else if (m_row == VODStatus || m_row == VODMoviesStatus || m_row == VODShowsStatus || m_row == VODM3UStatus)
            iptv_vod::refresh(true,true);
        else if (m_row == VODRefresh) iptv_vod::refresh(true);
        else if (m_row == UpdateNotes) {
            update_service::check(); // explicit, read-only, works with automatic checks off
            m_notes_open = true;
            m_notes_scroll = 0;
        }
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
    const char *title = r == IPTVServer ? T("Xtream server URL") :
                        r == IPTVUsername ? T("Xtream username") : T("Xtream password");
    ime::request(kind, title, initial, [this,r](const std::string &v) {
        if (v.empty()) return;
        if (r == IPTVServer) m_iptv_creds.server = v;
        else if (r == IPTVUsername) m_iptv_creds.username = v;
        else m_iptv_creds.password = v;
        // Persist only complete credentials. No keyboard chaining: each row is edited separately.
        if (iptv_xtream::save_credentials(m_iptv_creds)) {
            m_iptv_store.load(m_iptv_creds.server + "|" + m_iptv_creds.username);
            iptv_live::refresh(&m_client,true);iptv_vod::refresh(true);
        }
    });
}

void SettingsScreen::iptv_open_categories() {
    m_iptv_store.load(m_iptv_creds.server+"|"+m_iptv_creds.username);
    m_iptv_page=IPTVCategoryList;m_iptv_category=m_iptv_channel=0;
    m_iptv_search.clear();m_group_note.clear();m_group_menu=ChoiceSheet{};m_import_confirm=false;
    m_iptv_catalog=std::make_shared<IPTVCatalogState>();
    m_visible_catalog.reset();m_filtered_channels.clear();m_channel_search_names.clear();
    m_filtered_query.clear();m_channel_hold.reset();m_iptv_generation=m_iptv_content_generation=~uint64_t(0);
    iptv_sync_catalog();
    // Reuse the single shared catalogue. Opening this editor must not start a
    // competing authentication/full-download sequence on the provider.
    if(!m_visible_catalog||m_visible_catalog->channels.empty())iptv_live::refresh(&m_client);
}
void SettingsScreen::iptv_filter_groups() {
    m_provider_filtered.clear();auto needle=m_provider_query;
    std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return (char)std::tolower(c);});
    for(size_t i=0;i<m_provider_groups.size();++i){auto name=m_provider_groups[i].name;
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return (char)std::tolower(c);});
        if(needle.empty()||name.find(needle)!=std::string::npos)m_provider_filtered.push_back(i);}
    m_provider_index=std::clamp(m_provider_index,0,std::max(0,(int)m_provider_filtered.size()-1));
}
void SettingsScreen::iptv_open_group_actions() {
    const auto &cats=m_iptv_store.categories();
    if(m_iptv_category<0||m_iptv_category>=(int)cats.size())return;
    m_import_confirm=false;
    m_group_menu.open(cats[m_iptv_category].name,{T("Add / remove individual channels"),T("Add channels from a provider group")});
}
void SettingsScreen::iptv_sync_catalog() {
    const auto summary=iptv_live::summary();
    if(summary.generation!=m_iptv_generation&&summary.catalogue_generation==m_iptv_content_generation) {
        std::lock_guard<std::mutex> lock(m_iptv_catalog->mutex);
        m_iptv_catalog->loading=summary.loading;m_iptv_catalog->error=summary.xtream_error;
        m_iptv_generation=summary.generation;
    }
    if(summary.catalogue_generation!=m_iptv_content_generation) {
        const auto live=iptv_live::snapshot();
        auto available=std::make_shared<iptv_xtream::Catalog>();
        available->channels=live.channels;available->scan=live.xtream_scan;
        m_provider_groups.clear();std::unordered_map<std::string,size_t> by_group;
        for(const auto &ch:available->channels) {
            if(ch.id.empty())continue;
            const auto name=ch.category_id.empty()?ch.source+" Live TV":ch.category_id;
            const auto title=ch.category_id.empty()?TF("{source} Live TV",{{"source",ch.source}}):
                ch.category_id=="Other Channels"?std::string(T("Other Channels")):ch.category_id;
            const auto id=ch.source+":"+(ch.category_key.empty()?name:ch.category_key);
            auto inserted=by_group.emplace(id,m_provider_groups.size());
            if(inserted.second){const auto &state=ch.source=="Xtream"?live.xtream:ch.source=="M3U"?live.playlist:live.server;
                bool partial=state.loading||state.cached||!state.error.empty();
                if(ch.source=="Xtream"&&!state.loading&&!ch.category_key.empty()&&live.xtream_scan.completed.count(ch.category_key))partial=false;
                m_provider_groups.push_back({id,ch.source+" - "+title,{},partial});}
            m_provider_groups[inserted.first->second].channels.push_back(iptv_categories::Store::channel_key(ch.id));
        }
        iptv_filter_groups();
        {std::lock_guard<std::mutex> lock(m_iptv_catalog->mutex);
            m_iptv_catalog->catalog=available;m_iptv_catalog->loading=live.loading;
            m_iptv_catalog->error=live.xtream_error;}
        m_iptv_generation=live.generation;m_iptv_content_generation=live.catalogue_generation;
    }
    std::shared_ptr<const iptv_xtream::Catalog> fresh;
    {std::lock_guard<std::mutex> lock(m_iptv_catalog->mutex);fresh=m_iptv_catalog->catalog;}
    const bool changed=fresh!=m_visible_catalog;
    if(!changed && m_filtered_query==m_iptv_search)return;
    m_visible_catalog=std::move(fresh);
    if(changed) {
        m_channel_search_names.clear();
        if(m_visible_catalog) {
            m_channel_search_names.reserve(m_visible_catalog->channels.size());
            for(const auto &ch:m_visible_catalog->channels) {
                auto name=ch.name;
                std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return (char)std::tolower(c);});
                m_channel_search_names.push_back(std::move(name));
            }
        }
    }
    m_filtered_query=m_iptv_search;
    auto needle=m_iptv_search;
    std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return (char)std::tolower(c);});
    m_filtered_channels.clear();m_filtered_channels.reserve(m_channel_search_names.size());
    for(size_t i=0;i<m_channel_search_names.size();++i)
        if(needle.empty()||m_channel_search_names[i].find(needle)!=std::string::npos)m_filtered_channels.push_back(i);
    m_iptv_channel=std::max(0,std::min(m_iptv_channel,(int)m_filtered_channels.size()-1));
}
void SettingsScreen::iptv_move_channel(int delta) {
    m_iptv_channel=std::max(0,std::min(m_iptv_channel+delta,(int)m_filtered_channels.size()-1));
}
void SettingsScreen::update_hold(uint32_t held,double now) {
    if((m_iptv_page!=IPTVChannels && m_iptv_page!=IPTVProviderGroups) || m_group_menu.active() || ime::active() || !m_focused) {m_channel_hold.reset();return;}
    iptv_sync_catalog();
    const int delta=m_channel_hold.update(held,now);
    if(m_iptv_page==IPTVProviderGroups)m_provider_index=std::clamp(m_provider_index+delta,0,std::max(0,(int)m_provider_filtered.size()-1));
    else iptv_move_channel(delta);
}

void SettingsScreen::iptv_category_input(uint32_t p)
{
    if(ime::active())return;
    iptv_sync_catalog();
    if(m_group_menu.active()) {
        const int choice=m_group_menu.input(p);if(choice<0)return;
        if(m_import_confirm) {
            if(choice==1){size_t added=0;
                if(m_import_generation!=m_iptv_content_generation)m_group_note=T("The catalogue changed. Select the provider group again before importing.");
                else if(m_iptv_store.add_channels(m_import_destination,m_import_channels,&added))m_group_note=TF("Added {count} channels. Existing channels kept; duplicates skipped.",{{"count",std::to_string(added)}});
                else m_group_note=T("Could not save the group. No channels were added.");
            }
            m_import_channels.clear();m_import_confirm=false;
        } else if(choice==0) {m_iptv_page=IPTVChannels;m_iptv_channel=0;m_iptv_search.clear();m_channel_hold.reset();}
        else if(choice==1) {m_iptv_page=IPTVProviderGroups;m_provider_query.clear();m_provider_index=0;iptv_filter_groups();m_channel_hold.reset();}
        return;
    }
    const auto &cats = m_iptv_store.categories();
    if (p & NUVIO_BTN_CIRCLE) {
        m_channel_hold.reset();
        if (m_iptv_page == IPTVChannels || m_iptv_page == IPTVProviderGroups) {m_iptv_page = IPTVCategoryList;m_iptv_search.clear();}
        else m_iptv_page = IPTVRows;
        return;
    }
    if ((p&NUVIO_BTN_OPTIONS)&&m_iptv_page!=IPTVProviderGroups){iptv_open_group_actions();return;}
    if (m_iptv_page == IPTVCategoryList) {
        const int n = (int)cats.size();
        if (p & NUVIO_BTN_UP) m_iptv_category = std::max(0,m_iptv_category-1);
        if (p & NUVIO_BTN_DOWN) m_iptv_category = std::min(n,m_iptv_category+1);
        if ((p & NUVIO_BTN_CROSS) && m_iptv_category == n) {
            ime::request(ime::Kind::Text,T("New IPTV category"),"",[this](const std::string &name){
                if (m_iptv_store.create(name)) m_iptv_category=(int)m_iptv_store.categories().size()-1;
            });
        } else if (m_iptv_category < n) {
            const auto id = cats[(size_t)m_iptv_category].id;
            if (p & NUVIO_BTN_CROSS)
                ime::request(ime::Kind::Text,T("Rename IPTV category"),cats[(size_t)m_iptv_category].name,
                             [this,id](const std::string &name){m_iptv_store.rename(id,name);});
            if (p & NUVIO_BTN_SQUARE) {
                m_iptv_store.erase(id);
                m_iptv_category=std::min(m_iptv_category,(int)m_iptv_store.categories().size());
            }
            if (p & NUVIO_BTN_L2) {if(m_iptv_store.move(id,-1)) --m_iptv_category;}
            if (p & NUVIO_BTN_R2) {if(m_iptv_store.move(id,1)) ++m_iptv_category;}
            if (p & NUVIO_BTN_TRIANGLE) {m_iptv_page=IPTVChannels;m_iptv_channel=0;m_iptv_search.clear();m_channel_hold.reset();}
        }
        return;
    }
    if (m_iptv_category >= (int)cats.size()) {m_iptv_page=IPTVCategoryList;return;}
    if(m_iptv_page==IPTVProviderGroups) {
        if(p&NUVIO_BTN_TRIANGLE){ime::request(ime::Kind::Search,T("Find provider group"),m_provider_query,[this](const std::string &q){m_provider_query=q;m_provider_index=0;iptv_filter_groups();});return;}
        if(p&NUVIO_BTN_SQUARE){m_provider_query.clear();m_provider_index=0;iptv_filter_groups();return;}
        if(p&NUVIO_BTN_UP)m_provider_index=std::max(0,m_provider_index-1);
        if(p&NUVIO_BTN_DOWN)m_provider_index=std::min(std::max(0,(int)m_provider_filtered.size()-1),m_provider_index+1);
        if((p&NUVIO_BTN_CROSS)&&!m_provider_filtered.empty()) {
            const auto &group=m_provider_groups[m_provider_filtered[m_provider_index]];
            const auto &dest=cats[m_iptv_category];m_import_destination=dest.id;m_import_channels=group.channels;m_import_generation=m_iptv_content_generation;
            std::unordered_set<std::string> existing;for(const auto &id:dest.channels)existing.insert(iptv_categories::Store::channel_key(id));
            size_t count=0;for(const auto &id:m_import_channels)if(existing.insert(id).second)++count;
            m_import_confirm=true;
            m_group_menu.open(TF(group.partial?"Partial/cached group: {group}":"Import group: {group}",{{"group",group.name}}),
                {T("Avbryt"),TF("Add {count} available channels to {group}",{{"count",std::to_string(count)},{"group",dest.name}})});
        }
        return;
    }
    iptv_sync_catalog();
    if(!m_visible_catalog)return;
    const auto &catalog=*m_visible_catalog;
    if (p & NUVIO_BTN_TRIANGLE) {
        ime::request(ime::Kind::Text,T("Search IPTV channels"),m_iptv_search,
                     [this](const std::string &query){m_iptv_search=query;m_iptv_channel=0;});
        return;
    }
    if (p & NUVIO_BTN_SQUARE) {m_iptv_search.clear();m_iptv_channel=0;return;}
    const auto &filtered=m_filtered_channels;
    const int n=(int)filtered.size();
    if (p & NUVIO_BTN_UP) iptv_move_channel(-1);
    if (p & NUVIO_BTN_DOWN) iptv_move_channel(1);
    if (m_iptv_channel>=n) return;
    const auto id = cats[(size_t)m_iptv_category].id;
    const auto channel_id=catalog.channels[filtered[(size_t)m_iptv_channel]].id;
    const auto &assigned = cats[(size_t)m_iptv_category].channels;
    const bool is_assigned=m_iptv_store.contains(id,channel_id);
    if (p & NUVIO_BTN_CROSS) {if(!m_iptv_store.assign(id,channel_id,!is_assigned))m_group_note=T("Could not save channel assignment.");else m_group_note.clear();}
    if (p & NUVIO_BTN_L2) m_iptv_store.move_channel(id,channel_id,-1);
    if (p & NUVIO_BTN_R2) m_iptv_store.move_channel(id,channel_id,1);
}

void SettingsScreen::iptv_draw_categories()
{
    gfx::fill({0,0,gfx::W,gfx::H},kBg);
    gfx::text(180,150,T("IPTV category management"),{gfx::Bold,54},kText);
    const auto &cats=m_iptv_store.categories();
    const bool channels=m_iptv_page==IPTVChannels;
    const bool providers=m_iptv_page==IPTVProviderGroups;
    gfx::text(180,212,providers ? T("Hold Up/Down: Faster scrolling  Triangle: Find group  X: Review import  Circle: Back") : channels ? T("Hold Up/Down: Faster scrolling  Triangle: Search  Square: Clear  X: Assign  Circle: Back") :
              T("X: Rename/Create  Square: Delete  L2/R2: Reorder  Triangle: Channels  Options: Add group"),
              {gfx::Medium,23},kText2);
    iptv_sync_catalog();
    std::string error; bool loading;
    { std::lock_guard<std::mutex> lk(m_iptv_catalog->mutex);
      error=m_iptv_catalog->error; loading=m_iptv_catalog->loading; }
    static const iptv_xtream::Catalog empty_catalog;
    const auto &catalog=m_visible_catalog?*m_visible_catalog:empty_catalog;
    if (channels && m_iptv_category >= (int)cats.size()) return;
    const auto &filtered=m_filtered_channels;
    if (channels) gfx::text(180,255,TF("Search: {query}",{{"query",m_iptv_search.empty()?std::string(T("All channels (Triangle to search)")):m_iptv_search}}),{gfx::Medium,23,1400},kText2);
    if(providers && m_iptv_category>=0 && m_iptv_category<(int)cats.size()) gfx::text(180,255,TF("Add to: {group}  |  Find: {query}",{{"group",cats[m_iptv_category].name},{"query",m_provider_query.empty()?std::string(T("All provider groups")):m_provider_query}}),{gfx::Medium,23,1400},kText2);
    const int count=providers?(int)m_provider_filtered.size():channels ? (int)filtered.size() : (int)cats.size()+1;
    const int selected=providers?m_provider_index:channels ? m_iptv_channel : m_iptv_category;
    const int first=std::max(0,selected-7);
    for (int i=first;i<count && i<first+14;i++) {
        const float y=295.f+(i-first)*52.f;
        if (i==selected) {glass_panel({165,y-31,1500,48},14,1.f,false,1.f);draw_focus_outline({165,y-31,1500,48},14);}
        std::string name;
        if(providers){const auto &g=m_provider_groups[m_provider_filtered[i]];name=g.name+" ("+TF(g.partial?"{count} channels, partial/cached":"{count} channels",{{"count",std::to_string(g.channels.size())}})+")";}
        else if (!channels) name=i<(int)cats.size()?cats[(size_t)i].name:std::string(T("+ Create category"));
        else {
            const auto &ch=catalog.channels[filtered[(size_t)i]];
            const bool checked=m_iptv_store.contains(cats[(size_t)m_iptv_category].id,ch.id);
            name=std::string(checked?"[X] ":"[ ] ")+ch.name;
        }
        gfx::text(190,y,name,{gfx::SemiBold,26,1360},i==selected?kText:kText2);
    }
    if (channels && loading) gfx::text(190,1025,T("Loading channels …"),{gfx::Medium,24},kText2);
    else if(channels && !error.empty()) gfx::text(190,1025,i18n::message(error),{gfx::Medium,24},kText2);
    else if(channels && count==0) gfx::text(190,400,m_iptv_search.empty()?T("No channels loaded; check IPTV credentials"):T("No channels match your search"),{gfx::Medium,24},kText2);
    if(providers&&count==0)gfx::text(190,400,loading?T("Catalogue loading; available groups will appear here."):T("No matching provider groups available."),{gfx::Medium,24},kText2);
    if(!m_group_note.empty())gfx::text(190,1060,m_group_note,{gfx::Medium,22,1500},kText2);
    m_group_menu.draw();
}

void SettingsScreen::draw(double, float dt)
{
    if (m_iptv_page != IPTVRows) {
        iptv_draw_categories();
        return;
    }
    m_animating = false;
    if(m_appearance.active) {draw_appearance(dt);return;}
    if (m_want_password && !ime::active()) {   /* a local Seerr account: its password, after the e-mail */
        m_want_password = false;
        const std::string email = m_seerr_email;
        ime::request(ime::Kind::Password, T("Passord for Seerr-kontoen"), "",
                     [email](const std::string &pw) { seerr_service::sign_in(email, pw); });
    }
    const seerr_service::State seerr_state = seerr_service::snapshot().state;
    if (seerr_state == seerr_service::State::Connecting || seerr_service::snapshot().testing)
        m_animating = true;
    { const auto update=update_service::snapshot();
      if(update.checking) m_animating=true; }   /* the values change on their own */
    if (!shown(m_row))
        m_row = m_section==6?VODStatus:(m_section==7?Appearance:(m_section==4?Theme:SeerrOn));      /* turned off under the focus */
    gfx::fill({0, 0, gfx::W, gfx::H}, kBg);
    gfx::fill_vgradient({0, 0, gfx::W, 500}, 0x33302048u, 0x00000000u);
    if (m_tiles) {
        gfx::text(300,205,T("Innstillinger"),{gfx::Bold,64},kText);
        for(int i=0;i<tile_count;++i){
            const int col=i%2,row=i/2;
            const gfx::Rect r{300.f+col*670.f,265.f+row*174.f,630.f,150.f};
            glass_panel(r,24,1.f,false,m_focused && i==m_tile?1.f:0.f);
            if(m_focused && i==m_tile)draw_focus_outline(r,24);
            gfx::text(r.x+32,r.y+62,T(tiles[i].label),{gfx::Bold,31,580},kText);
            gfx::text(r.x+32,r.y+106,T(tiles[i].description),{gfx::Medium,23,580},kText2);
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
        if(r==VODStatus && m_section==3)y+=70;
        ys[r] = y;
        y += row_h + 8;
    }
    m_scroll.to(std::max(0.f, ys[m_row] - 700));
    if (m_scroll.step(dt, 11.f))
        m_animating = true;
    const float off = m_scroll.value;

    gfx::text(left, 200 - off, TF("Settings / {section}", {{"section", T(kHeaders[m_section])}}), {gfx::Bold, 48}, kText);
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
            if (sec < 8)
                gfx::text(left + 8, ys[r] - 22 - off, T(kHeaders[sec]), {gfx::Bold, 22}, kText3);
        }
        if(r==VODStatus && m_section==3)
            gfx::text(left+8,ys[r]-22-off,T("VOD settings"),{gfx::Bold,24},kText);
        const bool focus = r == m_row;
        const gfx::Rect rr{left, ys[r] - off, width, row_h};
        const uint32_t fg = kText, fg2 = focus ? kText : kText2;
        const float cy = rr.y + rr.h / 2 + 9;
        gfx::text(rr.x + 32, cy, label_of(r), {focus ? gfx::Bold : gfx::SemiBold, 26, width*.47f}, r == SignOut ? 0xffff7a7au : fg);
        const std::string v = value((Row)r);
        const bool adj = adjustable(r);
        const float vx = rr.x + rr.w - 32 - (adj && focus ? 30 : 0);
        gfx::text(vx, cy, v, {gfx::Medium, 24, width*.47f}, fg2, 2);
        if (adj && focus) {
            gfx::text(rr.x + rr.w - 30, cy, "\xE2\x80\xBA", {gfx::Bold, 30}, fg2, 2);
            gfx::text(vx - gfx::text_width(v, {gfx::Medium, 24, width*.47f}) - 14, cy, "\xE2\x80\xB9", {gfx::Bold, 30}, fg2, 2);   /* as the value's own width */
        }
    }
    if (m_section==4)
        gfx::text(left, y+40-off, T("Presets remain available. Overrides apply only to this PS5; subtitles have separate sizing."),
                  {gfx::Regular,20,width}, kText3);
    if (m_notes_open) {
        gfx::fill({0, 0, gfx::W, gfx::H}, 0xc608101bu);
        const gfx::Rect panel{270, 145, gfx::W-540, gfx::H-290};
        glass_panel(panel, 26, 1.f, true);
        gfx::text(panel.x+45, panel.y+70, T("What's New"), {gfx::Bold, 42}, kText);
        const auto u = update_service::snapshot();
        gfx::text(panel.x+45, panel.y+116,
                  u.latest.empty()?std::string(T("Release notes")):("Emby5 "+u.latest),
                  {gfx::Medium,25}, kText2);
        std::vector<std::string> lines;
        const std::string notes=u.notes.empty()?
            (u.checking ? std::string(T("Loading release notes …")) : u.message.empty() ?
             std::string(T("No release notes available. Close and reopen to retry.")) : u.message) : u.notes;
        const float max_w=panel.w-110;
        std::string line;
        auto flush=[&]{lines.push_back(line);line.clear();};
        size_t cursor=0;
        while(cursor<notes.size() && lines.size()<500){
            if(notes[cursor]=='\n'){flush();++cursor;continue;}
            size_t end=notes.find_first_of(" \n",cursor);
            if(end==std::string::npos)end=notes.size();
            const std::string word=notes.substr(cursor,end-cursor);
            if(!word.empty()){
                const std::string next=line.empty()?word:line+" "+word;
                if(!line.empty() &&
                   gfx::text_width(next,{gfx::Regular,25})>max_w)flush();
                line+=(line.empty()?"":" ")+word;
            }
            cursor=end;
            if(cursor<notes.size() && notes[cursor]==' ')++cursor;
        }
        if(!line.empty())flush();
        const int visible=std::max(1,(int)((panel.h-230)/37));
        const int start=std::min(m_notes_scroll,
                                  std::max(0,(int)lines.size()-visible));
        for(int j=0;j<visible && start+j<(int)lines.size();++j)
            gfx::text(panel.x+48,panel.y+172+j*37,
                      lines[(size_t)(start+j)],{gfx::Regular,25},kText);
        gfx::text(panel.x+48,panel.y+panel.h-42,
                  T("Up/Down: scroll     X / O: close"),
                  {gfx::Medium,22},kText2);
    }
}

} // namespace ui