/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/item_menu.h"
#include "app/i18n.h"
#include "ui/trailer_qr.h"

#include "nuvio_input.h"

#include <algorithm>
#include <cstdio>

namespace ui {

void apply_change(jf::Item &it, const UserDataChange &c)
{
    if (it.id != c.id)
        return;
    if (c.favorite_set)
        it.favorite = c.favorite;
    if (c.played_set) {
        it.played = c.played;
        it.position_ticks = 0;
        it.played_percent = 0;
    }
    if (c.resume_cleared) {
        it.position_ticks = 0;
        it.played_percent = 0;
    }
}

void ItemMenu::open(const jf::Item &item, bool in_resume_row)
{
    m_item = item;
    m_confirm=false;m_confirm_only=false;
    m_options.clear();
    m_confirm=false;m_confirm_only=false;m_collection=Action{};
    if(!item.id.empty())m_options.push_back(Info);
    const bool playable=!item.upcoming&&!item.external()&&
        (item.type=="Movie"||item.type=="Episode"||item.type=="Video"||item.type=="MusicVideo"||item.type=="Audio");
    if(playable){m_options.push_back(PlayNow);if(item.position_ticks>0)m_options.push_back(StartOver);}
    if(item.type=="Episode"){
        if(!item.series_id.empty())m_options.push_back(Series);
        if(!item.season_id.empty())m_options.push_back(Season);
    }
    if(!first_trailer_url(item.remote_trailers).empty())m_options.push_back(Trailer);
    if (item.type == "Movie" || item.type == "Series" || item.type == "BoxSet" || item.type == "Episode" ||
        item.type == "Video")
        m_options.push_back(List);
    if (!item.id.empty() && item.type != "BoxSet")
        m_options.push_back(Played);
    if (in_resume_row && item.position_ticks > 0)
        m_options.push_back(Resume);
    m_focus = 0;
    m_open = true;
    m_alpha.to(1.f);
}

void ItemMenu::open_actions(const jf::Item &item, bool played, bool ask_seerr)
{
    m_item = item;
    m_confirm=false;m_confirm_only=false;
    m_options.clear();
    if (played)
        m_options.push_back(Played);
    if (ask_seerr)
        m_options.push_back(AskSeerr);
    if (m_options.empty())
        return;
    m_focus = 0;
    m_asked = false;
    m_open = true;
    m_alpha.to(1.f);
}

bool ItemMenu::take_ask()
{
    const bool asked = m_asked;
    m_asked = false;
    return asked;
}

void ItemMenu::confirm_played(const jf::Item &item)
{
    open_actions(item,true,false);m_confirm=true;m_confirm_only=true;m_confirm_choice=0;
}

void ItemMenu::browser_actions(bool retry,const Action &collection)
{
    if(collection.kind==Action::ViewCollection){m_collection=collection;m_options.push_back(ViewAll);}
    if(retry)m_options.push_back(Retry);
}
void ItemMenu::input(uint32_t p, Action *action)
{
    if(m_confirm){
        if(p&(NUVIO_BTN_CIRCLE|NUVIO_BTN_OPTIONS)){m_confirm=false;if(m_confirm_only){m_open=false;m_alpha.to(0.f);}return;}
        if(p&(NUVIO_BTN_UP|NUVIO_BTN_LEFT))m_confirm_choice=0;
        if(p&(NUVIO_BTN_DOWN|NUVIO_BTN_RIGHT))m_confirm_choice=1;
        if(p&NUVIO_BTN_CROSS){
            m_confirm=false;
            if(!m_confirm_choice&&m_confirm_only){m_open=false;m_alpha.to(0.f);}
            if(m_confirm_choice){action->kind=Action::Changed;action->item=m_item;
                action->change.id=m_item.id;action->change.played_set=true;action->change.played=!m_item.played;
                m_open=false;m_alpha.to(0.f);}
        }
        return;
    }
    if (p & (NUVIO_BTN_CIRCLE | NUVIO_BTN_OPTIONS)) {
        m_open = false;m_alpha.to(0.f);return;
    }
    if (p & NUVIO_BTN_UP)m_focus=std::max(0,m_focus-1);
    else if (p & NUVIO_BTN_DOWN)m_focus=std::min((int)m_options.size()-1,m_focus+1);
    if (!(p & NUVIO_BTN_CROSS)||m_options.empty())return;
    const Option choice=m_options[(size_t)m_focus];
    if(choice==Played&&(m_item.type=="Series"||m_item.type=="Season")){
        m_confirm=true;m_confirm_choice=0;return;
    }
    m_open=false;m_alpha.to(0.f);action->item=m_item;
    switch(choice){
    case Info:action->kind=Action::Open;return;
    case AskSeerr:m_asked=true;return;
    case PlayNow:action->kind=Action::Play;return;
    case StartOver:action->kind=Action::PlayFromStart;return;
    case Series:case Season:
        action->item=jf::Item{};action->item.id=choice==Series?m_item.series_id:m_item.season_id;
        action->item.type=choice==Series?"Series":"Season";action->item.series_id=m_item.series_id;action->item.series_name=m_item.series_name;
        action->item.name=choice==Series?m_item.series_name:m_item.season_name;
        action->kind=Action::Open;return;
    case Trailer:action->kind=Action::Trailer;return;
    case Retry:action->kind=Action::RetryHome;return;
    case ViewAll:*action=m_collection;return;
    default:break;
    }
    auto &c=action->change;c.id=m_item.id;
    switch(choice){
    case List:c.favorite_set=true;c.favorite=!m_item.favorite;break;
    case Played:c.played_set=true;c.played=!m_item.played;break;
    case Resume:c.resume_cleared=true;break;
    default:return;
    }
    action->kind=Action::Changed;
}

void ItemMenu::draw(float dt, bool *animating)
{
    if (m_alpha.step(dt, 14.f) && animating)
        *animating = true;
    const float a = m_alpha.value;
    if (a <= 0.01f)
        return;
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0x99000000u, a));

    if(m_confirm){
        const gfx::Rect r{(gfx::W-820)/2,(gfx::H-360)/2,820,360};glass_panel(r,24,a);
        gfx::text(r.x+40,r.y+65,T(m_item.type=="Series"?"Change the entire series?":"Change the entire season?"),{gfx::Bold,30,740},alpha(kText,a));
        gfx::text(r.x+40,r.y+110,m_item.name,{gfx::Medium,24,740},alpha(kText2,a));
        for(int i=0;i<2;++i){gfx::Rect button{r.x+35,r.y+145+i*76.f,750,64};glass_panel(button,14,a,false,i==m_confirm_choice?1.f:0.f);
            gfx::text(button.x+24,button.y+42,i==0?T("Cancel"):m_item.played?T("Mark all as unplayed"):T("Mark all as played"),{gfx::SemiBold,26,700},alpha(kText,a));}
        return;
    }
    const bool ep = m_item.type == "Episode";
    const std::string title = m_item.id.empty()?T("Browse options"):ep ? m_item.series_name : m_item.name;
    std::string sub;
    if (ep) {
        char b[256];
        std::snprintf(b, sizeof b, "S%d:E%d \xC2\xB7 %s", m_item.parent_index, m_item.index, m_item.name.c_str());
        sub = b;
    }
    const float row_h = 68, w = 640;
    const float h = 60 + 52 + (sub.empty() ? 0 : 34) + 24 + std::min((size_t)7,m_options.size()) * (row_h + 6) + 40 + 52;
    const float rise = 24 * (1.f - a);
    const gfx::Rect r{(gfx::W - w) / 2, (gfx::H - h) / 2 + rise, w, h};
    glass_panel(r, 28, a);

    float y = r.y + 60 + 30;
    gfx::text(r.x + 48, y, title, {gfx::Bold, 34, w - 96}, alpha(kText, a));
    if (!sub.empty()) {
        y += 38;
        gfx::text(r.x + 48, y, sub, {gfx::Medium, 22, w - 96}, alpha(kText3, a));
    }
    y += 40;
    const int first=std::max(0,m_focus-6);
    m_drop.to({r.x + 30, y + (m_focus-first) * (row_h + 6), w - 60, row_h}, m_focus, r.x, r.y);
    m_drop.draw(dt, a, animating, 14);
    for (size_t i = (size_t)first; i < m_options.size() && i<(size_t)first+7; i++) {
        const bool focus = (int)i == m_focus;
        const gfx::Rect row{r.x + 30, y, w - 60, row_h};
        const char *label = "";
        switch (m_options[i]) {
        case PlayNow: label=m_item.position_ticks>0?T("Resume"):T("Play");break;
        case StartOver:label=T("Play from beginning");break;
        case Series:label=T("Go to series");break;
        case Season:label=T("Go to season");break;
        case Trailer:label=T("Trailer QR");break;
        case ViewAll:label=T("View all in this row");break;
        case Retry:label=T("Retry Home sections");break;
        case Info: label = T("Mer info"); break;
        case List: label = m_item.favorite ? T("Fjern fra Min liste") : T("Legg til i Min liste"); break;
        case Played:
            if (m_item.type == "Series")
                label = m_item.played ? T("Merk hele serien som usett") : T("Merk hele serien som sett");
            else if (m_item.type == "Season")
                label = m_item.played ? T("Merk sesongen som usett") : T("Merk sesongen som sett");
            else
                label = m_item.played ? T("Merk som usett") : T("Merk som sett");
            break;
        case Resume: label = T("Fjern fra Fortsett \xC3\xA5 se"); break;
        case AskSeerr: label = T("Be om flere sesonger"); break;
        }
        gfx::text(row.x + 26, row.y + 44, label, {focus ? gfx::Bold : gfx::SemiBold, 26}, alpha(focus ? kText : kText2, a));
        y += row_h + 6;
    }
    draw_pad_hints(r.x + 56, r.y + r.h - 52, {{PadButton::Cross, T("Velg")}, {PadButton::Circle, T("Lukk")}}, 0, 26,
                   a);
}

} // namespace ui
