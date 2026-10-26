/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/screen.h"
#include "ui/choice_sheet.h"
#include "app/browse_state.h"
#include "app/iptv_categories.h"
#include "app/iptv_live.h"
#include "app/iptv_navigation.h"
#include <map>
#include <memory>

namespace ui {
class IPTV : public Screen {
public:
    explicit IPTV(jf::Client &client);
    ~IPTV() override { m_life.reset(); }
    void activate() override;
    static bool launch(jf::Client &client, const iptv_xtream::Channel &channel);
    Action search(bool from_navigation = false) override;
    Action input(uint32_t pressed) override;
    void draw(double now, float dt) override;
    void update_hold(uint32_t held, double now);
    void focus_channel(const std::string &group_id, const std::string &channel_id);
    void enter_from_top() override;
    bool accelerating(double now) const { return !m_header && m_hold_dir && m_hold_since > 0 && now - m_hold_since >= 1.; }
    bool animating() const override { return m_animating; }
    bool needs_redraw() const override;
    float nav_alpha() const override { return m_nav.value; }
    /* The guide takes L1/R1 (its groups) from the tab bar. */
    bool modal() const override { return m_menu.active() || m_guide; }
    bool guide_open() const { return m_guide; }
private:
    struct Position { int index = 0; std::string key; };
    struct ReturnPosition { std::string group; Position position; bool header = false; float scroll = 0; bool navigation = false; };
    struct Result { size_t group = 0, channel = 0; };
    enum Menu { Options, Groups, Pinned };
    jf::Client &m_client;
    iptv_xtream::Credentials m_creds;
    iptv_categories::Store m_store;
    std::string m_scope;
    iptv_navigation::Playlist m_groups = std::make_shared<const iptv_navigation::Groups>();
    uint64_t m_seen = ~uint64_t(0), m_content_seen = ~uint64_t(0);
    int m_row = 0, m_index = 0;
    bool m_header = false, m_animating = true;
    std::map<std::string, Position> m_positions;
    std::string m_query;
    std::vector<Result> m_results;
    ReturnPosition m_return;
    Anim m_scroll, m_nav;
    Lifts m_lifts;
    std::map<std::string, Anim> m_row_scroll;
    double m_guide_focus_since = 0;
    std::string m_guide_focus, m_guide_requested;
    ChoiceSheet m_menu;
    Menu m_menu_mode = Options;
    std::string m_pin_group;
    std::vector<std::string> m_destinations;
    std::shared_ptr<int> m_life = std::make_shared<int>(0);
    uint32_t m_hold_dir = 0;
    double m_hold_since = 0, m_hold_last = 0;
    /* The TV guide (Square): the group's channels down, the time across. */
    bool m_guide = false;
    int64_t m_guide_time = 0;   /* the focused moment: the focused programme is the one on then */
    int64_t m_guide_left = 0;   /* the time at the grid's left edge (on a half hour) */
    Anim m_guide_rows, m_guide_x;   /* the first row shown; the left edge, in seconds from m_guide_base */
    int64_t m_guide_base = 0;
    unsigned m_guide_seen = 0;  /* epg::generation() last drawn */
    int64_t m_guide_minute = 0; /* the "now" line moves once a minute */
    void open_guide();
    void close_guide();
    Action guide_input(uint32_t p);
    void guide_move(int rows);
    void guide_follow();
    void draw_guide(double now, float dt);
    void synchronise();
    void build_groups(const iptv_live::Snapshot &snapshot);
    void rebuild_search(const std::string &selected = {});
    void clear_search();
    void remember();
    void select_group(int row, bool header);
    void open_groups(Menu mode, const std::string &query = {});
    const iptv_xtream::Channel *channel(int index) const;
    int count() const;
    void move(uint32_t p, Action *action);
    static std::string key(const iptv_xtream::Channel &channel);
};
} // namespace ui
