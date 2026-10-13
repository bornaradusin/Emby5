/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/search.h"
#include "ui/browse_grid.h"
#include "ui/search_text.h"
#include "app/io_worker.h"
#include "app/browse_state.h"
#include "app/seerr_service.h"
#include "app/user_data_update.h"
#include "platform/ime.h"
#include "gfx/art.h"
#include <algorithm>
#include <set>

namespace ui {
namespace {
bool accepts_type(const std::string &types, const std::string &type) {
    return ("," + types + ",").find("," + type + ",") != std::string::npos;
}
}
Search::Search(jf::Client &client, SearchContext context, std::string query, bool from_nav)
    : m_client(client), m_context(std::move(context)), m_account(browse_state::scope(client)),
      m_query(search_text(std::move(query))), m_from_nav(from_nav) {}
Search::~Search() {
    std::lock_guard<std::mutex> lock(m_data->lock);
    m_data->alive = false; ++m_data->generation;
}
void Search::activate() { if (!m_started) { m_started = true; start(false); } }
bool Search::needs_redraw() const {
    std::lock_guard<std::mutex> lock(m_data->lock);
    return m_revision != m_data->revision || m_data->query_ready;
}
void Search::refresh_user_data() { if (m_started) start(false, true); }
void Search::reconcile_user_data(const jf::Item &fresh) {
    std::lock_guard<std::mutex> lock(m_data->lock);
    auto copy = std::make_shared<std::vector<jf::Item>>(*m_data->items);
    for (auto &item : *copy) emby5::reconcile_user_data(item, fresh);
    m_data->items = std::move(copy); ++m_data->revision;
}
void Search::edit_query() {
    const auto data = m_data;
    ime::request(ime::Kind::Search, "Search " + m_context.title, m_query,
        [data](const std::string &q) {
            std::lock_guard<std::mutex> lock(data->lock);
            if (data->alive) { data->query = search_text(q); data->query_ready = true; ++data->revision; }
        });
}
void Search::consume_query() {
    bool ready = false;
    {
        std::lock_guard<std::mutex> lock(m_data->lock);
        if (m_data->query_ready) {
            m_query = std::move(m_data->query); m_data->query_ready = false; ready = true;
        }
    }
    if (ready) { m_index = 0; m_anchor.clear(); m_scroll.snap(0); start(false); }
}
void Search::sync() {
    std::lock_guard<std::mutex> lock(m_data->lock);
    m_revision = m_data->revision;
    if (m_items != m_data->items) {
        m_items = m_data->items;
        if (!m_anchor.empty()) for (size_t i = 0; i < m_items->size(); ++i)
            if ((*m_items)[i].id == m_anchor) { m_index = (int)i; break; }
        m_index = std::clamp(m_index, 0, std::max(0, grid_count(m_items->size()) - 1));
    }
    m_anchor = m_index < (int)m_items->size() ? (*m_items)[m_index].id : std::string();
}
void Search::start(bool more, bool retain) {
    const auto data = m_data;
    const auto query = m_query;
    const auto context = m_context;
    const auto account = m_account;
    auto *client = &m_client;
    const auto seerr = context.discover ? seerr_service::client() : nullptr;
    const auto seerr_generation = seerr_service::generation();
    int offset, limit = 60;
    unsigned generation;
    {
        std::lock_guard<std::mutex> lock(data->lock);
        if (!data->alive || (more && (data->loading || !data->more || data->failed))) return;
        if (!more) {
            ++data->generation; data->next = 0; data->more = false;
            if (retain && !context.discover) limit = std::max(limit, std::min(120, grid_count(data->items->size())));
            if (!retain) data->items = std::make_shared<const std::vector<jf::Item>>();
        }
        generation = data->generation; offset = data->next;
        data->failed = false; data->loading = !query.empty(); ++data->revision;
        if (query.empty()) return;
    }
    const auto fail = [data, generation] {
        std::lock_guard<std::mutex> lock(data->lock);
        if (data->alive && generation == data->generation) {
            data->failed = true; data->loading = false; ++data->revision;
        }
    };
    if (!emby5::spawn_io([data, client, query, context, account, seerr, seerr_generation, generation, offset, limit, more] {
        std::vector<jf::Item> found;
        bool ok = false, has_more = false;
        int next = offset;
        if (context.discover) {
            if (seerr) {
                int pages = 0;
                const int page = offset + 1;
                for (const auto &title : seerr->search(query, page, &pages))
                    found.push_back(seerr_service::to_item(title));
                ok = seerr->last_status() >= 200 && seerr->last_status() < 300;
                next = page; has_more = page < pages;
            }
        } else if (!context.types.empty()) {
            const auto page = context.types == "MusicArtist"
                ? client->album_artists(context.parent, "SortName", false, offset, limit, query)
                : client->library(context.parent, context.types, "SortName", false, offset, limit,
                                  context.filter + "&SearchTerm=" + jf::Client::escape(query));
            ok = page.ok;
            for (const auto &item : page.items)
                if (accepts_type(context.types, item.type)) found.push_back(item);
            next += grid_count(page.items.size());
            has_more = next < page.total && !page.items.empty();
        }
        if (account != browse_state::scope(*client) ||
            (context.discover && seerr_generation != seerr_service::generation())) ok = false;
        std::lock_guard<std::mutex> lock(data->lock);
        if (!data->alive || generation != data->generation) return;
        if (ok) {
            auto values = std::make_shared<std::vector<jf::Item>>();
            if (more) *values = *data->items;
            std::set<std::string> ids;
            for (const auto &item : *values) ids.insert(item.id);
            const auto previous = values->size();
            for (auto &item : found) if (ids.insert(item.id).second) values->push_back(std::move(item));
            data->items = std::move(values); data->next = next;
            data->more = has_more && (!more || data->items->size() > previous);
        }
        data->failed = !ok; data->loading = false; ++data->revision;
    }, fail)) fail();
}
Action Search::input(uint32_t p) {
    Action action;
    if (ime::active()) return action;
    consume_query(); sync();
    if (m_menu.active()) { m_menu.input(p, &action); return action; }
    if (p & NUVIO_BTN_CIRCLE) { action.kind = Action::Back; return action; }
    if (p & NUVIO_BTN_TRIANGLE) { edit_query(); return action; }
    if (m_items->empty()) {
        if (p & NUVIO_BTN_CROSS) { if (m_query.empty()) edit_query(); else start(false); }
        return action;
    }
    const bool square = m_context.types == "MusicArtist" || m_context.types == "MusicAlbum" || m_context.types == "Playlist";
    const BrowseGridGeometry layout(square ? 1.f : 2.f/3.f);
    bool header = false;
    const auto moved = move_browse_grid(p, grid_count(m_items->size()), layout.cols, m_index, header);
    if (header) m_index = std::min(m_index, layout.cols - 1); // no source-selection row in search
    m_bump |= moved.bump;
    m_anchor = (*m_items)[m_index].id;
    if ((p & NUVIO_BTN_OPTIONS) && !m_context.discover) m_menu.open((*m_items)[m_index], (*m_items)[m_index].position_ticks > 0);
    if (p & NUVIO_BTN_CROSS) { action.kind = Action::Open; action.item = (*m_items)[m_index]; }
    if (m_index > grid_count(m_items->size()) - 24) start(true);
    return action;
}
void Search::draw(double, float dt) {
    consume_query(); sync(); m_animating = false; m_has_card = false;
    gfx::fill({0,0,gfx::W,gfx::H}, kBg);
    gfx::text(kPad, 140, "Search " + m_context.title, {gfx::Bold, 50, 1400}, kText);
    gfx::text(kPad, 206, m_query.empty() ? "Triangle: enter a search" : m_query, {gfx::Medium, 28, 1500}, kText2);
    bool loading, failed;
    { std::lock_guard<std::mutex> lock(m_data->lock); loading = m_data->loading; failed = m_data->failed; }
    const bool square = m_context.types == "MusicArtist" || m_context.types == "MusicAlbum" || m_context.types == "Playlist";
    const BrowseGridGeometry layout(square ? 1.f : 2.f/3.f);
    // Unlike the tab grid, the search title remains visible: first card stays at 290.
    m_scroll.to((m_index / layout.cols) * layout.pitch);
    m_animating |= m_scroll.step(dt, 11.f);
    const float top = 290.f - m_scroll.value, bottom = gfx::H - 88.f;
    const auto range = layout.visible(grid_count(m_items->size()), top, 240, bottom);
    gfx::push_scissor({0,240,gfx::W,bottom-240});
    for (int pass = 0; pass != 2; ++pass) for (int i = range.first; i < range.second; ++i) {
        const bool focus = m_focused && !m_menu.active() && !ime::active() && i == m_index;
        if ((pass == 0) == focus) continue;
        const auto &item = (*m_items)[i];
        const float lift = m_lifts.step(item.id, focus, dt, &m_animating);
        const auto rect = layout.tile(i, top);
        draw_poster(m_client, item, rect, lift, 1.f);
        if (focus) {
            m_card = {browse_lifted_rect(rect, lift), poster_url(m_client, item, 480), item.primary_blurhash, 14};
            m_has_card = true;
        }
    }
    gfx::pop_scissor();
    if (m_items->empty()) gfx::text(kPad, 350,
        failed ? "Could not search. X: retry, Triangle: edit." : loading ? "Searching..." : m_query.empty() ? "Enter a search to see results." : "No results in this section.",
        {gfx::Medium,28,1500}, kText2);
    else if (failed) gfx::text(kPad, gfx::H-70, "Refresh failed. Triangle: edit or retry this search.", {gfx::Medium,20,1500}, kText2);
    else if (loading) gfx::text(kPad, gfx::H-70, "Searching...", {gfx::Medium,20}, kText2);
    draw_pad_hints(kPad, gfx::H-32,
        {{PadButton::Triangle,"Edit search"},{PadButton::Cross,"Open"},{PadButton::Circle,"Back"}}, 0, 24);
    m_menu.draw(dt, &m_animating);
    m_animating |= loading || art::animating() || ime::active();
}
} // namespace ui
