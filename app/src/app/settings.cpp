/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app/settings.h"
#include "app/i18n.h"

#include <algorithm>

#include "evo_boot_trace.h"
#include "evo_data_path.h"
#include "evo_jailbreak.h"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <sys/stat.h>
#include <thread>

extern "C" {
#include "cJSON.h"
}

namespace settings {
namespace {

const char *settings_file() { return evo_data_path("emby5/settings.json"); }
std::mutex s_lock;
All s_all;
unsigned long s_local_revision = 0;
bool s_local_worker_running = false;
std::mutex s_server_write_lock;


// Do not write preference files into the transient /download0 fallback.
bool durable_storage()
{
    if (!evo_jailbreak_is_open() && !evo_jailbreak_ensure()) {
        return false;
    }
    evo_data_path_rebind();
    if (evo_mkdir(evo_data_dir()) != 0 || evo_mkdir(evo_data_path("emby5")) != 0) {
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
    const bool closed = std::fclose(f) == 0;
    if (!written || !flushed || !closed || std::rename(tmp.c_str(), file.c_str()) != 0) {
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

void load_local()
{
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
    s_all.local.max_mbps = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(j, "maxMbps"));
    if (s_all.local.max_mbps < 0)
        s_all.local.max_mbps = 0;
    s_all.local.auto_skip_intro = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "autoSkipIntro"));
    s_all.local.language = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(j, "language"));
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "theme"))
        if (cJSON_IsNumber(v)) s_all.local.theme = std::max(0, std::min(30, (int)v->valuedouble));
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "stillWatching"))
        if (cJSON_IsNumber(v)) s_all.local.still_watching = std::max(0, std::min(2, (int)v->valuedouble));
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "nightMode"))
        s_all.local.night_mode = cJSON_IsTrue(v);
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "hdmiBitstream"))
        s_all.local.hdmi_bitstream = cJSON_IsTrue(v);
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "themeMusic"))
        s_all.local.theme_music = cJSON_IsTrue(v);
    if (const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "checkUpdates"))
        s_all.local.check_updates = cJSON_IsTrue(v);
    s_all.local.audio_delay_ms =
        std::max(-500, std::min(500, (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(j, "audioDelayMs"))));
    if (const cJSON *hz = cJSON_GetObjectItemCaseSensitive(j, "refresh120"))
        s_all.local.refresh_120 = cJSON_IsTrue(hz);
    if (const cJSON *st = cJSON_GetObjectItemCaseSensitive(j, "subtitles")) {
        Local &l = s_all.local;
        const cJSON *v;
        if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(st, "size")))
            l.sub_size = std::max(50, std::min(200, (int)v->valuedouble));
        if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(st, "offset")))
            l.sub_offset = std::max(0.f, std::min(40.f, (float)v->valuedouble));
        if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(st, "background")))
            l.sub_background = std::max(0.f, std::min(1.f, (float)v->valuedouble));
        if (cJSON_IsBool(v = cJSON_GetObjectItemCaseSensitive(st, "outline")))
            l.sub_outline = cJSON_IsTrue(v);
    }
    if (s_all.local.language < 0 || s_all.local.language >= i18n::ChoiceCount)
        s_all.local.language = 0;
    cJSON_Delete(j);
}

// Serialize local writes on a worker. Persistent-storage promotion can wait
// several seconds, so never perform it on the controller/UI thread.
static void save_local_snapshot(const Local &l)
{
    if (!durable_storage()) return;
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "maxMbps", l.max_mbps);
    cJSON_AddBoolToObject(j, "autoSkipIntro", l.auto_skip_intro);
    cJSON_AddNumberToObject(j, "stillWatching", l.still_watching);
    cJSON_AddNumberToObject(j, "theme", l.theme);
    cJSON_AddNumberToObject(j, "language", l.language);
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
    cJSON_AddItemToObject(j, "subtitles", st);
    char *text = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!write_atomic(settings_file(), text))
    std::free(text);
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
            save_local_snapshot(snapshot);
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
}

void set_server(jf::Client &c, const jf::UserPrefs &p)
{
    {
        std::lock_guard<std::mutex> g(s_lock);
        s_all.server = p;
    }
    jf::Client *cp = &c;
    std::thread([cp, p] { cp->set_prefs(p); }).detach();
}

} // namespace settings
