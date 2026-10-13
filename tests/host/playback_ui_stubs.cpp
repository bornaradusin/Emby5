/* Host test doubles for the real PlayerUi. No decoder, server or GPU is used. */
#include "app/iptv_live.h"
#include "app/settings.h"
#include "app/syncplay.h"
#include "app/remote.h"
#include "app/i18n.h"
#include "emby5_playback.h"
#include "emby5_bitstream.h"
#include "nuvio_subs.h"
#include "evo_agc_runtime.h"
#include "evo_audio_out.h"
#include <deque>

const char *T(const char *s) { return s; }
namespace iptv_live {
std::string playing_channel() { return {}; }
Snapshot snapshot() { return {}; }
void set_playing_channel(const std::string &) { }
bool channel_by_id(const std::string &, iptv_xtream::Channel *) { return false; }
}
namespace settings {
All get() { return {}; }
void set_local(const Local &) { }
}
namespace emby5_subs {
bool available() { return false; }
void search(const std::string &) { }
State results(std::vector<jf::RemoteSubtitle> *, std::string *) { return Idle; }
void download(const jf::RemoteSubtitle &) { }
State download_state(int *) { return Idle; }
}
namespace remote {
static std::deque<Command> queue;
bool take(Command *out) { if (queue.empty()) return false; *out=queue.front(); queue.pop_front(); return true; }
void put_back(const Command &c) { queue.push_front(c); }
void send(const Command &c) { queue.push_back(c); }
}
namespace syncplay {
bool test_active=false;
bool active() { return test_active; }
void player_started(double, bool) { }
void request_pause(bool, double) { }
void request_seek(double) { }
void request_next() { }
void seeked(double) { }
}
extern "C" {
int nuvio_subs_count() { return 0; }
int nuvio_subs_selected() { return -1; }
void nuvio_subs_get_style(nuvio_sub_style *out) { *out = {}; }
void nuvio_subs_set_style(const nuvio_sub_style *) { }
int nuvio_subs_delay_ms() { return 0; }
int emby5_bs_active() { return 0; }
float evo_audio_speed() { return 1.0f; }
void evo_audio_set_speed(float) { }
void evo_agc_upscale_set_mode(int) { }
}
