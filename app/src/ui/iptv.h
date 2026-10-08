#pragma once
#include "ui/screen.h"
#include "app/iptv_xtream.h"
#include "app/iptv_categories.h"
#include <memory>
#include <mutex>
namespace ui {
class IPTV : public Screen {
public:
    IPTV();
    ~IPTV() override;
    void activate() override;
    Action input(uint32_t p) override;
    void draw(double now, float dt) override;
    bool animating() const override { return true; }
    float nav_alpha() const override { return 1.f; }
    bool modal() const override { return false; }
private:
    struct Data { std::mutex lock; iptv_xtream::Catalog catalog; std::string error; bool loading=false, ready=false; };
    std::shared_ptr<Data> m_data = std::make_shared<Data>();
    iptv_xtream::Credentials m_creds;
    iptv_categories::Store m_store;
    bool m_connected=false;
    bool m_category_focus=false;
    int m_filter=0, m_index=0, m_category=0, m_scroll=0;
    void refresh();
    void reload_store();
    std::vector<iptv_xtream::Channel> visible(const iptv_xtream::Catalog &c) const;
};
}
