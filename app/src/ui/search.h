/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/screen.h"
#include "ui/item_menu.h"
#include <memory>
#include <mutex>

namespace ui {
// Results for one immutable context supplied by the screen that opened search.
// There is deliberately no source selector and no provider catalogue here.
class Search : public Screen {
public:
    Search(jf::Client &client, SearchContext context, std::string query, bool from_nav = false);
    ~Search() override;
    void activate() override;
    void refresh_user_data() override;
    void reconcile_user_data(const jf::Item &fresh) override;
    Action input(uint32_t pressed) override;
    void draw(double now, float dt) override;
    bool animating() const override { return m_animating; }
    bool needs_redraw() const override;
    float nav_alpha() const override { return 0.f; }
    bool modal() const override { return true; }
    bool return_to_navigation() const { return m_from_nav; }
    bool focused_card(Card *card) const override {
        if (!m_has_card || m_menu.active()) return false;
        *card = m_card; return true;
    }
private:
    struct Data {
        std::mutex lock;
        bool alive = true, loading = false, more = false, failed = false;
        bool query_ready = false;
        unsigned generation = 0;
        uint64_t revision = 1;
        int next = 0;
        std::string query;
        std::shared_ptr<const std::vector<jf::Item>> items = std::make_shared<const std::vector<jf::Item>>();
    };
    std::shared_ptr<Data> m_data = std::make_shared<Data>();
    jf::Client &m_client;
    const SearchContext m_context;
    const std::string m_account;
    std::string m_query, m_anchor;
    bool m_started = false, m_from_nav = false, m_animating = false, m_has_card = false;
    int m_index = 0;
    uint64_t m_revision = 0;
    std::shared_ptr<const std::vector<jf::Item>> m_items;
    Anim m_scroll;
    Lifts m_lifts;
    ItemMenu m_menu;
    Card m_card;
    void edit_query();
    void consume_query();
    void start(bool more, bool retain = false);
    void sync();
};
} // namespace ui
