/* Real PlayerUi handling and native command contract. Decoder/services mocked. */
#include "ui/player_ui.h"
#include "app/iptv_navigation.h"
#include <cassert>
#include <iostream>
namespace syncplay {extern bool test_active;}
static nuvio_input_state press(uint32_t b,bool repeat=false){nuvio_input_state s{};s.held=s.pressed=b;s.repeats=repeat?b:0;return s;}
static NuvioRequest req(){iptv_navigation::Selection s;assert(iptv_navigation::current(&s));NuvioRequest r;
    r.title=s.channel.name;r.item_type="movie";r.live_channel_id=s.channel.id;r.live_group_name=s.group_name;r.live_navigation_token=s.token;return r;}
static bool zap(const std::vector<OsdCommand> &o){for(const auto &c:o)if(c.cmd==OsdCmd::LiveChannel||c.cmd==OsdCmd::LiveGroup)return true;return false;}
int main(){
    iptv_xtream::Channel a,b,c,d;a.id="xtream:1";b.id="xtream:2";c.id="xtream:3";d.id="xtream:4";a.name=b.name=c.name=d.name="Same channel title";
    auto groups=std::make_shared<const iptv_navigation::Groups>(iptv_navigation::Groups{{"f1","F1",{a}},{"cro","Cro",{b,c}},{"srb","Srb",{d}}});
    assert(iptv_navigation::begin(groups,1,0));auto r=req();ui::PlayerUi ui;NuvioStatus st;st.now=5;st.started=true;st.duration=100;st.position=20;
    std::vector<OsdCommand> out;ui.begin(&r,0);ui.tick(st,out,false);assert(out.empty()); // no auto-open controls for live
    const auto check=[&](uint32_t key,OsdCmd cmd,int dir){out.clear();ui.input(press(key),st,out);assert(out.size()==1&&out[0].cmd==cmd&&out[0].index==dir);};
    check(NUVIO_BTN_UP,OsdCmd::LiveGroup,-1);check(NUVIO_BTN_DOWN,OsdCmd::LiveGroup,1);
    check(NUVIO_BTN_LEFT,OsdCmd::LiveChannel,-1);check(NUVIO_BTN_RIGHT,OsdCmd::LiveChannel,1);
    out.clear();ui.input(press(NUVIO_BTN_RIGHT,true),st,out);assert(out.empty());
    out.clear();ui.input(press(NUVIO_BTN_LEFT|NUVIO_BTN_RIGHT),st,out);assert(out.empty());
    // Native dispatch requests a transition and exits the old player first.
    assert(iptv_navigation::request_move(r.live_navigation_token,r.live_channel_id,0,1));
    ui.end();assert(iptv_navigation::advance());r=req();ui.begin(&r,6);
    out.clear();ui.input(press(NUVIO_BTN_RIGHT),st,out);assert(out.empty()); // reopen reports held key as pressed
    ui.input({},st,out);check(NUVIO_BTN_RIGHT,OsdCmd::LiveChannel,1);
    // Options gives normal controls; arrows do not change channels there.
    out.clear();ui.input(press(NUVIO_BTN_OPTIONS),st,out);ui.input(press(NUVIO_BTN_RIGHT),st,out);assert(!zap(out));
    ui.input(press(NUVIO_BTN_UP),st,out);assert(!zap(out));
    ui.input(press(NUVIO_BTN_CIRCLE),st,out);check(NUVIO_BTN_LEFT,OsdCmd::LiveChannel,-1);
    ui.input(press(NUVIO_BTN_SQUARE),st,out);out.clear();ui.input(press(NUVIO_BTN_DOWN),st,out);assert(!zap(out));
    ui.input(press(NUVIO_BTN_CIRCLE),st,out); // close tracks
    ui.begin(&r,7);out.clear();st.started=false;ui.input(press(NUVIO_BTN_RIGHT),st,out);assert(out.empty());
    ui.input(press(NUVIO_BTN_CIRCLE),st,out);assert(out.size()==1&&out[0].cmd==OsdCmd::Stop);
    ui.begin(&r,8);st.started=true;st.switching=true;out.clear();ui.input(press(NUVIO_BTN_RIGHT),st,out);assert(out.empty());st.switching=false;
    ui.input(press(NUVIO_BTN_L3),st,out);ui.input(press(NUVIO_BTN_UP),st,out);assert(!zap(out));
    ui.begin(&r,9);st.error="Failed";out.clear();ui.input(press(NUVIO_BTN_UP),st,out);assert(out.empty());
    ui.input(press(NUVIO_BTN_CIRCLE),st,out);assert(out.size()==1&&out[0].cmd==OsdCmd::Stop);st.error.clear();
    // Stale channel-title matching is never enough. VOD and movies keep seeking.
    r.live_navigation_token.clear();r.live_channel_id.clear();ui.begin(&r,10);out.clear();
    ui.input(press(NUVIO_BTN_RIGHT),st,out);assert(!zap(out));
    // SyncPlay cannot be silently left by Live TV shortcuts.
    r=req();ui.begin(&r,11);syncplay::test_active=true;out.clear();ui.input(press(NUVIO_BTN_UP),st,out);assert(!zap(out));syncplay::test_active=false;
    iptv_navigation::end();ui.begin(&r,12);out.clear();ui.input(press(NUVIO_BTN_DOWN),st,out);assert(!zap(out));
    std::cout<<"PASS: real PlayerUi Up/Down groups and Left/Right channels; press/repeat/reopen release; menus/tracks/stats/loading/error/SyncPlay isolation; unchanged movie/VOD controls\n";
}
