/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/screen.h"
#include "ui/choice_sheet.h"
#include "ui/description_reader.h"
#include "app/browse_state.h"
#include "app/iptv_vod.h"
#include <memory>
#include <mutex>
namespace ui {
class VodDetail : public Screen {
private:
    struct Data{std::mutex lock;bool alive=true,ready=false;iptv_vod::Detail detail;};
    std::shared_ptr<Data> m_data=std::make_shared<Data>();
    jf::Client &m_client;
    iptv_vod::Item m_item;
    std::string m_scope,m_provider,m_initial_episode;
    std::vector<int> m_seasons,m_indices;
    int m_season=0,m_episode=0,m_zone=0,m_button=0;
    bool m_loaded=false,m_animating=false;
    ChoiceSheet m_choices;
    DescriptionReader m_reader;
    SynopsisView m_synopsis;
    iptv_vod::Detail m_view;
    void sync();
    void select_season();
    browse_state::Progress selected()const;
    Action play(bool from_start);
public:
    VodDetail(jf::Client &client,iptv_vod::Item item,std::string episode={});
    ~VodDetail()override;
    void activate()override;
    Action input(uint32_t p)override;
    void draw(double now,float dt)override;
    bool animating()const override{return m_animating;}
    bool needs_redraw()const override;
    float nav_alpha()const override{return 0;}
    bool modal()const override{return m_reader.active()||m_choices.active();}
};
}
