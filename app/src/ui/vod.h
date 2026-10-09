#pragma once
#include "ui/screen.h"
#include "app/iptv_vod.h"
#include <memory>
#include <mutex>
namespace ui {
class Vod : public Screen {
public:
    Vod();
    void activate() override;
    Action input(uint32_t pressed) override;
    void draw(double now,float dt) override;
    bool animating() const override {return true;}
    float nav_alpha() const override {return 1.f;}
private:
    struct Group {std::string title;size_t count=0;};
    struct EpisodeState {std::mutex mutex;std::vector<iptv_vod::Episode> list;bool ready=false;};
    uint64_t seen=0;
    bool shows=false,search=false,detail=false;
    int row=0,episode_index=0;
    std::vector<int> columns;
    std::vector<Group> groups;
    std::string query;
    std::string series_name;
    std::shared_ptr<EpisodeState> episode_state;
    void regroup(const iptv_vod::Snapshot &snapshot);
};
}
