/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The detail page (concept: .detail): backdrop, logo, metadata and badges,
 * tagline, overview, Play / From start / Favourite, credits; then, for a
 * series, the season picker and the season's episodes; cast and crew; more
 * like this. A collection (BoxSet) lists its titles instead. The page slides
 * up as focus moves down and the backdrop dims. Circle goes back to the
 * buttons, then off the page. Options on an episode marks it watched.
 *
 * A series the library has only some seasons of, with Seerr signed in: a
 * button to request the others (Seerr finds the series by its TMDB or TVDB
 * id; the seasons there or asked for already are greyed out in the sheet).
 */
#pragma once

#include "seerr/seerr_client.h"
#include "ui/screen.h"
#include "ui/description_reader.h"
#include <set>
#include "ui/item_menu.h"
#include "ui/seerr_request.h"
#include "ui/overview_scroll.h"
#include "ui/trailer_qr.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ui {

class Detail : public Screen {
public:
    /* What the page shows (also what prefetch caches). */
    struct Content {
        jf::Item item;
        jf::Detail detail;
        bool have_detail = false;
        // A failed optional endpoint must not hide successfully fetched episodes.
        bool seasons_ok = false, episodes_ok = false, upcoming_ok = false;
        bool similar_ok = false, extras_ok = false, trailers_ok = false;
        bool resume_ok = false, next_ok = false;
        std::vector<jf::Item> available_episodes, upcoming_episodes, resume_items, next_items;
        jf::Item target;              /* what Play plays (a series: its next episode) */
        bool have_target = false;
        std::vector<jf::Item> seasons;
        std::vector<jf::Item> all_episodes;   /* every season's, fetched with the page */
        std::vector<jf::Item> similar;
        std::vector<jf::Item> extras; /* behind the scenes, deleted scenes, featurettes */
        jf::Item trailer;             /* its first local trailer, when it has one */
        bool have_trailer = false;
    };
    Detail(jf::Client &client, const jf::Item &item);
    const jf::Item &item() const { return m_view.item; }

    /* Loads an item's page data in the background so opening it is instant
     * (screens call it when focus rests on a card). Cached across pages. */
    static void prefetch(jf::Client &client, const jf::Item &item);

    void activate() override;
    void reconcile_user_data(const jf::Item &fresh) override;
    void refresh_user_data() override { activate(); }
    static void invalidate_user_data();
    Action input(uint32_t pressed) override;
    void draw(double now, float dt) override;
    bool animating() const override { return m_animating; }
    float nav_alpha() const override { return 0.f; }
    float enter() const override { return m_enter.value; }
    bool modal() const override { return m_sheet.active() || m_menu.active() || m_trailer_qr.active() || m_read_more.active(); }

private:
    friend struct DetailRegressionAccess; // host regression access, no runtime hook
    enum Zone { Buttons, Seasons, Episodes, Extras, Cast, Similar, ZoneCount };
    enum Button { PlayButton, RestartButton, TrailerButton, TrailerQrButton, WatchedButton, FavouriteButton };

    struct Data {
        std::mutex lock;
        Content c;
        unsigned generation = 0;
        unsigned revision = 1; // published content/Seerr revision, not a per-frame copy
        bool loading = false, refresh_again = false;
        /* The series in Seerr: what of it can be requested. */
        seerr::Detail seerr;
        bool have_seerr = false, seerr_pending = false;
    };
    static void look_up_in_seerr(const std::shared_ptr<Data> &d, const jf::Item &series);

    /* The focused season's episodes, from all_episodes (no request: instant). */
    void select_episodes(bool preserve_selection = true);
    void season_moved();
    static Content fetch(jf::Client &client, const jf::Item &base);
    static void merge_content(Content &current, Content fresh);
    static void rebuild_content(Content &content);
    std::vector<Zone> zones() const;
    std::vector<Button> buttons() const;
    void sync_button();
    bool can_ask_seerr() const;   /* Seerr has seasons of this series to ask for (a menu entry) */
    float zone_top(Zone z) const;     /* page y of a section */

    void draw_top(float y0, float dt);
    void draw_sections(float dt);

    jf::Client &m_client;
    std::shared_ptr<Data> m_data = std::make_shared<Data>();
    Content m_view;                    /* last published snapshot, drawn without the lock */
    unsigned m_revision = 0;

    Zone m_zone = Buttons;
    int m_button = 0;
    /* Which button has the focus (its index can change: "Be om flere sesonger"
     * comes and goes when Seerr answers). sync_button() maps it back each time. */
    int m_button_id = -1;
    int m_season = 0, m_episode = 0, m_extra = 0, m_cast = 0, m_similar = 0;
    int m_episode_first = 0;          /* first whole episode card in the viewport */
    double m_now = 0;
    bool m_season_picked = false;      /* the viewer moved the picker (else follow the target) */
    std::vector<jf::Item> m_eps;       /* the focused season's episodes */
    std::string m_followed;            /* the target already brought into view */

    Anim m_page;                       /* page scroll */
    Anim m_enter;                      /* fade in over the screen below */
    Anim m_content;                    /* text and buttons, once the details are in */
    double m_opened = -1;
    float m_overview_scale = 0;
    std::string m_overview_text;
    std::vector<std::string> m_overview_lines;
    overview::Scroll m_overview_scroll;
    Anim m_scroll[ZoneCount];          /* per-row horizontal scroll */
    Lifts m_lifts;
    Drop m_btn_drop;                  /* action buttons; seasons use one matching outline */
    bool m_animating = false;

    seerr::Detail m_seerr;              /* this frame's copy */
    bool m_have_seerr = false;
    bool m_seerr_asked_late = false;    /* looked up again once Seerr came up after the page */
    TrailerQr m_trailer_qr;
    DescriptionReader m_read_more;
    std::set<std::string> m_revealed_episodes; // this page only; never changes UserData
    bool spoiler_hidden(const jf::Item &item) const;
    RequestSheet m_sheet;
    ItemMenu m_menu;                    /* Options: the focused season's or episode's menu (PS5's way) */
    int m_menu_zone = -1;               /* the zone it was opened in */
    std::string m_note;                 /* how a request went, shown for a few seconds */
    uint32_t m_note_dot = 0xff30d158u;      /* its dot: green done, amber failed, grey neither */
    double m_note_at = -100;
    Anim m_note_a;
};

} // namespace ui
