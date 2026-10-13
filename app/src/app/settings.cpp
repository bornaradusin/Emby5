/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app/settings.h"
#include "app/appearance.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "evo_boot_trace.h"
#include "evo_data_path.h"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include <unistd.h>

extern "C" {
#include "cJSON.h"
}

namespace settings {
namespace {

const char *settings_file() { return evo_data_path("emby5/settings.json"); }
std::mutex s_lock;
All s_all;
bool s_preview_active=false;
Appearance s_preview;
unsigned long s_local_revision = 0;
bool s_local_worker_running = false;
std::mutex s_server_write_lock;


// Use the same resolved data root as accounts, IPTV and the catalogue caches.
// Never make settings alone depend on a synchronous sandbox-promotion attempt:
// that prevented saving any preference when promotion was unavailable.
bool settings_storage_ready()
{
    const std::string root = evo_data_dir();
    const std::string directory = evo_data_path("emby5");
    if (evo_mkdir(root.c_str()) != 0 || evo_mkdir(directory.c_str()) != 0) {
        evo_bt("emby5: settings directory unavailable: %s", directory.c_str());
        return false;
    }
    return true;
}

bool write_atomic(const std::string &file, const char *text)
{
    if (!text) return false;
    const std::string tmp = file + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f) {
        return false;
    }
    const size_t len = std::strlen(text);
    const bool written = std::fwrite(text, 1, len, f) == len;
    const bool flushed = std::fflush(f) == 0;
    // Some PS5 mounts reject fsync despite accepting durable file writes.
    // Treat only unsupported-sync errors as non-fatal; keep real I/O errors fatal.
    bool synced = false;
    if (flushed) {
        if (::fsync(::fileno(f)) == 0) synced = true;
        else {
            const int e = errno;
            synced = (e == EINVAL || e == ENOSYS || e == ENOTSUP || e == EOPNOTSUPP);
        }
    }
    const bool closed = std::fclose(f) == 0;
    if (!written || !flushed || !synced || !closed || std::rename(tmp.c_str(), file.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

} // namespace

All get()
{
    std::lock_guard<std::mutex> g(s_lock);
    return s_all;
}

Appearance get_appearance() {
    std::lock_guard<std::mutex> g(s_lock);
    return s_preview_active ? s_preview : static_cast<const Appearance&>(s_all.local);
}
void preview_appearance(const Appearance *a) {
    std::lock_guard<std::mutex> g(s_lock);
    s_preview_active=a!=nullptr;
    if(a)s_preview=normalize_appearance(*a);
}

// cJSON returns NaN for missing/non-numeric fields. Clamp in double precision
// before an integer conversion; old or malformed local files must keep defaults.
static int read_int(const cJSON *object,const char *key,int fallback,int lo,int hi) {
    const auto *v=cJSON_GetObjectItemCaseSensitive(object,key);
    return cJSON_IsNumber(v)&&std::isfinite(v->valuedouble)
        ? (int)std::max((double)lo,std::min((double)hi,v->valuedouble)):fallback;
}
static float read_float(const cJSON *object,const char *key,float fallback,float lo,float hi) {
    const auto *v=cJSON_GetObjectItemCaseSensitive(object,key);
    return cJSON_IsNumber(v)&&std::isfinite(v->valuedouble)
        ? (float)std::max((double)lo,std::min((double)hi,v->valuedouble)):fallback;
}
static Appearance read_appearance(const cJSON *j, Appearance a) {
    const auto number=[&](const char *key,int fallback,int lo,int hi) {
        return read_int(j,key,fallback,lo,hi);
    };
    const auto boolean=[&](const char *key,bool fallback) {
        const auto *v=cJSON_GetObjectItemCaseSensitive(j,key);
        return cJSON_IsBool(v)?cJSON_IsTrue(v)!=0:fallback;
    };
    a.theme=number("theme",a.theme,0,kThemePresetCount-1);
    a.theme_accent=number("themeAccent",a.theme_accent,0,8);
    a.colour_variant=number("colourVariant",a.colour_variant,0,5);
    a.button_shape=number("buttonShape",a.button_shape,0,5);
    a.theme_background=number("themeBackground",a.theme_background,0,kThemeBackgroundCount-1);
    a.ui_text_percent=number("uiTextPercent",a.ui_text_percent,90,120);
    a.poster_size=number("posterSize",a.poster_size,0,2);
    a.backdrop_dim=number("backdropDimming",a.backdrop_dim,0,4);
    a.control_style=number("controlStyle",a.control_style,0,2);
    a.description_scroll=number("descriptionScroll",a.description_scroll,0,2);
    a.background_artwork=boolean("backgroundArtwork",a.background_artwork);
    a.strong_focus=boolean("strongFocus",a.strong_focus);
    a.poster_enlarge=boolean("posterEnlarge",a.poster_enlarge);
    a.reduced_motion=boolean("reducedMotion",a.reduced_motion);
    a.spoiler_protection=boolean("spoilerProtection",a.spoiler_protection);
    a.title_as_text=boolean("titleAsText",a.title_as_text);
    a.detail_poster=boolean("detailPoster",a.detail_poster);
    return normalize_appearance(a);
}
static void write_appearance(cJSON *j,const Appearance &input) {
    const auto a=normalize_appearance(input);
    cJSON_AddNumberToObject(j,"theme",a.theme);
    cJSON_AddNumberToObject(j,"themeAccent",a.theme_accent);
    cJSON_AddNumberToObject(j,"colourVariant",a.colour_variant);
    cJSON_AddNumberToObject(j,"buttonShape",a.button_shape);
    cJSON_AddNumberToObject(j,"themeBackground",a.theme_background);
    cJSON_AddNumberToObject(j,"uiTextPercent",a.ui_text_percent);
    cJSON_AddNumberToObject(j,"posterSize",a.poster_size);
    cJSON_AddNumberToObject(j,"backdropDimming",a.backdrop_dim);
    cJSON_AddNumberToObject(j,"controlStyle",a.control_style);
    cJSON_AddNumberToObject(j,"descriptionScroll",a.description_scroll);
    cJSON_AddBoolToObject(j,"backgroundArtwork",a.background_artwork);
    cJSON_AddBoolToObject(j,"strongFocus",a.strong_focus);
    cJSON_AddBoolToObject(j,"posterEnlarge",a.poster_enlarge);
    cJSON_AddBoolToObject(j,"reducedMotion",a.reduced_motion);
    cJSON_AddBoolToObject(j,"spoilerProtection",a.spoiler_protection);
    cJSON_AddBoolToObject(j,"titleAsText",a.title_as_text);
    cJSON_AddBoolToObject(j,"detailPoster",a.detail_poster);
}

void load_local()
{
    // Read from the exact shared root used by saved accounts and catalogues.
    // Loading does not modify process permissions or request storage access.
    std::string body;
    if (FILE *f = std::fopen(settings_file(), "rb")) {
        char buf[1024];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            body.append(buf, n);
        std::fclose(f);
    }
    cJSON *j = cJSON_Parse(body.c_str());
    if (!j)
        return;
    std::lock_guard<std::mutex> g(s_lock);
    s_all.local.upscale_mode=read_int(j,"upscaling",s_all.local.upscale_mode,0,3);
    s_all.local.max_mbps=read_int(j,"maxMbps",s_all.local.max_mbps,0,std::numeric_limits<int>::max());
    s_all.local.auto_skip_intro = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "autoSkipIntro"));
    // Missing/malformed legacy values use the enabled default. A real saved false
    // remains false; appearance resets do not change this independent preference.
    const auto *updates = cJSON_GetObjectItemCaseSensitive(j, "checkUpdates");
    s_all.local.check_updates = cJSON_IsBool(updates) ? cJSON_IsTrue(updates) : true;
    /* Ignore legacy saved language: the PS5 language is authoritative. */
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "autoplayNextLocal")) {
        if (cJSON_IsBool(v)) {
            s_all.local.autoplay_next_override_valid = true;
            s_all.local.autoplay_next_override = cJSON_IsTrue(v);
        }
    }
    static_cast<Appearance&>(s_all.local)=read_appearance(j,Appearance{});
    s_all.local.my_theme_valid=false;
    if(const auto *saved=cJSON_GetObjectItemCaseSensitive(j,"myTheme")) {
        if(cJSON_IsObject(saved)) { s_all.local.my_theme=read_appearance(saved,Appearance{}); s_all.local.my_theme_valid=true; }
    }
    s_all.local.custom_themes={};
    const auto *slots=cJSON_GetObjectItemCaseSensitive(j,"customThemes");
    if(cJSON_IsArray(slots)) {
        for(int i=0;i<kCustomThemes;++i) {
            auto *slot=cJSON_GetArrayItem(slots,i);
            if(!cJSON_IsObject(slot))continue;
            auto &s=s_all.local.custom_themes[i];
            auto *name=cJSON_GetObjectItemCaseSensitive(slot,"name");
            if(cJSON_IsString(name)&&name->valuestring)s.name=theme_slot_name(name->valuestring);
            auto *appearance=cJSON_GetObjectItemCaseSensitive(slot,"appearance");
            s.valid=cJSON_IsObject(appearance);
            if(s.valid)s.appearance=read_appearance(appearance,Appearance{});
        }
    } else if(s_all.local.my_theme_valid) {
        s_all.local.custom_themes[0]={true,"My theme",s_all.local.my_theme};
    }
    s_all.local.my_theme=s_all.local.custom_themes[0].appearance;
    s_all.local.my_theme_valid=s_all.local.custom_themes[0].valid;
    s_all.local.still_watching=read_int(j,"stillWatching",s_all.local.still_watching,0,2);
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "nightMode"))
        s_all.local.night_mode = cJSON_IsTrue(v);
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "hdmiBitstream"))
        s_all.local.hdmi_bitstream = cJSON_IsTrue(v);
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "themeMusic"))
        s_all.local.theme_music = cJSON_IsTrue(v);
    s_all.local.audio_delay_ms=read_int(j,"audioDelayMs",s_all.local.audio_delay_ms,-500,500);
    if (const cJSON *hz = cJSON_GetObjectItemCaseSensitive(j, "refresh120"))
        s_all.local.refresh_120 = cJSON_IsTrue(hz);
    if (const cJSON *st = cJSON_GetObjectItemCaseSensitive(j, "subtitles")) {
        Local &l = s_all.local;
        const cJSON *v;
        l.sub_size=read_int(st,"size",l.sub_size,50,200);
        l.sub_offset=read_float(st,"offset",l.sub_offset,0.f,40.f);
        l.sub_background=read_float(st,"background",l.sub_background,0.f,1.f);
        if (cJSON_IsBool(v = cJSON_GetObjectItemCaseSensitive(st, "outline")))
            l.sub_outline = cJSON_IsTrue(v);
        l.sub_color=read_int(st,"color",l.sub_color,0,0xffffff);
    }
    cJSON_Delete(j);
}

// Serialize local writes on a worker. Persistent-storage promotion can wait
// several seconds, so never perform it on the controller/UI thread.
static bool save_local_snapshot(const Local &l)
{
    if (!settings_storage_ready()) return false;
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "upscaling", l.upscale_mode);
    cJSON_AddNumberToObject(j, "maxMbps", l.max_mbps);
    cJSON_AddBoolToObject(j, "autoSkipIntro", l.auto_skip_intro);
    if (l.autoplay_next_override_valid)
        cJSON_AddBoolToObject(j, "autoplayNextLocal", l.autoplay_next_override);
    cJSON_AddNumberToObject(j, "stillWatching", l.still_watching);
    write_appearance(j,l);
    if(l.my_theme_valid) {
        auto *saved=cJSON_CreateObject();write_appearance(saved,l.my_theme);
        cJSON_AddItemToObject(j,"myTheme",saved);
    }
    auto slots=l.custom_themes;
    if(!slots[0].valid && l.my_theme_valid)slots[0]={true,"My theme",l.my_theme};
    auto *saved_slots=cJSON_CreateArray();
    for(const auto &slot:slots) {
        auto *entry=cJSON_CreateObject();
        cJSON_AddStringToObject(entry,"name",theme_slot_name(slot.name).c_str());
        if(slot.valid){auto *a=cJSON_CreateObject();write_appearance(a,slot.appearance);cJSON_AddItemToObject(entry,"appearance",a);}
        cJSON_AddItemToArray(saved_slots,entry);
    }
    cJSON_AddItemToObject(j,"customThemes",saved_slots);
    cJSON_AddBoolToObject(j, "refresh120", l.refresh_120);
    cJSON_AddNumberToObject(j, "audioDelayMs", l.audio_delay_ms);
    cJSON_AddBoolToObject(j, "nightMode", l.night_mode);
    cJSON_AddBoolToObject(j, "hdmiBitstream", l.hdmi_bitstream);
    cJSON_AddBoolToObject(j, "themeMusic", l.theme_music);
    cJSON_AddBoolToObject(j, "checkUpdates", l.check_updates);

    cJSON *st = cJSON_CreateObject();
    cJSON_AddNumberToObject(st, "size", l.sub_size);
    cJSON_AddNumberToObject(st, "offset", l.sub_offset);
    cJSON_AddNumberToObject(st, "background", l.sub_background);
    cJSON_AddBoolToObject(st, "outline", l.sub_outline);
    cJSON_AddNumberToObject(st, "color", l.sub_color);
    cJSON_AddItemToObject(j, "subtitles", st);
    char *text = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    const bool saved = text && write_atomic(settings_file(), text);
    std::free(text);
    return saved;
}

void set_local(const Local &l)
{
    bool start_worker = false;
    {
        std::lock_guard<std::mutex> g(s_lock);
        s_all.local = l;
        ++s_local_revision;
        if (!s_local_worker_running) {
            s_local_worker_running = true;
            start_worker = true;
        }
    }
    if (!start_worker) return;
    std::thread([] {
        for (;;) {
            Local snapshot;
            unsigned long revision;
            {
                std::lock_guard<std::mutex> g(s_lock);
                snapshot = s_all.local;
                revision = s_local_revision;
            }
            // Retry transient storage errors instead of silently discarding a preference.
            // Keep the newest revision rather than overwriting it with an old snapshot.
            bool saved = false;
            for (int attempt = 0; attempt < 6; ++attempt) {
                saved = save_local_snapshot(snapshot);
                if (saved) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
                {
                    std::lock_guard<std::mutex> g(s_lock);
                    if (revision != s_local_revision) break;
                }
            }
            {
                std::lock_guard<std::mutex> g(s_lock);
                if (!saved && revision == s_local_revision) {
                    s_local_worker_running = false;
                    evo_bt("emby5: local settings persist failed after retries");
                    return;
                }
            }
            {
                std::lock_guard<std::mutex> g(s_lock);
                if (revision == s_local_revision) {
                    s_local_worker_running = false;
                    return;
                }
            }
        }
    }).detach();
}

void load_server(jf::Client &c)
{
    jf::UserPrefs p;
    if (!c.get_prefs(&p)) return;
    std::lock_guard<std::mutex> g(s_lock);
    s_all.server = p;
    if (s_all.local.autoplay_next_override_valid)
        s_all.server.autoplay_next = s_all.local.autoplay_next_override;
}

void set_server(jf::Client &c, const jf::UserPrefs &p)
{
    Local local;
    {
        std::lock_guard<std::mutex> g(s_lock);
        s_all.server = p;
        s_all.local.autoplay_next_override_valid = true;
        s_all.local.autoplay_next_override = p.autoplay_next;
        local = s_all.local;
    }
    set_local(local);
    jf::Client *cp = &c;
    std::thread([cp, p] { cp->set_prefs(p); }).detach();
}

} // namespace settings
