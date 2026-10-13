/* White-box test: run the real message dispatcher, without opening a socket. */
#include "../../app/src/app/remote.cpp"
#include <cassert>
#include <iostream>
namespace syncplay {
void on_group_update(const cJSON *) { }
void on_command(const cJSON *) { }
}
// These paths are deliberately not exercised. Define fail-fast stubs so an
// ASan-instrumented translation unit also links without real socket libraries.
namespace jf {
std::string Client::websocket_url() const { std::abort(); }
std::string Client::auth_header() const { std::abort(); }
bool Client::post_capabilities() { std::abort(); }
bool WebSocket::open(const std::string &, const std::vector<std::string> &, std::string *) { std::abort(); }
bool WebSocket::send_text(const std::string &) { std::abort(); }
int WebSocket::recv(std::string *, int) { std::abort(); }
void WebSocket::close() { std::abort(); }
}
int main() {
    remote::s_generation=7;
    const std::string event=R"({"MessageType":"UserDataChanged","Data":{"UserId":"A","UserDataList":[{"ItemId":"ep1","Played":true}]}})";
    remote::handle(event,"A",7); remote::handle(event,"A",7);
    assert(remote::take_user_data_changed());
    assert(!remote::take_user_data_changed());
    remote::handle(event,"B",7); assert(!remote::take_user_data_changed());
    remote::handle(event,"A",6); assert(!remote::take_user_data_changed());
    remote::handle("{}","A",7); assert(!remote::take_user_data_changed());
    const std::string next=R"({"MessageType":"Playstate","Data":{"Command":"NextTrack"}})";
    remote::handle(next,"A",6); remote::Command c; assert(!remote::take(&c));
    remote::handle(next,"A",7); assert(remote::take(&c) && c.kind==remote::Command::Next);
    std::cout << "PASS: real Emby UserDataChanged dispatcher, coalescing, user isolation, old-session drop\n";
}
