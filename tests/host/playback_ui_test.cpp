#include "ui/player_ui.h"
#include "app/remote.h"
#include <cassert>
#include <iostream>

static nuvio_input_state press(uint32_t button, bool repeat=false) {
    nuvio_input_state in{}; in.pressed=button; in.held=button;
    in.repeats=repeat?button:0; return in;
}
int main() {
    NuvioRequest req;
    req.item_type="episode";
    req.has_next=true; req.prefs.autoplay_next=true;
    req.prefs.still_watching_mode=1;
    NuvioStatus st{}; st.started=true; st.now=10; st.duration=100; st.position=100;
    std::vector<OsdCommand> out;
    ui::PlayerUi ui;
    for (int completed=0; completed<3; ++completed) {
        req.autoplay_count=completed; ui.begin(&req, 0); out.clear();
        ui.playback_ended(st,out);
        if (completed<2) {
            assert(out.size()==1 && out[0].cmd==OsdCmd::PlayNext && out[0].automatic);
            assert(ui.completed_episodes(true)==completed+1);
        } else {
            assert(out.empty() && ui.post_play_active());
        }
    }
    ui.input(press(NUVIO_BTN_DOWN),st,out); assert(out.empty() && ui.post_play_active());
    ui.input(press(NUVIO_BTN_CROSS,true),st,out); assert(out.empty() && ui.post_play_active());
    remote::Command queued; queued.kind=remote::Command::Next; remote::send(queued);
    queued.kind=remote::Command::Unpause; remote::send(queued);
    ui.tick(st,out,true); assert(out.empty());
    assert(!remote::take(&queued)); // stale skips cannot execute after confirmation
    ui.input(press(NUVIO_BTN_CROSS),st,out);
    assert(out.size()==1 && out[0].cmd==OsdCmd::PlayNext && out[0].automatic);
    assert(!ui.post_play_active() && ui.completed_episodes(true)==0);
    req.autoplay_count=2; ui.begin(&req,0); out.clear();
    ui.input(press(NUVIO_BTN_SQUARE),st,out); out.clear(); // prior input must not disable prompt
    ui.playback_ended(st,out); assert(ui.post_play_active() && out.empty());
    ui.input(press(NUVIO_BTN_CIRCLE),st,out);
    assert(out.size()==1 && out[0].cmd==OsdCmd::Stop);
    ui.begin(&req,0); out.clear(); ui.playback_ended(st,out);
    queued.kind=remote::Command::Stop; remote::send(queued);
    ui.tick(st,out,true); assert(out.size()==1 && out[0].cmd==OsdCmd::Stop);
    req.prefs.still_watching_mode=0; ui.begin(&req,0); out.clear();
    ui.playback_ended(st,out); assert(out.size()==1 && !ui.post_play_active());
    req.prefs.still_watching_mode=1; ui.begin(&req,0); req.autoplay_count=0;
    ui.reopen(&req,3); out.clear(); ui.playback_ended(st,out);
    assert(ui.post_play_active() && out.empty()); // reconnect retains count
    req.prefs.still_watching_mode=2; req.prefs.unattended_seconds=7199;
    req.has_next=false; ui.begin(&req,0); out.clear();
    st.position=9999; st.duration=10000; st.now=0;
    ui.tick(st,out,false); assert(!ui.post_play_active());
    st.now=1; ui.tick(st,out,false);
    assert(ui.post_play_active() && out.size()==1 && out[0].cmd==OsdCmd::TogglePause);
    out.clear(); st.paused=true; st.now=4000; ui.tick(st,out,false);
    assert(out.empty() && ui.watched_seconds()==7200);
    ui.input(press(NUVIO_BTN_CROSS),st,out);
    assert(out.size()==1 && out[0].cmd==OsdCmd::TogglePause && ui.watched_seconds()==0);
    req.prefs.unattended_seconds=0; ui.begin(&req,0); out.clear();
    st.paused=false; st.now=0; ui.tick(st,out,false);
    st.now=1; ui.tick(st,out,false); assert(ui.watched_seconds()==1 && !ui.post_play_active());
    st.buffering=true; st.now=2; ui.tick(st,out,false);
    st.now=7202; ui.tick(st,out,false); assert(ui.watched_seconds()==1 && !ui.post_play_active());
    // Loading/opening must have an immediate, explicit cancellation action.
    req.prefs.still_watching_mode=0;ui.begin(&req,0);out.clear();st=NuvioStatus{};
    ui.input(press(NUVIO_BTN_CIRCLE),st,out);
    assert(out.size()==1 && out.front().cmd==OsdCmd::Stop);
    std::cout << "PASS: actual PlayerUi gates episode 4, ignores repeats/other keys/queued skips, X continues, Circle stops, reconnect and two-hour pause\n";
}
