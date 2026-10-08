#pragma once
#include "ui/screen.h"
#include "app/iptv_xtream.h"
#include "app/iptv_categories.h"
#include "app/iptv_live.h"
#include "jf/jf_client.h"
#include <memory>
#include <mutex>
namespace ui {
class IPTV : public Screen {
public:
    explicit IPTV(jf::Client &client);
    ~IPTV() override;
    void activate() override;
    Action input(uint32_t p) override;
    void draw(double now, float dt) override;
    void update_hold(uint32_t held, double now);
    bool accelerating(double now) const { return m_hold_dir && m_hold_since>0 && now-m_hold_since>=1.0; }
    bool animating() const override { return true; }
    float nav_alpha() const override { return 1.f; }
    bool modal() const override { return false; }
private:
    jf::Client &m_client;
    uint64_t m_seen=0;
    std::string m_guide_requested;
    iptv_xtream::Credentials m_creds;
    iptv_categories::Store m_store;
    bool m_connected=false;
    bool m_category_focus=false;
    int m_filter=0, m_index=0, m_category=0, m_scroll=0;
    bool m_search=false;
    int m_saved_index=0;
    std::string m_query;
    uint32_t m_hold_dir=0;
    double m_hold_since=0, m_hold_last=0;
    void refresh();
    std::vector<iptv_xtream::Channel> current() const;
    void reload_store();
    std::vector<iptv_xtream::Channel> visible(const iptv_xtream::Catalog &c) const;
    void move_selection(int delta, bool allow_category=true);
};
}
