/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/epg.h"
#include "app/catalogue_request_queue.h"
#include "app/guide_text.h"
#include "app/i18n.h"
#include "app/io_worker.h"
#include "app/iptv_live.h"
#include "app/iptv_m3u.h"
#include "app/xmltv.h"
#include "jf/jf_catalogue_http.h"
#include "jf/jf_client.h"
#include "jf/jf_http.h"
#include "evo_boot_trace.h"
#include "evo_data_path.h"
extern "C" {
#include "cJSON.h"
}
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace epg {
namespace {

constexpr int64_t kBefore = 3600;            /* keep what ended within the hour */
constexpr int64_t kAhead = 26 * 3600;        /* and what starts within a day and a bit */
constexpr int64_t kStale = 2 * 3600;         /* a channel's guide is fetched again after this */
constexpr int64_t kRetry = 10 * 60;          /* a failed fetch waits this long */
constexpr int64_t kM3UStale = 12 * 3600;     /* the XMLTV file is downloaded again after this */
constexpr int64_t kM3UAhead = 14 * 3600;     /* an XMLTV file keeps what starts within this */
constexpr size_t kM3UMaxProgrammes = 80000;  /* memory bound for a giant guide (about 30 MB at most) */
constexpr size_t kM3UTextMax = 360;          /* a description, for the M3U guide */
constexpr size_t kPendingMax = 48;

int64_t now_s() { return (int64_t)std::time(nullptr); }

struct Entry {
    List list;
    int64_t fetched = 0, failed_at = 0;
    bool loading = false;
};

struct M3U {
    std::string url;                                  /* what was loaded */
    std::unordered_map<std::string, List> by_id;      /* XMLTV channel id -> programmes */
    std::unordered_map<std::string, std::string> by_name;   /* folded display name -> id */
    int64_t fetched = 0, failed_at = 0;
    bool loading = false, tried_disk = false;
    size_t programmes = 0;
    std::string error;
};

std::mutex mu;
std::unordered_map<std::string, Entry> rows;   /* channel id -> its guide (Emby, Jellyfin, Xtream) */
M3U m3u;
std::deque<iptv_xtream::Channel> pending;     /* what to fetch, nearest first */
jf::Client *pending_client = nullptr;
bool worker = false;
std::unordered_set<std::string> in_flight;    /* being fetched right now */
std::string s_url;                             /* iptv_m3u::guide_url(), read every few seconds */
int64_t s_url_read = 0;
std::atomic<unsigned> s_generation{1};
uint64_t s_epoch = 1;                          /* reset() moves it on: late answers are dropped */

void bump() { s_generation.fetch_add(1); }

std::string field(const cJSON *o, const char *k)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    if (cJSON_IsNumber(v)) return std::to_string((long long)v->valuedouble);
    return {};
}
int64_t number(const cJSON *o, const char *k)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    if (cJSON_IsNumber(v)) return (int64_t)v->valuedouble;
    if (cJSON_IsString(v) && v->valuestring) return std::strtoll(v->valuestring, nullptr, 10);
    return 0;
}

/* A channel name for matching an M3U channel to its XMLTV channel: lower
 * case, letters and digits only, and without a quality mark at the end. */
std::string fold(const std::string &s)
{
    std::string out;
    for (unsigned char c : s) {
        if (c >= 0x80) out.push_back((char)c);   /* keep non-ASCII letters as they are */
        else if (std::isalnum(c)) out.push_back((char)std::tolower(c));
    }
    for (const char *mark : {"uhd", "fhd", "hd", "sd", "4k", "hevc"}) {
        const size_t n = std::strlen(mark);
        if (out.size() > n + 2 && out.compare(out.size() - n, n, mark) == 0) {
            out.resize(out.size() - n);
            break;
        }
    }
    return out;
}

bool is_m3u(const iptv_xtream::Channel &c) { return c.source == "M3U"; }

std::string guide_address()   /* caller holds mu; files, so not every frame */
{
    const int64_t now = now_s();
    if (now - s_url_read >= 3 || now < s_url_read) {
        s_url = iptv_m3u::guide_url();
        s_url_read = now;
    }
    return s_url;
}
bool is_server(const iptv_xtream::Channel &c) { return c.source == "Emby" || c.source == "Jellyfin"; }

/* Only what the guide can show: sorted, without overlaps or stale entries. */
List tidy(std::vector<Programme> v)
{
    const int64_t now = now_s();
    v.erase(std::remove_if(v.begin(), v.end(), [&](const Programme &p) {
                return p.title.empty() || p.start <= 0 || (p.end > 0 && p.end < now - kBefore) || p.start > now + kAhead;
            }),
            v.end());
    std::sort(v.begin(), v.end(), [](const Programme &a, const Programme &b) { return a.start < b.start; });
    std::vector<Programme> out;
    out.reserve(v.size());
    for (auto &p : v) {
        if (!out.empty() && p.start == out.back().start) continue;   /* the same slot twice */
        if (!out.empty() && (out.back().end <= 0 || out.back().end > p.start))
            out.back().end = p.start;   /* an open end, or one that runs into the next */
        out.push_back(std::move(p));
    }
    if (!out.empty() && out.back().end <= out.back().start) out.back().end = out.back().start + 3600;
    return std::make_shared<const std::vector<Programme>>(std::move(out));
}

const List *m3u_find(const iptv_xtream::Channel &c)   /* caller holds mu */
{
    if (!c.epg_id.empty()) {
        auto it = m3u.by_id.find(c.epg_id);
        if (it != m3u.by_id.end()) return &it->second;
    }
    auto n = m3u.by_name.find(fold(c.name));
    if (n != m3u.by_name.end()) {
        auto it = m3u.by_id.find(n->second);
        if (it != m3u.by_id.end()) return &it->second;
    }
    return nullptr;
}

/* ---- M3U: the XMLTV file -------------------------------------------------- */

std::string cache_path() { return evo_data_path("emby5/guide-m3u.tsv"); }

std::string clean(const std::string &s)   /* one field of a cache line */
{
    std::string out = s;
    for (char &c : out)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return out;
}

void save_cache(const std::string &url, int64_t fetched, const std::unordered_map<std::string, List> &by_id,
                const std::unordered_map<std::string, std::string> &by_name)
{
    if (evo_mkdir(evo_data_dir()) != 0 || evo_mkdir(evo_data_path("emby5")) != 0) return;
    const std::string path = cache_path(), tmp = path + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f) return;
    bool ok = std::fprintf(f, "EMBY5-GUIDE\t1\t%lld\t%s\n", (long long)fetched, clean(url).c_str()) > 0;
    for (const auto &n : by_name)
        if (ok) ok = std::fprintf(f, "N\t%s\t%s\n", clean(n.first).c_str(), clean(n.second).c_str()) > 0;
    for (const auto &c : by_id)
        for (const auto &p : *c.second)
            if (ok)
                ok = std::fprintf(f, "P\t%s\t%lld\t%lld\t%s\t%s\n", clean(c.first).c_str(), (long long)p.start,
                                  (long long)p.end, clean(p.title).c_str(), clean(p.description).c_str()) > 0;
    ok = std::fclose(f) == 0 && ok;
    if (!ok || std::rename(tmp.c_str(), path.c_str()) != 0) std::remove(tmp.c_str());
}

std::vector<std::string> split(const std::string &line)
{
    std::vector<std::string> out;
    size_t a = 0;
    while (true) {
        const size_t b = line.find('\t', a);
        out.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

/* The guide kept on disk, when it is this address's and fresh enough. */
bool load_cache(const std::string &url, M3U *out)
{
    FILE *f = std::fopen(cache_path().c_str(), "rb");
    if (!f) return false;
    std::unordered_map<std::string, std::vector<Programme>> lists;
    std::string line;
    char buf[8192];
    bool header = false, ok = true;
    while (ok && std::fgets(buf, sizeof buf, f)) {
        line += buf;
        if (line.empty() || line.back() != '\n') continue;
        line.pop_back();
        const auto v = split(line);
        line.clear();
        if (!header) {
            header = true;
            if (v.size() < 4 || v[0] != "EMBY5-GUIDE" || v[1] != "1" || v[3] != clean(url)) ok = false;
            else out->fetched = std::strtoll(v[2].c_str(), nullptr, 10);
            if (ok && now_s() - out->fetched > kM3UStale) ok = false;
        } else if (v.size() == 3 && v[0] == "N") {
            out->by_name[v[1]] = v[2];
        } else if (v.size() == 6 && v[0] == "P") {
            Programme p;
            p.start = std::strtoll(v[2].c_str(), nullptr, 10);
            p.end = std::strtoll(v[3].c_str(), nullptr, 10);
            p.title = v[4];
            p.description = v[5];
            lists[v[1]].push_back(std::move(p));
        }
    }
    std::fclose(f);
    if (!ok || !header) return false;
    size_t total = 0;
    for (auto &l : lists) {
        List t = tidy(std::move(l.second));
        total += t->size();
        out->by_id[l.first] = std::move(t);
    }
    out->programmes = total;
    out->url = url;
    return total > 0;
}

/* Download and read the XMLTV file: keep the programmes of the channels the
 * playlist has (by tvg-id, or by name), within the guide's window. */
void load_m3u(const std::string &url, uint64_t epoch)
{
    M3U fresh;
    fresh.url = url;
    bool from_disk = false;
    {
        std::lock_guard<std::mutex> g(mu);
        from_disk = !m3u.tried_disk;
        m3u.tried_disk = true;
    }
    if (from_disk && load_cache(url, &fresh)) {
        std::lock_guard<std::mutex> g(mu);
        if (epoch != s_epoch) return;
        fresh.loading = false;
        fresh.tried_disk = true;
        m3u = std::move(fresh);
        bump();
        evo_bt("epg: M3U guide from disk, %zu programmes", m3u.programmes);
        return;
    }
    /* The playlist's channels decide what is kept. */
    std::unordered_set<std::string> ids, names;
    for (const auto &c : iptv_live::snapshot().channels)
        if (is_m3u(c)) {
            if (!c.epg_id.empty()) ids.insert(c.epg_id);
            names.insert(fold(c.name));
        }
    const bool keep_all = ids.empty() && names.empty();
    std::unordered_set<std::string> wanted;   /* XMLTV channel ids worth keeping */
    std::unordered_map<std::string, std::vector<Programme>> lists;
    const int64_t now = now_s();
    size_t kept = 0;
    xmltv::Reader reader(
        [&](const std::string &id, const std::vector<std::string> &display) {
            bool want = keep_all || ids.count(id);
            for (const auto &n : display) {
                const std::string f = fold(n);
                if (f.empty()) continue;
                if (names.count(f)) {
                    want = true;
                    fresh.by_name.emplace(f, id);   /* the first channel with this name wins */
                }
            }
            if (want) wanted.insert(id);
        },
        [&](xmltv::Programme &&p) {
            if (kept >= kM3UMaxProgrammes) return;
            if (!keep_all && !wanted.count(p.channel) && !ids.count(p.channel)) return;
            if ((p.end > 0 && p.end < now - kBefore) || p.start > now + kM3UAhead) return;
            Programme q;
            q.start = p.start;
            q.end = p.end;
            q.title = std::move(p.title);
            q.description = guide_text::clip(p.description, kM3UTextMax);
            lists[p.channel].push_back(std::move(q));
            kept++;
        });
    bool reader_ok = true;
    const auto result = jf::http_stream(
        url,
        [&](const char *data, size_t n) { return (reader_ok = reader.feed(data, n)); },
        [&] {
            std::lock_guard<std::mutex> g(mu);
            return epoch != s_epoch || iptv_catalogue::playback_active();
        },
        {}, 60, 1ull << 30);
    if (reader_ok && (result.complete || kept > 0)) reader_ok = reader.finish();
    size_t total = 0;
    for (auto &l : lists) {
        List t = tidy(std::move(l.second));
        total += t->size();
        fresh.by_id[l.first] = std::move(t);
    }
    fresh.programmes = total;
    fresh.fetched = now_s();
    std::unique_lock<std::mutex> g(mu);
    if (epoch != s_epoch) return;
    m3u.loading = false;
    if (total == 0) {
        m3u.failed_at = now_s();
        m3u.error = !result.complete && result.cancelled ? std::string()
                    : !reader_ok ? std::string(N_("The TV guide address isn't an XMLTV guide"))
                    : !result.complete ? std::string(N_("The TV guide couldn't be downloaded"))
                                       : std::string(N_("The TV guide has nothing for these channels"));
        if (!result.complete && result.cancelled) m3u.failed_at = 0;   /* paused for playback: try again later */
        evo_bt("epg: M3U guide failed (%s; %s)", result.error.c_str(), reader.error().c_str());
        bump();
        return;
    }
    fresh.tried_disk = true;
    m3u = std::move(fresh);
    bump();
    evo_bt("epg: M3U guide, %zu programmes for %zu channels", m3u.programmes, m3u.by_id.size());
    const auto by_id = m3u.by_id;   /* the lists are shared, not copied */
    const auto by_name = m3u.by_name;
    const auto fetched = m3u.fetched;
    g.unlock();
    save_cache(url, fetched, by_id, by_name);
}

/* ---- Emby / Jellyfin and Xtream: a few channels at a time ---------------- */

std::string iso(int64_t t)
{
    std::time_t tt = (std::time_t)t;
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return b;
}

void fetch_server(jf::Client *client, const std::vector<iptv_xtream::Channel> &batch, uint64_t epoch)
{
    std::string ids;
    for (const auto &c : batch) ids += (ids.empty() ? "" : ",") + c.emby_id;
    const int64_t now = now_s();
    const std::string path = "/LiveTv/Programs?ChannelIds=" + ids + "&MinEndDate=" + iso(now - kBefore) +
                             "&MaxStartDate=" + iso(now + 12 * 3600) +
                             "&SortBy=StartDate&SortOrder=Ascending&Fields=Overview&EnableImages=false"
                             "&EnableUserData=false&Limit=2000";
    std::string body;
    const bool ok = client && client->get_json(path, &body);
    std::unordered_map<std::string, std::vector<Programme>> lists;
    if (ok) {
        cJSON *j = cJSON_Parse(body.c_str());
        const cJSON *items = cJSON_IsArray(j) ? j : cJSON_GetObjectItemCaseSensitive(j, "Items");
        const cJSON *row;
        cJSON_ArrayForEach(row, items) {
            Programme p;
            p.title = guide_text::clip(field(row, "Name"), 240);
            p.start = guide_text::iso_epoch(field(row, "StartDate"));
            p.end = guide_text::iso_epoch(field(row, "EndDate"));
            const std::string episode = field(row, "EpisodeTitle"), overview = field(row, "Overview");
            p.description = guide_text::clip(episode.empty() ? overview
                                             : overview.empty() ? episode
                                                                : episode + " \xE2\x80\x93 " + overview, 900);
            lists[field(row, "ChannelId")].push_back(std::move(p));
        }
        cJSON_Delete(j);
    }
    std::lock_guard<std::mutex> g(mu);
    if (epoch != s_epoch) return;
    for (const auto &c : batch) {
        Entry &e = rows[c.id];
        e.loading = false;
        if (!ok) {
            e.failed_at = now_s();
            continue;
        }
        e.fetched = now_s();
        e.list = tidy(std::move(lists[c.emby_id]));
    }
    bump();
}

void fetch_xtream(const iptv_xtream::Credentials &creds, const iptv_xtream::Channel &c, uint64_t epoch)
{
    const std::string stream = c.id.rfind("xtream:", 0) == 0 ? c.id.substr(7) : c.id;
    const auto r = jf::http_request("GET", iptv_xtream::api_url(creds, "get_short_epg") + "&stream_id=" + stream + "&limit=40",
                                    {}, "", 15);
    std::vector<Programme> list;
    bool ok = r.ok();
    if (ok) {
        cJSON *j = cJSON_Parse(r.body.c_str());
        ok = j != nullptr;
        const cJSON *entry;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(j, "epg_listings")) {
            Programme p;
            p.title = guide_text::clip(guide_text::base64_text(field(entry, "title")), 240);
            p.description = guide_text::clip(guide_text::base64_text(field(entry, "description")), 900);
            p.start = number(entry, "start_timestamp");
            p.end = number(entry, "stop_timestamp");
            list.push_back(std::move(p));
        }
        cJSON_Delete(j);
    }
    std::lock_guard<std::mutex> g(mu);
    if (epoch != s_epoch) return;
    Entry &e = rows[c.id];
    e.loading = false;
    if (!ok) e.failed_at = now_s();
    else {
        e.fetched = now_s();
        e.list = tidy(std::move(list));
    }
    bump();
}

/* One worker at a time works through the queue, nearest channel first. */
void work()
{
    iptv_xtream::Credentials creds;
    bool have_creds = false;
    std::unique_lock<std::mutex> g(mu);
    while (true) {
        if (pending.empty() || iptv_catalogue::playback_active()) {
            for (const auto &c : pending) rows[c.id].loading = false;
            pending.clear();
            worker = false;
            bump();
            return;
        }
        const uint64_t epoch = s_epoch;
        jf::Client *client = pending_client;
        const iptv_xtream::Channel first = pending.front();
        if (is_server(first)) {
            std::vector<iptv_xtream::Channel> batch;
            for (auto it = pending.begin(); it != pending.end() && batch.size() < 12;)
                if (is_server(*it)) {
                    batch.push_back(*it);
                    it = pending.erase(it);
                } else
                    ++it;
            for (const auto &c : batch) in_flight.insert(c.id);
            g.unlock();
            fetch_server(client, batch, epoch);
            g.lock();
            for (const auto &c : batch) in_flight.erase(c.id);
        } else {
            pending.pop_front();
            in_flight.insert(first.id);
            g.unlock();
            if (!have_creds) have_creds = iptv_xtream::load_credentials(&creds);
            if (have_creds) fetch_xtream(creds, first, epoch);
            g.lock();
            in_flight.erase(first.id);
            if (!have_creds) {
                rows[first.id].loading = false;
                rows[first.id].failed_at = now_s();
            }
        }
    }
}

} // namespace

Row get(const iptv_xtream::Channel &channel)
{
    std::lock_guard<std::mutex> g(mu);
    Row r;
    if (is_m3u(channel)) {
        if (const List *l = m3u_find(channel)) r.programmes = *l;
        r.loading = !r.programmes && m3u.loading;
        r.none = !r.programmes && !m3u.loading;
        return r;
    }
    if ((channel.source != "Xtream" && !is_server(channel)) || (is_server(channel) && channel.emby_id.empty())) {
        r.none = true;   /* a source without a guide */
        return r;
    }
    auto it = rows.find(channel.id);
    if (it == rows.end()) {
        r.loading = true;   /* not asked yet: it will be */
        return r;
    }
    r.programmes = it->second.list;
    r.loading = it->second.loading && !r.programmes;
    r.none = !r.loading && (!r.programmes || r.programmes->empty());
    return r;
}

void want(jf::Client *client, const std::vector<iptv_xtream::Channel> &channels)
{
    if (iptv_catalogue::playback_active()) return;
    const int64_t now = now_s();
    std::lock_guard<std::mutex> g(mu);
    bool m3u_wanted = false;
    std::deque<iptv_xtream::Channel> next;
    for (const auto &c : channels) {
        if (is_m3u(c)) {
            m3u_wanted = true;
            continue;
        }
        if (c.source != "Xtream" && !is_server(c)) continue;
        if (is_server(c) && c.emby_id.empty()) continue;
        Entry &e = rows[c.id];
        const bool covered = e.list && !e.list->empty() && e.list->back().end > now + 3 * 3600;
        const bool fresh = e.fetched && now - e.fetched < kStale && (covered || now - e.fetched < 30 * 60);
        if (fresh || (e.failed_at && now - e.failed_at < kRetry) || in_flight.count(c.id)) continue;
        e.loading = true;
        next.push_back(c);
        if (next.size() >= kPendingMax) break;
    }
    if (!next.empty()) {
        /* What is on screen now goes first; what scrolled away is dropped. */
        for (const auto &c : pending) {
            bool kept = false;
            for (const auto &n : next)
                if (n.id == c.id) kept = true;
            if (!kept) rows[c.id].loading = false;
        }
        pending.swap(next);
        pending_client = client;
        if (!worker) {
            worker = true;
            if (!emby5::spawn_io(work)) {
                worker = false;
                for (const auto &c : pending) rows[c.id].loading = false;
                pending.clear();
            }
        }
    }
    if (m3u_wanted && !m3u.loading) {
        const std::string url = guide_address();
        const bool other = url != m3u.url;
        const bool stale = !m3u.fetched || now - m3u.fetched > kM3UStale;
        const bool waiting = m3u.failed_at && now - m3u.failed_at < kRetry && !other;
        if (!url.empty() && (other || stale) && !waiting) {
            if (other) {   /* another address: start over, the disk copy may be it */
                m3u = M3U();
                m3u.url.clear();
            }
            m3u.loading = true;
            m3u.error.clear();
            const uint64_t epoch = s_epoch;
            if (!emby5::spawn_io([url, epoch] { load_m3u(url, epoch); }, [] {
                    std::lock_guard<std::mutex> l(mu);
                    m3u.loading = false;
                }))
                m3u.loading = false;
            bump();
        } else if (url.empty() && (m3u.fetched || !m3u.error.empty())) {
            m3u = M3U();
            bump();
        }
    }
}

bool now_next(const iptv_xtream::Channel &channel, std::string *now, std::string *next, int64_t *now_start,
              int64_t *now_end)
{
    const Row r = get(channel);
    if (!r.programmes) return false;
    const int64_t t = now_s();
    bool found = false;
    for (size_t i = 0; i < r.programmes->size(); i++) {
        const Programme &p = (*r.programmes)[i];
        if (p.start <= t && t < p.end) {
            if (now) *now = p.title;
            if (now_start) *now_start = p.start;
            if (now_end) *now_end = p.end;
            if (next && i + 1 < r.programmes->size()) *next = (*r.programmes)[i + 1].title;
            found = true;
            break;
        }
        if (p.start > t) {   /* nothing on now: the next one */
            if (next) *next = p.title;
            found = true;
            break;
        }
    }
    return found;
}

unsigned generation() { return s_generation.load(); }

void reset()
{
    std::lock_guard<std::mutex> g(mu);
    ++s_epoch;
    rows.clear();
    pending.clear();
    m3u = M3U();
    bump();
}

Status m3u_status()
{
    std::lock_guard<std::mutex> g(mu);
    Status s;
    s.configured = !guide_address().empty();
    s.loading = m3u.loading;
    s.channels = m3u.by_id.size();
    s.programmes = m3u.programmes;
    s.error = m3u.error;
    return s;
}

} // namespace epg
