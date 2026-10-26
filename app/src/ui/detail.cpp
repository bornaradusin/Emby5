/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sizes and timings follow concept/style.css (.detail, .dtop, .pills,
 * .card.ep, .card.cast).
 */
#include "ui/detail.h"
#include "app/appearance.h"
#include "app/user_data_update.h"
#include "app/spawn.h"
#include "app/io_worker.h"
#include "emby5_playback.h"
#include "app/i18n.h"
#include "app/seerr_service.h"

#include "gfx/art.h"
#include "nuvio_input.h"
#include "ui/item_menu.h"
#include "ui/detail_icons.h"
#include "ui/detail_layout.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <future>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace ui {
namespace {

constexpr float kTopH = 880;                    /* the info block before the sections */
constexpr float kCastD = 170, kCastGap = 32;
constexpr float kSimW = 400, kSimH = 225, kSimGap = 32;
constexpr float kExtrasH = 340, kCastH = 330, kSimilarH = 340;

std::string runtime_label(int64_t ticks)
{
    const int min = (int)(ticks / jf::kTicksPerSecond / 60);
    if (min <= 0)
        return std::string();
    char b[64];
    if (min >= 60)
        std::snprintf(b, sizeof b, T("%d t %d min"), min / 60, min % 60);
    else
        std::snprintf(b, sizeof b, T("%d min"), min);
    return b;
}

std::string ep_code(const jf::Item &e)
{
    char b[32];
    std::snprintf(b, sizeof b, "S%d:E%d", e.parent_index, e.index);
    return b;
}

/* 4K/HD, HDR10/HLG/Dolby Vision, Atmos/7.1/5.1, CC, from the media streams. */
std::vector<std::string> badges(const jf::Detail &d)
{
    std::vector<std::string> out;
    const jf::MediaStream *video = nullptr, *audio = nullptr;
    bool subs = false;
    for (const auto &s : d.streams) {
        if (s.type == "Video" && !video)
            video = &s;
        if (s.type == "Audio" && (!audio || s.is_default))
            audio = &s;
        if (s.type == "Subtitle")
            subs = true;
    }
    if (video) {
        if (video->width >= 3200)
            out.push_back("4K");
        else if (video->width >= 1200)
            out.push_back("HD");
        if (video->video_range_type.find("DOVI") != std::string::npos)
            out.push_back("Dolby Vision");
        else if (video->video_range == "HDR")
            out.push_back(video->video_range_type.find("HLG") != std::string::npos ? "HLG" : "HDR10");
    }
    if (audio) {
        if ((audio->display_title + audio->profile).find("Atmos") != std::string::npos)
            out.push_back("Atmos");
        else if (audio->channels >= 8)
            out.push_back("7.1");
        else if (audio->channels >= 6)
            out.push_back("5.1");
    }
    if (subs)
        out.push_back("CC");
    return out;
}

std::vector<jf::Person> cast_of(const jf::Detail &d)
{
    std::vector<jf::Person> out;
    std::set<std::string> seen;
    for (const auto &p : d.people)
        if ((p.type == "Actor" || p.type == "Director" || p.type == "GuestStar") && seen.insert(p.id).second &&
            out.size() < 24)
            out.push_back(p);
    return out;
}

/* Horizontal row scroll: the focused card at the left edge until the row ends. */
float row_target(int index, int count, float card_w, float gap)
{
    const float max_scroll = std::max(0.f, count * (card_w + gap) - gap - (gfx::W - 2 * kPad));
    return std::min(max_scroll, index * (card_w + gap));
}

} // namespace

/* Page data by item id: what was fetched (or prefetched) last, newest first. */
namespace {
std::mutex s_cache_lock;
std::list<std::string> s_cache_order;
std::map<std::string, std::shared_ptr<void>> s_cache;     /* holds Detail::Content */
unsigned s_cache_generation = 0;
std::set<std::string> s_inflight;

/* Per user: what one account has watched or liked never shows on another's page. */
std::string cache_key(const jf::Client &c, const std::string &id) { return c.server() + "\n" + c.user_id() + "/" + id; }
}

void Detail::invalidate_user_data()
{
    std::lock_guard<std::mutex> g(s_cache_lock);
    ++s_cache_generation;
    s_cache.clear();
    s_cache_order.clear();
    // In-flight requests remove their own keys when done, but cannot repopulate.
}

/* Everything the page shows, the requests in parallel. */
Detail::Content Detail::fetch(jf::Client &c, const jf::Item &base)
{
    Content out;
    out.item = base;
    jf::Detail detail;
    jf::Item item = base;
    bool got = false;
    const bool series = base.type == "Series", boxset = base.type == "BoxSet";
    std::vector<std::function<void()>> jobs;
    jobs.emplace_back([&] { got = c.item(base.id, &item, &detail); });
    if (boxset)
        jobs.emplace_back([&] { out.similar = c.children(base.id, "PremiereDate,ProductionYear,SortName", 200, &out.similar_ok); });
    else
        jobs.emplace_back([&] { out.similar = c.similar(base.id, 16, &out.similar_ok); });
    if (series) {
        jobs.emplace_back([&] { out.seasons = c.seasons(base.id, &out.seasons_ok); });
        jobs.emplace_back([&] { out.resume_items = c.resume(1, base.id, &out.resume_ok); });
        jobs.emplace_back([&] { out.next_items = c.next_up(1, base.id, &out.next_ok); });
        jobs.emplace_back([&] { out.available_episodes = c.episodes(base.id, {}, &out.episodes_ok); });
        jobs.emplace_back([&] { out.upcoming_episodes = c.upcoming_episodes(base.id, &out.upcoming_ok); });
    }
    emby5::run_io_jobs(jobs);
    if (got) {
        out.item = std::move(item);
        out.detail = std::move(detail);
        out.have_detail = true;
        out.extras_ok = out.item.special_features == 0;
        if (!out.extras_ok) out.extras = c.special_features(base.id, &out.extras_ok);
        out.trailers_ok = out.item.local_trailers == 0;
        if (!out.trailers_ok) {
            auto trailers = c.local_trailers(base.id, &out.trailers_ok);
            if (!trailers.empty()) { out.trailer = std::move(trailers.front()); out.have_trailer = true; }
        }
    }
    rebuild_content(out);
    return out;
}

void Detail::merge_content(Content &current, Content fresh)
{
    if (fresh.item.id != current.item.id) return;
    if (fresh.have_detail) {
        current.item = std::move(fresh.item);
        current.detail = std::move(fresh.detail);
        current.have_detail = true;
    }
    // A successful empty list clears old data. A failed request preserves that
    // section only; a first load with no previous data can still display others.
    if (fresh.seasons_ok) { current.seasons = std::move(fresh.seasons); current.seasons_ok = true; }
    if (fresh.episodes_ok) { current.available_episodes = std::move(fresh.available_episodes); current.episodes_ok = true; }
    if (fresh.upcoming_ok) { current.upcoming_episodes = std::move(fresh.upcoming_episodes); current.upcoming_ok = true; }
    if (fresh.similar_ok) { current.similar = std::move(fresh.similar); current.similar_ok = true; }
    if (fresh.extras_ok) { current.extras = std::move(fresh.extras); current.extras_ok = true; }
    if (fresh.trailers_ok) {
        current.trailer = std::move(fresh.trailer); current.have_trailer = fresh.have_trailer; current.trailers_ok = true;
    }
    if (fresh.resume_ok) { current.resume_items = std::move(fresh.resume_items); current.resume_ok = true; }
    if (fresh.next_ok) { current.next_items = std::move(fresh.next_items); current.next_ok = true; }
    rebuild_content(current);
}

void Detail::rebuild_content(Content &out)
{
    out.all_episodes = out.available_episodes;
    for (const jf::Item &e : out.upcoming_episodes) {
        const bool duplicate = std::any_of(out.all_episodes.begin(), out.all_episodes.end(),
            [&](const jf::Item &v) {
                return (!e.id.empty() && v.id == e.id) ||
                    (e.parent_index >= 0 && e.index >= 0 && v.parent_index == e.parent_index && v.index == e.index);
            });
        if (!duplicate) out.all_episodes.push_back(e);
    }
    std::stable_sort(out.all_episodes.begin(), out.all_episodes.end(), [](const jf::Item &a, const jf::Item &b) {
        return a.parent_index != b.parent_index ? a.parent_index < b.parent_index : a.index < b.index;
    });
    // Remove previous synthetic groups, then derive only missing groups from the
    // server's episode metadata (never invent an upcoming episode).
    out.seasons.erase(std::remove_if(out.seasons.begin(), out.seasons.end(),
        [](const jf::Item &s) { return s.type == "EpisodeGroup"; }), out.seasons.end());
    for (const auto &ep : out.all_episodes) {
        const bool grouped = std::any_of(out.seasons.begin(), out.seasons.end(), [&](const jf::Item &s) {
            return (!ep.season_id.empty() && ep.season_id == s.id) ||
                   (ep.parent_index >= 0 && ep.parent_index == s.index) ||
                   (ep.parent_index < 0 && ep.season_id.empty() && s.index < 0 && s.id.empty());
        });
        if (!grouped) {
            jf::Item group;
            group.type = "EpisodeGroup"; // UI grouping, not an item safe to mark watched.
            group.id = ep.season_id;
            group.index = ep.parent_index;
            group.name = !ep.season_name.empty() ? ep.season_name :
                ep.parent_index < 0 ? std::string(T("Episoder")) : TF("Season {number}", {{"number", std::to_string(ep.parent_index)}});
            out.seasons.push_back(std::move(group));
        }
    }
    std::stable_sort(out.seasons.begin(), out.seasons.end(), [](const jf::Item &a, const jf::Item &b) { return a.index < b.index; });
    out.have_target = false;
    out.target = {};
    const bool boxset = out.item.type == "BoxSet", series = out.item.type == "Series";
    if (boxset) {
        /* Play: the first title not yet watched, else the first. */
        for (const jf::Item &t : out.similar)
            if (!t.played) {
                out.target = t;
                out.have_target = true;
                break;
            }
        if (!out.have_target && !out.similar.empty()) {
            out.target = out.similar.front();
            out.have_target = true;
        }
    } else if (!series) {
        out.target = out.item;
        out.have_target = true;
    } else {
        // A resumed/Next Up item must be playable and belong to this series.
        auto take = [&](const std::vector<jf::Item> &items, bool skip_played = false) {
            for (const auto &e : items)
                if (!e.id.empty() && !e.upcoming && (!skip_played || !e.played) &&
                    (e.series_id.empty() || e.series_id == out.item.id)) {
                    out.target = e; out.have_target = true; return true;
                }
            return false;
        };
        if (!take(out.resume_items, true) && !take(out.next_items, true) && !take(out.all_episodes, true))
            take(out.all_episodes);
    }

}

static void cache_put(const std::string &key, const Detail::Content &content, unsigned generation);

void Detail::prefetch(jf::Client &client, const jf::Item &item)
{
    if (item.id.empty() || (item.type != "Movie" && item.type != "Series" && item.type != "BoxSet"))
        return;
    unsigned generation;
    {
        std::lock_guard<std::mutex> g(s_cache_lock);
        const std::string key = cache_key(client, item.id);
        if (s_cache.count(key) || s_inflight.count(key) || s_inflight.size() >= 2)
            return;
        s_inflight.insert(key);
        generation = s_cache_generation;
    }
    jf::Client *c = &client;
    const jf::Item base = item;
    const std::string key = cache_key(client, item.id);
    if (!emby5::spawn_io([c, base, generation, key] {
        Content content = fetch(*c, base);
        cache_put(key, content, generation);
        std::lock_guard<std::mutex> g(s_cache_lock);
        s_inflight.erase(key);
    }, [key] { std::lock_guard<std::mutex> g(s_cache_lock); s_inflight.erase(key); })) {
        std::lock_guard<std::mutex> g(s_cache_lock); s_inflight.erase(key);
    }
}

static void cache_put(const std::string &key, const Detail::Content &content, unsigned generation)
{
    std::lock_guard<std::mutex> g(s_cache_lock);
    if (generation != s_cache_generation) return;
    s_cache[key] = std::make_shared<Detail::Content>(content);
    s_cache_order.remove(key);
    s_cache_order.push_front(key);
    while (s_cache_order.size() > 40) {
        s_cache.erase(s_cache_order.back());
        s_cache_order.pop_back();
    }
}

Detail::Detail(jf::Client &client, const jf::Item &item) : m_client(client)
{
    m_data->c.item = item;
    {
        std::lock_guard<std::mutex> g(s_cache_lock);
        auto it = s_cache.find(cache_key(client, item.id));
        if (it != s_cache.end())
            m_data->c = *std::static_pointer_cast<Content>(it->second);   /* instant; refreshed below */
    }
    m_view = m_data->c;
    m_revision = m_data->revision;
    select_episodes();
}

void Detail::reconcile_user_data(const jf::Item &fresh)
{
    std::lock_guard<std::mutex> g(m_data->lock);
    auto update = [&](Content &c) {
        emby5::reconcile_user_data(c.item, fresh);
        emby5::reconcile_user_data(c.target, fresh);
        for (auto *items : {&c.seasons, &c.available_episodes, &c.upcoming_episodes,
                            &c.all_episodes, &c.resume_items, &c.next_items, &c.similar, &c.extras})
            for (auto &it : *items) emby5::reconcile_user_data(it, fresh);
    };
    ++m_data->generation; // an older request cannot undo the confirmed button action
    update(m_data->c);
    update(m_view);
    ++m_data->revision;
    m_revision = m_data->revision;
    select_episodes();
}

void Detail::activate()
{
    std::shared_ptr<Data> d = m_data;
    jf::Client *c = &m_client;
    const jf::Item base = m_view.item;
    unsigned generation, cache_generation;
    {
        std::lock_guard<std::mutex> g(d->lock);
        if (d->loading) { ++d->generation; d->refresh_again = true; return; }
        generation = ++d->generation;
        d->loading = true;
        d->refresh_again = false;
    }
    { std::lock_guard<std::mutex> g(s_cache_lock); cache_generation = s_cache_generation; }
    auto failed = [d] {
        std::lock_guard<std::mutex> g(d->lock);
        d->loading = false; d->seerr_pending = false;
    };
    if (!emby5::spawn_io([d, c, base, generation, cache_generation] {
        emby5_wait_reports(4000);
        Content fresh = fetch(*c, base);
        {
            std::lock_guard<std::mutex> g(d->lock);
            d->loading = false;
            if (generation != d->generation) return;
            merge_content(d->c, std::move(fresh));
            ++d->revision;
            cache_put(cache_key(*c, base.id), d->c, cache_generation);
        }
        if (base.type == "Series") look_up_in_seerr(d, base);
    }, failed)) failed();
}

/* The series in Seerr, by its TMDB id (or by its TVDB id, which Seerr's search
 * takes as "tvdb:<id>"). Nothing when Seerr is off or not signed in. */
void Detail::look_up_in_seerr(const std::shared_ptr<Data> &d, const jf::Item &series)
{
    std::shared_ptr<seerr::Client> sc = seerr_service::client();
    {
        std::lock_guard<std::mutex> g(d->lock);
        d->seerr_pending = sc && !(series.tmdb_id.empty() && series.tvdb_id.empty());
        if (!d->seerr_pending)
            return;   /* (pending ends on every way out) */
    }
    int tmdb = std::atoi(series.tmdb_id.c_str());
    if (tmdb <= 0)
        for (const seerr::Title &t : sc->search("tvdb:" + series.tvdb_id))
            if (t.tv) {
                tmdb = t.id;
                break;
            }
    seerr::Detail sd;
    const bool ok = tmdb > 0 && sc->tv(tmdb, &sd);
    if (!ok && (sc->last_status() == 401 || sc->last_status() == 403))
        seerr_service::session_lost();
    std::lock_guard<std::mutex> g(d->lock);
    d->seerr_pending = false;
    if (ok) {
        d->seerr = std::move(sd);
        d->have_seerr = true;
        ++d->revision;
    }
}

void Detail::select_episodes(bool preserve_selection)
{
    const std::string selected_id = preserve_selection && m_episode >= 0 && m_episode < (int)m_eps.size() ?
        m_eps[m_episode].id : std::string();
    m_eps.clear();
    if (m_view.seasons.empty())
        return;
    if(!m_requested_season.empty())for(size_t i=0;i<m_view.seasons.size();++i)if(m_view.seasons[i].id==m_requested_season){
        m_season=(int)i;m_season_picked=true;m_zone=Seasons;m_requested_season.clear();break;}
    m_season = std::max(0, std::min(m_season, (int)m_view.seasons.size() - 1));
    const jf::Item &season = m_view.seasons[m_season];
    for (const jf::Item &e : m_view.all_episodes)
        if ((!season.id.empty() && e.season_id == season.id) ||
            ((season.type == "EpisodeGroup" || e.season_id.empty()) && e.parent_index == season.index))
            m_eps.push_back(e);
    if (!selected_id.empty()) {
        const auto selected = std::find_if(m_eps.begin(), m_eps.end(), [&](const jf::Item &e) { return e.id == selected_id; });
        if (selected != m_eps.end()) m_episode = (int)(selected - m_eps.begin());
    }
    m_episode = std::clamp(m_episode, 0, std::max(0, (int)m_eps.size() - 1));
}

/* The picker moved: its season's episodes show at once, on the episode to
 * continue when it is in that season, else the first. */
void Detail::season_moved()
{
    select_episodes(false);
    m_episode = 0;
    m_episode_first = 0;
    for (size_t i = 0; i < m_eps.size(); i++)
        if (m_view.have_target && m_eps[i].id == m_view.target.id)
            m_episode = (int)i;
    const detail_layout::Episodes layout(gfx::W - 2 * kPad, gfx::text_scale(), theme_card_scale());
    m_episode_first = detail_layout::visible_first(m_episode, 0, (int)m_eps.size(), layout.capacity);
}

std::vector<Detail::Zone> Detail::zones() const
{
    std::vector<Zone> z{Buttons};
    if (!m_view.seasons.empty()) {
        z.push_back(Seasons);
        if (!m_eps.empty())
            z.push_back(Episodes);
    }
    if (!m_view.extras.empty())
        z.push_back(Extras);
    if (!cast_of(m_view.detail).empty())
        z.push_back(Cast);
    if (!m_view.similar.empty())
        z.push_back(Similar);
    return z;
}

bool Detail::can_ask_seerr() const
{
    if (!m_have_seerr || m_view.item.type != "Series")
        return false;
    const seerr_service::Snapshot s = seerr_service::snapshot();
    return s.state == seerr_service::State::Ready && RequestSheet::offers(m_seerr, s.user, s.settings);
}

/* The buttons never change with Seerr's answer: asking for more seasons is in
 * Options' menu (the PS5's way), not a button that comes and goes. */
std::vector<Detail::Button> Detail::buttons() const
{
    std::vector<Button> b;
    /* Play from the start for what can be played, so the row does not shift when
     * the page has loaded and knows what Play plays (it does nothing until then). */
    const std::string &t = m_view.item.type;
    const bool playable = t == "Movie" || t == "Series" || t == "Season" || t == "Episode" || t == "Video" ||
                          t == "MusicVideo";
    if (m_view.have_target || (!m_view.have_detail && playable))
        b.push_back(PlayButton);
    if (m_view.have_target && m_view.target.position_ticks > 0)
        b.push_back(RestartButton);
    if (m_view.have_trailer)
        b.push_back(TrailerButton);
    if (m_view.item.type != "BoxSet")
        b.push_back(WatchedButton);
    b.push_back(FavouriteButton);
    if (!first_trailer_url(m_view.item.remote_trailers).empty()) b.push_back(TrailerQrButton);
    return b;
}

/* The focus stays on the same button when the row changes under it; when that
 * button goes, on the first (Play), never on one to its right (a press there
 * could mark the series watched). */
void Detail::sync_button()
{
    const std::vector<Button> b = buttons();
    if (b.empty())
        return;
    if (m_button_id >= 0) {
        const auto it = std::find(b.begin(), b.end(), (Button)m_button_id);
        m_button = it != b.end() ? (int)(it - b.begin()) : 0;
    }
    m_button = std::max(0, std::min(m_button, (int)b.size() - 1));
    m_button_id = (int)b[m_button];
}

float Detail::zone_top(Zone z) const
{
    float y = kTopH;
    for (Zone k : zones()) {
        if (k == Buttons)
            continue;
        if (k == z)
            return y;
        y += k == Seasons ? detail_layout::seasons_height(gfx::text_scale(), m_eps.empty()) :
             k == Episodes ? detail_layout::Episodes(gfx::W - 2 * kPad, gfx::text_scale(), theme_card_scale()).section_height :
             k == Extras ? kExtrasH : k == Cast ? kCastH : kSimilarH;
    }
    return y;
}

/* Button actions request a server mutation; badges change after acknowledgement. */

bool Detail::spoiler_hidden(const jf::Item &item) const {
    return settings::hide_episode_spoilers(settings::get_appearance(),item) &&
        m_revealed_episodes.find(item.id)==m_revealed_episodes.end();
}

Action Detail::input(uint32_t p)
{
    Action a;
    if(m_read_more.active()){m_read_more.input(p);return a;}
    if (m_trailer_qr.active()) { m_trailer_qr.input(p); return a; }
    if (m_sheet.active()) {
        m_sheet.input(p);
        return a;
    }
    if (m_menu.active()) {   /* Options' menu: mark as seen, or ask Seerr for more seasons */
        m_menu.input(p, &a);
        if (m_menu.take_ask()) {
            const seerr_service::Snapshot s = seerr_service::snapshot();
            m_sheet.open(m_seerr, s.user, s.settings);
        }
        return a;
    }
    if(p&NUVIO_BTN_SQUARE) {
        if(m_zone==Buttons) {
            if(spoiler_hidden(m_view.item))m_revealed_episodes.insert(m_view.item.id);
            else m_read_more.open(m_view.item.name,m_view.item.overview);
        } else if(m_zone==Episodes && m_episode>=0 && m_episode<(int)m_eps.size()) {
            const auto &ep=m_eps[(size_t)m_episode];
            if(spoiler_hidden(ep))m_revealed_episodes.insert(ep.id);
            else m_read_more.open(ep.name,ep.overview);
        }
        return a;
    }
    sync_button();
    const std::vector<Zone> zs = zones();
    const auto zi = std::find(zs.begin(), zs.end(), m_zone) - zs.begin();
    const int nb = (int)buttons().size();
    if (p & NUVIO_BTN_DOWN) {
        if (zi + 1 < (long)zs.size())
            m_zone = zs[zi + 1];
    } else if (p & NUVIO_BTN_UP) {
        if (zi > 0)
            m_zone = zs[zi - 1];
    } else if (p & NUVIO_BTN_CIRCLE) {
        if (m_zone != Buttons)
            m_zone = Buttons;
        else
            a.kind = Action::Back;
    } else if (p & (NUVIO_BTN_LEFT | NUVIO_BTN_RIGHT)) {
        const int d = (p & NUVIO_BTN_RIGHT) ? 1 : -1;
        auto move = [d](int &i, int n) { i = std::max(0, std::min(n - 1, i + d)); };
        switch (m_zone) {
        case Buttons:
            move(m_button, nb);
            if (m_button < nb)
                m_button_id = (int)buttons()[m_button];
            break;
        case Seasons: {
            const int before = m_season;
            move(m_season, (int)m_view.seasons.size());
            m_season_picked = true;
            if (m_season != before)
                season_moved();
            break;
        }
        case Episodes:
            move(m_episode, (int)m_eps.size());
            m_season_picked = true;
            break;
        case Extras: move(m_extra, (int)m_view.extras.size()); break;
        case Cast: move(m_cast, (int)cast_of(m_view.detail).size()); break;
        case Similar: move(m_similar, (int)m_view.similar.size()); break;
        default: break;
        }
    } else if (p & NUVIO_BTN_OPTIONS) {   /* the focused part's menu */
        m_menu_zone = m_zone;
        if (m_zone == Episodes && m_episode < (int)m_eps.size() && !m_eps[m_episode].upcoming)
            m_menu.open_actions(m_eps[m_episode], true, can_ask_seerr());
        else if (m_zone == Seasons && m_season < (int)m_view.seasons.size() &&
                 m_view.seasons[m_season].type == "Season" && !m_view.seasons[m_season].id.empty())
            m_menu.open_actions(m_view.seasons[m_season], true, can_ask_seerr());
        else if (m_zone == Buttons)   /* "Merk som sett" is a button here: only Seerr's */
            m_menu.open_actions(m_view.item, false, can_ask_seerr());
    } else if (p & NUVIO_BTN_CROSS) {
        switch (m_zone) {
        case Buttons: {
            const std::vector<Button> b = buttons();
            if (b.empty()) break;
            const Button btn = b[std::max(0, std::min(m_button, (int)b.size() - 1))];
            if (btn == FavouriteButton || btn == WatchedButton) {
                if (m_view.item.id.empty()) break;
                if(btn==WatchedButton&&(m_view.item.type=="Series"||m_view.item.type=="Season")){
                    m_menu_zone=m_zone;m_menu.confirm_played(m_view.item);break;
                }
                /* Ask the app to write and reconcile this state with Emby. */
                UserDataChange &ch = a.change;
                ch.id = m_view.item.id;
                if (btn == FavouriteButton) {
                    ch.favorite_set = true;
                    ch.favorite = !m_view.item.favorite;
                } else {
                    ch.played_set = true;
                    ch.played = !m_view.item.played;
                }
                a.kind = Action::Changed;
                a.item = m_view.item;
                // No local watched-state guess; main confirms with Emby first.
            } else if (btn == TrailerQrButton) {
                m_trailer_qr.open(first_trailer_url(m_view.item.remote_trailers));
            } else if (btn == TrailerButton) {
                a.kind = Action::PlayFromStart;
                a.item = m_view.trailer;
            } else if (m_view.have_target) {
                a.kind = btn == RestartButton ? Action::PlayFromStart : Action::Play;
                a.item = m_view.target;
            }
            break;
        }
        case Seasons:
            m_season_picked = true;
            if (!m_eps.empty()) m_zone = Episodes;
            break;
        case Episodes:
            if (m_episode >= 0 && m_episode < (int)m_eps.size() && !m_eps[m_episode].upcoming) {
                a.kind = Action::Play;
                a.item = m_eps[m_episode];
            }
            break;
        case Extras:
            if (m_extra < (int)m_view.extras.size()) {
                a.kind = Action::PlayFromStart;
                a.item = m_view.extras[m_extra];
            }
            break;
        case Cast: {
            const std::vector<jf::Person> cast = cast_of(m_view.detail);
            if (m_cast < (int)cast.size()) {
                const jf::Person &p = cast[m_cast];
                a.kind = Action::Open;
                a.item.id = p.id;
                a.item.name = p.name;
                a.item.type = "Person";
                a.item.primary_tag = p.image_tag;
                a.item.primary_blurhash = p.blurhash;
            }
            break;
        }
        case Similar:
            if (m_similar < (int)m_view.similar.size()) {
                a.kind = Action::Open;
                a.item = m_view.similar[m_similar];
            }
            break;
        default:
            break;
        }
    }
    return a;
}

void Detail::draw_top(float y0, float dt)
{
    const jf::Item &it = m_view.item;
    const bool series = it.type == "Series";

    /* Logo (fades in; nothing until then) or the title. */
    const std::string logo = settings::get_appearance().title_as_text?std::string():m_client.image_url(it.logo_owner, "Logo", it.logo_tag, 900);
    const float title_bottom = y0 + 380;
    const gfx::Texture *logo_image=logo.empty()?nullptr:art::get(logo,900,900);
    if(logo_image && gfx::texture_width(logo_image)>0 && gfx::texture_height(logo_image)>0) {
        const float iw=(float)gfx::texture_width(logo_image),ih=(float)gfx::texture_height(logo_image);
        const float k=std::min(800.f/iw,210.f/ih);
        gfx::image({kPad,title_bottom-ih*k,iw*k,ih*k},logo_image,art::fade(logo),0,false);
    } else gfx::text(kPad,title_bottom-20,it.name,{gfx::Bold,74,1360},kText);
    if(settings::get_appearance().detail_poster && !spoiler_hidden(it))
        art::draw({gfx::W-kPad-200,y0+215,200,300},poster_url(m_client,it,400),it.primary_blurhash,400,600,14,1.f);

    /* Meta: rating, year, runtime or seasons, genres, age rating, tech badges. */
    const float my = y0 + 440;
    float x = kPad;
    const gfx::TextStyle meta{gfx::Medium, 24};
    bool first = true;
    auto sep = [&] {
        if (!first) {
            gfx::fill({x + 12, my - 10, 5, 5}, kText3, 2.5f);
            x += 29;
        }
        first = false;
    };
    if (it.community_rating > 0) {
        char r[16];
        std::snprintf(r, sizeof r, "\xE2\x98\x85 %.1f", it.community_rating);
        sep();
        x += gfx::text(x, my, r, meta, 0xfff5c518u);
    }
    if (it.year) {
        sep();
        x += gfx::text(x, my, std::to_string(it.year), meta, kText2);
    }
    if (series && !m_view.seasons.empty()) {
        sep();
        const size_t n = m_view.seasons.size();
        x += gfx::text(x, my, std::to_string(n) + (n == 1 ? T(" sesong") : T(" sesonger")), meta, kText2);
    } else if (!series && it.runtime_ticks > 0) {
        sep();
        x += gfx::text(x, my, runtime_label(it.runtime_ticks), meta, kText2);
    }
    if (!it.genres.empty()) {
        sep();
        std::string g = it.genres[0];
        for (size_t i = 1; i < it.genres.size() && i < 3; i++)
            g += " \xC2\xB7 " + it.genres[i];
        x += gfx::text(x, my, g, meta, kText2);
    }
    std::vector<std::string> tags;
    if (!it.official_rating.empty())
        tags.push_back(it.official_rating);
    for (const auto &b : badges(m_view.detail))
        tags.push_back(b);
    x += 16;
    for (const auto &tag : tags) {
        const gfx::TextStyle bs{gfx::Bold, 17};
        const float bw = gfx::text_width(tag, bs) + 18;
        const bool solid = tag == "4K" || tag == "Dolby Vision";
        gfx::fill({x, my - 22, bw, 30}, solid ? 0xe6ffffffu : 0x73ffffffu, 6);
        if (!solid)
            gfx::fill({x + 1.5f, my - 20.5f, bw - 3, 27}, 0xd90d0d12u, 5);
        gfx::text(x + 9, my - 1, tag, bs, solid ? 0xff000000u : kText);
        x += bw + 10;
    }

    float y = my + 52;
    if (!m_view.detail.tagline.empty() && !spoiler_hidden(it)) {
        gfx::text(kPad, y, m_view.detail.tagline, {gfx::Medium, 26, 900}, alpha(kText,.9f));
        y += 44;
    }
    const gfx::TextStyle overview_style{gfx::Regular, 26};
    const float ui_scale = gfx::text_scale();
    const float line_height = 37.7f * ui_scale;
    // Keep the synopsis viewport and buttons fixed even at large text sizes.
    const size_t visible_lines = ui_scale > 1.0f ? 2u : 3u;
    const std::string description=spoiler_hidden(it)?T("Episode details hidden. Press Square to reveal without marking watched."):it.overview;
    if (m_overview_text != description || m_overview_scale != ui_scale) {
        m_overview_scale = ui_scale;
        m_overview_text = description;
        m_overview_lines = overview::wrap(description, 860.f, [&](const std::string &text) {
            return gfx::measure_text(text, overview_style);
        });
        m_overview_scroll.reset();
    }
    const bool reading = m_zone == Buttons && !modal() && std::fabs(m_page.value)<1.f;
    if (m_overview_scroll.step(dt, m_overview_lines.size(), reading, line_height, visible_lines, theme_description_speed())) m_animating = true;
    const float scroll = m_overview_scroll.offset();
    const float end = m_overview_lines.size()>visible_lines ? (m_overview_lines.size()-visible_lines)*line_height : 0.f;
    const gfx::Rect viewport{kPad-2, y-29*ui_scale, 864, visible_lines*line_height+4};
    gfx::push_scissor(viewport);
    gfx::push_fade_mask(viewport, scroll>0 ? 7.f : 0.f, scroll<end ? 7.f : 0.f);
    const size_t first_line = static_cast<size_t>(scroll/line_height);
    for (size_t i=first_line; i<m_overview_lines.size() && i<first_line+4; ++i)
        gfx::text(kPad, y + i*line_height - scroll, m_overview_lines[i], overview_style, kText2);
    gfx::pop_fade_mask();
    gfx::pop_scissor();

    gfx::text(kPad,y0+648,spoiler_hidden(it)?T("Square: Reveal episode details"):T("Square: Read more"),{gfx::Medium,19,860},kText2);
    /* Buttons. */
    const float by = y0 + 668;
    sync_button();
    const std::vector<Button> bs = buttons();
    /* Glass buttons: the panes, then the focus drop over them, then their labels. */
    if (m_zone != Buttons || m_sheet.active())
        m_btn_drop.hide();
    for (int pass = 0; pass < 2; pass++) {
    if (pass == 1)
        m_btn_drop.draw(dt, 1.f, &m_animating, 16);
    float bx = kPad;
    for (size_t i = 0; i < bs.size(); i++) {
        const bool focus = m_zone == Buttons && !m_sheet.active() && (int)i == std::min(m_button, (int)bs.size() - 1);
        const gfx::TextStyle st{gfx::Bold, 26};
        std::string label, sub;
        float pct = -1;
        if (bs[i] == PlayButton) {
            const jf::Item &t = m_view.target;
            const bool resume = m_view.have_target && t.position_ticks > 0 && t.runtime_ticks > 0;
            label = resume ? T("Fortsett") : T("Spill av");
            if (m_view.have_target && t.type == "Episode")
                label += "  " + ep_code(t);
            if (resume) {
                pct = (float)t.position_ticks / (float)t.runtime_ticks;
                const int left = (int)((t.runtime_ticks - t.position_ticks) / jf::kTicksPerSecond / 60);
                sub = std::to_string(std::max(1, left)) + T(" min igjen");
            }
        }
        float w = 76;
        if (bs[i] == WatchedButton)
            w = gfx::text_width(m_view.item.played ? T("Sett") : T("Merk som sett"), st) + 64 + 34;
        if (bs[i] == RestartButton)
            w = gfx::text_width(T("Fra start"), st) + 64;
        if (bs[i] == TrailerButton)
            w = gfx::text_width(T("Trailer"), st) + 64;
        if (bs[i] == TrailerQrButton)
            w = gfx::text_width(T("Trailer QR"), st) + 52;
        if (bs[i] == PlayButton)
            w = 40 + 30 + 14 + gfx::text_width(label, st) + (pct >= 0 ? 14 + 90 + 14 + gfx::text_width(sub, {gfx::Medium, 24}) : 0) + 40;
        const float k = 1.f;
        const gfx::Rect r{bx, by, w, 76};
        if (pass == 0) {
            glass_panel(r, 16, 1.f, false);
            if (focus)
                m_btn_drop.to(r, (int)bs[i], 0, by);
            bx += w + 20;
            continue;
        }
        const uint32_t fg = kText;
        const float cy = r.y + r.h / 2;
        if (bs[i] == PlayButton) {
            draw_detail_icon(DetailIcon::Play, {r.x + 36*k, cy-16, 32, 32}, fg);
            float tx = r.x + (40 + 30 + 14) * k;
            tx += gfx::text(tx, cy + 9, label, st, fg);
            if (pct >= 0) {
                tx += 14;
                gfx::fill({tx, cy - 3, 90, 6}, 0x40ffffffu, 3);
                gfx::fill({tx, cy - 3, 90 * pct, 6}, fg, 3);
                gfx::text(tx + 104, cy + 8, sub, {gfx::Medium, 24}, alpha(fg, 0.75f));
            }
        } else if (bs[i] == RestartButton) {
            gfx::text(r.x + r.w / 2, cy + 9, T("Fra start"), st, fg, 1);
        } else if (bs[i] == TrailerQrButton) {
            gfx::text(r.x+r.w/2,cy+9,T("Trailer QR"),st,fg,1);
        } else if (bs[i] == TrailerButton) {
            gfx::text(r.x + r.w / 2, cy + 9, T("Trailer"), st, fg, 1);
        } else if (bs[i] == WatchedButton) {
            const bool seen = m_view.item.played;
            const uint32_t cc = seen ? 0xff30d158u : alpha(fg, 0.85f);
            draw_detail_icon(DetailIcon::Check, {r.x+26*k, cy-16, 32, 32}, cc);
            gfx::text(r.x + 30 * k + 34, cy + 9, seen ? T("Sett") : T("Merk som sett"), st, fg);
        } else {
            const uint32_t hc = m_view.item.favorite ? 0xffff5a7au : alpha(fg, 0.85f);
            draw_detail_icon(DetailIcon::Heart, {r.x+r.w/2-16, cy-16, 32, 32}, hc);
        }
        bx += w + 20;
    }
    }

    /* Credits. */
    std::string with, dir;
    int actors = 0;
    for (const auto &p : m_view.detail.people) {
        if (p.type == "Actor" && actors < 4)
            with += (actors++ ? ", " : "") + p.name;
        if (p.type == "Director" && dir.size() < 80)
            dir += (dir.empty() ? "" : ", ") + p.name;
    }
    float cy = by + 76 + 46;
    const gfx::TextStyle cs{gfx::Regular, 21, 1100};
    auto credit = [&](const char *head, const std::string &v) {
        if (v.empty())
            return;
        const float hw = gfx::text(kPad, cy, head, {gfx::SemiBold, 21}, kText2);
        gfx::text(kPad + hw + 8, cy, v, cs, kText3);
        cy += 32;
    };
    credit(T("Med:"), with);
    credit(T("Regi:"), dir);
    if (!m_view.detail.studios.empty())
        credit(series ? T("Kanal:") : T("Studio:"), m_view.detail.studios[0]);
}

void Detail::draw_sections(float dt)
{
    const float off = m_page.value;
    const auto vis = [](float y, float h) { return y < gfx::H + 20 && y + h > -40; };

    const float scale = detail_layout::text_scale(gfx::text_scale());
    const float viewport_width = gfx::W - 2 * kPad;

    // One themed control per season, not a nested glass bar + bouncing drop.
    // Focus and selection are distinct: when browsing episodes the selected
    // season stays tinted, but only the episode owns the focus outline.
    if (!m_view.seasons.empty()) {
        const float y = zone_top(Seasons) - off;
        if (vis(y, detail_layout::seasons_height(scale, m_eps.empty()))) {
            gfx::text(kPad, y + 29.f * scale, T("Sesong"), {gfx::Bold, 26}, kText);
            if (m_zone == Seasons && m_season >= 0 && m_season < (int)m_view.seasons.size() &&
                m_view.seasons[m_season].type == "Season" && !m_view.seasons[m_season].id.empty()) {
                const std::string hint = T("Valg");
                draw_pad_hint(gfx::W - kPad - pad_hint_width(PadButton::Options, hint, 24),
                              y + 20.f * scale, PadButton::Options, hint, 24);
            }
            const float tab_y = y + 48.f * scale, tab_h = 54.f * scale;
            const gfx::Rect view{kPad, tab_y - 10, viewport_width, tab_h + 20};
            constexpr float gutter = 10.f, gap = 14.f;
            const float inner = viewport_width - 2.f * gutter;
            std::vector<std::string> labels;
            std::vector<float> widths, starts;
            float total = 0;
            for (const auto &season : m_view.seasons) {
                const std::string label = !season.name.empty() ? season.name : season.index >= 0 ?
                    std::string(T("Sesong")) + " " + std::to_string(season.index) : T("Episoder");
                const float w = std::clamp(gfx::measure_text(label, {gfx::Bold, 23}) + 64.f * scale,
                                            132.f * scale, inner);
                labels.push_back(label); starts.push_back(total); widths.push_back(w); total += w + gap;
            }
            total = std::max(0.f, total - gap);
            const size_t selected = (size_t)std::clamp(m_season, 0, (int)labels.size() - 1);
            const float sx = detail_layout::tab_scroll(starts[selected], widths[selected], inner, total,
                                                       m_scroll[Seasons].value);
            // Keep the entire newly selected control inside the viewport. This
            // avoids clipping an outline while a separate highlight catches up.
            m_scroll[Seasons].snap(sx);
            gfx::push_scissor(view);
            for (size_t i = 0; i < labels.size(); ++i) {
                const gfx::Rect r{kPad + gutter + starts[i] - sx, tab_y, widths[i], tab_h};
                if (r.x + r.w <= kPad || r.x >= gfx::W - kPad) continue;
                const bool chosen = i == selected;
                glass_panel(r, 16, 1.f, false, chosen ? 1.f : 0.f);
                if (chosen && m_zone == Seasons) draw_focus_outline(r, 16);
                gfx::text(r.x + r.w * .5f, r.y + r.h * .5f + 8.f * scale, labels[i],
                          {chosen ? gfx::Bold : gfx::SemiBold, 23, r.w - 48.f * scale}, kText, 1);
            }
            gfx::pop_scissor();
            if (total > inner) {
                char range[80];
                std::snprintf(range, sizeof range, "%s%d / %zu%s", sx > .5f ? "<  " : "",
                              m_season + 1, labels.size(), sx + inner < total - .5f ? "  >" : "");
                gfx::text(kPad + viewport_width * .5f, y + 27.f * scale, range,
                          {gfx::Medium, 20, viewport_width * .5f}, kText2, 1);
            }
            if (m_eps.empty()) {
                gfx::text(kPad + gutter, tab_y + tab_h + 42.f * scale,
                          T("No episodes available for this season."), {gfx::Regular, 22, inner}, kText2);
            }
        }
    }

    // The episode row uses a whole-card window. No clipped earlier episode
    // remnants against the screen edge; metadata never overlaps the next row.
    if (!m_eps.empty()) {
        const float y = zone_top(Episodes) - off;
        const detail_layout::Episodes layout(viewport_width, scale, theme_card_scale());
        const int count = (int)m_eps.size();
        m_episode_first = detail_layout::visible_first(m_episode, m_episode_first, count, layout.capacity);
        const int last = std::min(count, m_episode_first + layout.capacity);
        if (vis(y, layout.section_height)) {
            gfx::text(kPad, y + 26.f * scale, T("Episoder"), {gfx::Bold, 26}, kText);
            char range[80];
            std::snprintf(range, sizeof range, "%s%d-%d / %d%s", m_episode_first > 0 ? "<  " : "",
                          m_episode_first + 1, last, count, last < count ? "  >" : "");
            gfx::text(gfx::W - kPad, y + 26.f * scale, range, {gfx::Medium, 20, viewport_width * .45f}, kText2, 2);
            gfx::push_scissor({kPad, y + 30.f * scale, viewport_width, layout.section_height - 30.f * scale});
            for (int pass = 0; pass < 2; ++pass) {
                for (int i = m_episode_first; i < last; ++i) {
                    const jf::Item &e = m_eps[(size_t)i];
                    const bool focus = m_zone == Episodes && i == m_episode;
                    if ((pass == 0) == focus) continue;
                    const float x = kPad + layout.inset + (i - m_episode_first) * (layout.width + layout.gap);
                    const float lift = m_lifts.step("ep" + e.id, focus, dt, &m_animating);
                    const float k = 1.f + 0.1f * theme_poster_lift(lift);
                    const gfx::Rect r{x - layout.width * (k - 1) / 2,
                        y + layout.image_y - layout.height * (k - 1) / 2, layout.width * k, layout.height * k};
                    if (lift > 0.01f) gfx::shadow(r, 14 * k, 26, 0.3f * lift, 10 * lift);
                    const bool hidden = spoiler_hidden(e);
                    if (hidden) {
                        gfx::fill(r, theme_palette().panel, 14 * k);
                        gfx::text(r.x + r.w / 2, r.y + r.h / 2, T("Unwatched episode"), {gfx::Medium, 23, r.w - 24}, kText, 1);
                    } else art::draw(r, landscape_url(m_client, e, 640), e.primary_blurhash, 640, 360, 14 * k);
                    if (focus) draw_image_focus_outline(r, 14 * k);
                    if (e.upcoming || e.played) {
                        const std::string badge = e.upcoming ? T("Upcoming") : T("Sett");
                        const float bw = std::min(r.w - 28.f, gfx::measure_text(badge, {gfx::SemiBold, 18}) + 28.f);
                        const float bh = 32.f * scale;
                        const gfx::Rect br{r.x + r.w - bw - 14, r.y + 14, bw, bh};
                        gfx::fill(br, 0xcc000000u, bh / 2);
                        gfx::text(br.x + bw / 2, br.y + bh / 2 + 6.f * scale, badge,
                                  {gfx::SemiBold, 18, bw - 20}, 0xfff5f7fbu, 1);
                    }
                    if (!e.upcoming && e.played_percent > 0 && e.played_percent < 100) {
                        gfx::fill({r.x + 18, r.y + r.h - 22, r.w - 36, 6}, 0x47ffffffu, 3);
                        gfx::fill({r.x + 18, r.y + r.h - 22, (r.w - 36) * (float)(e.played_percent / 100), 6}, 0xffffffffu, 3);
                    }
                    const std::string title = (e.index >= 0 ? std::to_string(e.index) + ". " : "") + e.name;
                    gfx::text(x, y + layout.title_y, title, {gfx::SemiBold, 22, layout.width}, focus ? kText : kText2);
                    gfx::text(x, y + layout.runtime_y, e.upcoming ?
                              (i18n::date(e.premiere_date).empty() ? std::string(T("Upcoming")) : TF("Airs {date}", {{"date", i18n::date(e.premiere_date)}})) : runtime_label(e.runtime_ticks),
                              {gfx::Medium, 19, layout.width}, kText3);
                    gfx::text(x, y + layout.synopsis_y, hidden ? T("Square: Reveal details") : e.overview,
                              {gfx::Regular, 19, layout.width, 3, 27}, kText3);
                }
            }
            gfx::pop_scissor();
        }
    }

    /* Extras: landscape cards, Cross plays. */
    if (!m_view.extras.empty()) {
        const float y = zone_top(Extras) - off;
        m_scroll[Extras].to(row_target(m_extra, (int)m_view.extras.size(), kSimW, kSimGap));
        if (m_scroll[Extras].step(dt, 12.f))
            m_animating = true;
        if (vis(y, kExtrasH)) {
            gfx::text(kPad, y + 30, T("Ekstramateriale"), {gfx::Bold, 30}, alpha(kText,.92f));
            for (int pass = 0; pass < 2; pass++)
                for (size_t i = 0; i < m_view.extras.size(); i++) {
                    const jf::Item &e = m_view.extras[i];
                    const bool focus = m_zone == Extras && (int)i == m_extra;
                    if ((pass == 0) == focus)
                        continue;
                    const float x = kPad + i * (kSimW + kSimGap) - m_scroll[Extras].value;
                    if (x > gfx::W || x + kSimW < -40)
                        continue;
                    const float lift = m_lifts.step("x" + e.id, focus, dt, &m_animating);
                    const float k = 1.f + 0.1f * theme_poster_lift(lift), cy = y + 60;
                    const gfx::Rect r{x - kSimW * (k - 1) / 2, cy - kSimH * (k - 1) / 2, kSimW * k, kSimH * k};
                    if (lift > 0.01f)
                        gfx::shadow(r, 14 * k, 26, 0.3f * lift, 10 * lift);
                    /* Its own still, else the title's backdrop. */
                    const jf::Item &it = m_view.item;
                    if (!e.primary_tag.empty())
                        art::draw(r, m_client.image_url(e.id, "Primary", e.primary_tag, 640), e.primary_blurhash, 640,
                                  360, 14 * k);
                    else
                        art::draw(r, m_client.image_url(it.backdrop_owner, "Backdrop", it.backdrop_tag, 640),
                                  it.backdrop_blurhash, 640, 360, 14 * k);
                    gfx::text(x, cy + kSimH + 40, e.name, {gfx::SemiBold, 21, kSimW}, focus ? kText : kText2);
                }
        }
    }

    /* Cast and crew: round portraits. */
    const std::vector<jf::Person> cast = cast_of(m_view.detail);
    if (!cast.empty()) {
        const float y = zone_top(Cast) - off;
        m_scroll[Cast].to(row_target(m_cast, (int)cast.size(), kCastD, kCastGap));
        if (m_scroll[Cast].step(dt, 12.f))
            m_animating = true;
        if (vis(y, kCastH)) {
            gfx::text(kPad, y + 30, T("Skuespillere og crew"), {gfx::Bold, 30}, alpha(kText,.92f));
            for (size_t i = 0; i < cast.size(); i++) {
                const jf::Person &p = cast[i];
                const float x = kPad + i * (kCastD + kCastGap) - m_scroll[Cast].value;
                if (x > gfx::W || x + kCastD < -40)
                    continue;
                const bool focus = m_zone == Cast && (int)i == m_cast;
                const float lift = m_lifts.step("cast" + p.id, focus, dt, &m_animating);
                const float k = 1.f + 0.1f * theme_poster_lift(lift);
                const float d = kCastD * k;
                const gfx::Rect r{x - (d - kCastD) / 2, y + 56 - (d - kCastD) / 2, d, d};
                const std::string url =
                    p.image_tag.empty() ? std::string() : m_client.image_url(p.id, "Primary", p.image_tag, 340);
                if (url.empty()) {
                    gfx::fill(r, 0xff1a1a20u, d / 2);
                    gfx::text(r.x + d / 2, r.y + d / 2 + 16, p.name.substr(0, 1), {gfx::Bold, 46}, kText3, 1);
                } else {
                    art::draw(r, url, p.blurhash, 340, 340, d / 2);
                }
                gfx::text(x + kCastD / 2, y + 56 + kCastD + 40, p.name, {gfx::SemiBold, 19, kCastD + 20},
                          focus ? kText : kText2, 1);
                gfx::text(x + kCastD / 2, y + 56 + kCastD + 66, p.role.empty() ? p.type : p.role,
                          {gfx::Medium, 17, kCastD + 20}, kText3, 1);
            }
        }
    }

    /* More like this: landscape cards that open their own page. */
    if (!m_view.similar.empty()) {
        const float y = zone_top(Similar) - off;
        m_scroll[Similar].to(row_target(m_similar, (int)m_view.similar.size(), kSimW, kSimGap));
        if (m_scroll[Similar].step(dt, 12.f))
            m_animating = true;
        if (vis(y, kSimilarH)) {
            gfx::text(kPad, y + 30, m_view.item.type == "BoxSet" ? T("I denne samlingen") : T("Mer som dette"),
                      {gfx::Bold, 30}, alpha(kText,.92f));
            for (int pass = 0; pass < 2; pass++)
                for (size_t i = 0; i < m_view.similar.size(); i++) {
                    const jf::Item &s = m_view.similar[i];
                    const bool focus = m_zone == Similar && (int)i == m_similar;
                    if ((pass == 0) == focus)
                        continue;
                    const float x = kPad + i * (kSimW + kSimGap) - m_scroll[Similar].value;
                    if (x > gfx::W || x + kSimW < -40)
                        continue;
                    const float lift = m_lifts.step("sim" + s.id, focus, dt, &m_animating);
                    const float k = 1.f + 0.1f * theme_poster_lift(lift);
                    const gfx::Rect r{x - kSimW * (k - 1) / 2, y + 56 - kSimH * (k - 1) / 2, kSimW * k, kSimH * k};
                    if (lift > 0.01f)
                        gfx::shadow(r, 14 * k, 26, 0.3f * lift, 10 * lift);
                    if(spoiler_hidden(s)) {
                        gfx::fill(r,theme_palette().panel,14*k);
                        gfx::text(r.x+r.w/2,r.y+r.h/2,T("Unwatched episode"),{gfx::Medium,23,r.w-24},kText,1);
                    } else art::draw(r, landscape_url(m_client, s, 640), landscape_blurhash(s), 640, 360, 14 * k);
                    if(focus)draw_image_focus_outline(r,14*k);
                    if (lift > 0.01f)
                        gfx::text(r.x, r.y + r.h + 36, s.name, {gfx::SemiBold, 22, r.w}, alpha(kText, lift));
                }
        }
    }
}

void Detail::draw(double now, float dt)
{
    m_now = now;
    m_animating = false;
    if (m_opened < 0)
        m_opened = now;
    m_enter.to(1.f);
    if (m_enter.step(dt, 14.f))
        m_animating = true;
    bool seerr_pending, refresh_again, loading, content_changed = false;
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        if (m_revision != m_data->revision) {
            const std::string selected_id = m_season >= 0 && m_season < (int)m_view.seasons.size()
                ? m_view.seasons[m_season].id : std::string();
            const int selected_index = m_season >= 0 && m_season < (int)m_view.seasons.size()
                ? m_view.seasons[m_season].index : -1;
            m_view = m_data->c;
            for (size_t i = 0; m_season_picked && i < m_view.seasons.size(); ++i)
                if ((!selected_id.empty() && m_view.seasons[i].id == selected_id) ||
                    (selected_id.empty() && m_view.seasons[i].index == selected_index)) {
                    m_season = (int)i; break;
                }
            m_seerr = m_data->seerr;
            m_have_seerr = m_data->have_seerr;
            m_revision = m_data->revision;
            content_changed = true;
        }
        seerr_pending = m_data->seerr_pending;
        loading = m_data->loading;
        refresh_again = m_data->refresh_again && !loading;
    }
    if (refresh_again) activate();
    if (loading || refresh_again) m_animating = true;
    /* Seerr signed in after the page loaded (just after launch, say): look the
     * series up now, once, so its Options menu offers more seasons. */
    if (!m_have_seerr && !seerr_pending && !m_seerr_asked_late && m_view.item.type == "Series" &&
        !m_view.item.id.empty() && seerr_service::ready()) {
        m_seerr_asked_late = true;
        std::shared_ptr<Data> d = m_data;
        const jf::Item series = m_view.item;
        if (!emby5::spawn_io([d, series] { look_up_in_seerr(d, series); }, [d] {
            std::lock_guard<std::mutex> g(d->lock); d->seerr_pending = false;
        })) { std::lock_guard<std::mutex> g(d->lock); d->seerr_pending = false; }
    }
    if (seerr_pending)
        m_animating = true;   /* the request button shows as soon as Seerr answers */
    seerr::RequestResult done;
    if (m_sheet.take_done(&done)) {   /* say how it went, and ask Seerr again (seasons moved) */
        m_note = request_note(done);
        m_note_dot = request_note_dot(done);
        m_note_at = now;
        std::shared_ptr<Data> d = m_data;
        const jf::Item series = m_view.item;
        if (!emby5::spawn_io([d, series] { look_up_in_seerr(d, series); }, [d] {
            std::lock_guard<std::mutex> g(d->lock); d->seerr_pending = false;
        })) { std::lock_guard<std::mutex> g(d->lock); d->seerr_pending = false; }   /* no thread: the button stays as it was */
    }
    const jf::Item &it = m_view.item;

    /* Seasons: until the viewer moves the picker, follow the season and
     * episode to continue; episodes come from the page's own data. */
    if (!m_view.seasons.empty()) {
        if (!m_season_picked && m_view.have_target && m_view.target.id != m_followed &&
            !m_view.all_episodes.empty()) {
            /* Once per target (and again after a playback changes it). */
            for (size_t i = 0; i < m_view.seasons.size(); i++)
                if (m_view.seasons[i].id == m_view.target.season_id)
                    m_season = (int)i;
            season_moved();
            m_followed = m_view.target.id;
        } else if (content_changed) {
            select_episodes();
        }
        m_episode = std::min(m_episode, std::max(0, (int)m_eps.size() - 1));
    }
    if (m_view.seasons.empty()) m_eps.clear();
    const std::vector<Zone> zs = zones();
    if (std::find(zs.begin(), zs.end(), m_zone) == zs.end())
        m_zone = Buttons;

    /* Keep the season selector above its episodes while browsing that row. */
    const float page_target = m_zone == Buttons ? 0.f : m_zone == Episodes && !m_view.seasons.empty() ?
        zone_top(Seasons) - 120.f : zone_top(m_zone) - 160.f;
    m_page.to(std::max(0.f, page_target));
    if (m_page.step(dt, 10.f))
        m_animating = true;

    const gfx::Rect full{0, 0, gfx::W, gfx::H};
    gfx::fill(full, kBg);
    if(theme_artwork_enabled() && !spoiler_hidden(it)) art::draw(full, m_client.image_url(it.backdrop_owner, "Backdrop", it.backdrop_tag, 1920), it.backdrop_blurhash,
              1920, 1080, 0, 1.f, kBg);
    gfx::fill_hgradient({0, 0, 576, gfx::H}, alpha(kBg, 0.92f), alpha(kBg, 0.72f));
    gfx::fill_hgradient({576, 0, 538, gfx::H}, alpha(kBg, 0.72f), alpha(kBg, 0.2f));
    gfx::fill_hgradient({1114, 0, 326, gfx::H}, alpha(kBg, 0.2f), alpha(kBg, 0.f));
    gfx::fill_vgradient({0, 486, gfx::W, 356}, alpha(kBg, 0.f), alpha(kBg, 0.85f));
    gfx::fill_vgradient({0, 842, gfx::W, 238}, alpha(kBg, 0.85f), kBg);
    gfx::fill(full, alpha(kBg, std::min(0.55f, m_page.value / 700.f)));
    if(theme_artwork_enabled())gfx::fill(full,alpha(kBg,theme_backdrop_dim(0.f)));

    /* The text waits for the details (and the logo) so nothing is swapped in
     * front of the viewer; at most 0.6 s, then it fades in as one. */
    const std::string logo = settings::get_appearance().title_as_text?std::string():m_client.image_url(it.logo_owner, "Logo", it.logo_tag, 900);
    const bool logo_ready = logo.empty() || art::get(logo, 900, 900);
    if ((m_view.have_detail && logo_ready) || now - m_opened > 0.6)
        m_content.to(1.f);
    if (m_content.step(dt, 12.f) || m_content.target < 1.f)
        m_animating = true;
    gfx::push_opacity(m_content.value);
    draw_top(-m_page.value, dt);
    draw_sections(dt);
    gfx::pop_opacity();
    m_note_a.to(now - m_note_at < 4.0 ? 1.f : 0.f);
    if (m_note_a.step(dt, 10.f) || m_note_a.target > 0)
        m_animating = true;
    draw_note(m_note, m_note_a.value, m_note_dot);
    m_menu.draw(dt, &m_animating);
    m_sheet.draw(dt, &m_animating);
    m_trailer_qr.draw();
    m_read_more.draw();
    if (art::animating())
        m_animating = true;
}

} // namespace ui
