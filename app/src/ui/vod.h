/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/screen.h"
#include "ui/choice_sheet.h"
#include "app/browse_state.h"
#include "app/iptv_vod.h"
#include <map>
#include <memory>

namespace ui {
class Vod : public Screen {
public:
    explicit Vod(jf::Client *client = nullptr);
    ~Vod() override { m_life.reset(); }
    void activate() override;
    Action search(bool from_navigation = false) override;
    Action input(uint32_t pressed) override;
    void draw(double now, float dt) override;
    bool animating() const override { return m_animating; }
    bool needs_redraw() const override;
    float nav_alpha() const override { return m_nav.value; }
    bool modal() const override { return m_menu.active(); }
    void enter_from_top() override;
private:
    struct Position { int index = 0; std::string source, id; };
    struct ReturnPosition { std::string group; Position position; bool header = false; int control = 0; float scroll = 0; bool navigation = false; };
    enum Menu { Options, ContinueWatching, Categories };
    jf::Client *m_client;
    std::shared_ptr<int> m_life = std::make_shared<int>(0);
    std::string m_identity;
    uint64_t m_seen = ~uint64_t(0);
    bool m_shows = false, m_header = false, m_animating = true;
    int m_row = 0, m_index = 0, m_control = 0;
    std::vector<iptv_vod::Group> m_groups, m_matches;
    std::map<std::string,Position> m_positions;
    std::map<bool,std::string> m_last_group;
    std::string m_query;
    ReturnPosition m_return;
    Anim m_scroll, m_nav;
    Lifts m_lifts;
    struct RowPage { int first=-1,limit=0; std::vector<iptv_vod::Item> items; };
    std::map<std::string,RowPage> m_row_pages;
    std::map<std::string,Anim> m_row_scroll;
    ChoiceSheet m_menu;
    Menu m_menu_mode = Options;
    std::vector<std::string> m_destinations;
    std::vector<browse_state::Progress> m_progress;
    std::vector<iptv_vod::Item> m_page;
    int m_page_first = -1, m_page_limit = 0;
    void synchronise();
    void regroup(const iptv_vod::Snapshot &snapshot);
    void remember();
    void clear_search();
    void switch_type(bool shows);
    void select_group(int row, bool header);
    void open_groups(const std::string &query = {});
    int count() const;
    std::string position_key(const std::string &group) const;
    std::vector<iptv_vod::Item> read_page(int first, int limit) const;
    const iptv_vod::Item *selected();
    void cache_page(int first, int limit);
    void invalidate_page();
};
} // namespace ui
