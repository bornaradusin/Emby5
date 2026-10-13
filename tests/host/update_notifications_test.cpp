/* Actual notification service, bounded IO workers and controlled HTTP replies. */
#include "app/update_service.h"
#include "jf/jf_http.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
std::mutex mutex;
std::condition_variable cv;
std::string reply=R"({"tag_name":"1.2.5","body":"Release five"})";
int status=200,calls=0,active=0,max_active=0;
bool block=false,throws=false;
void config(std::string body, int code=200, bool hold=false, bool error=false) {
    std::lock_guard<std::mutex> l(mutex); reply=std::move(body);status=code;block=hold;throws=error;
}
int count(){std::lock_guard<std::mutex> l(mutex);return calls;}
template<class Predicate> void wait(Predicate predicate) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!predicate()) {
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void idle(int expected) { wait([&]{return count()==expected&&!update_service::snapshot().checking;}); }
void release() { {std::lock_guard<std::mutex> l(mutex);block=false;}cv.notify_all(); }
void cycle() {update_service::set_automatic_enabled(false);update_service::set_automatic_enabled(true);}
}
namespace jf {
HttpResponse http_request(const std::string &method,const std::string &url,
                          const std::vector<std::string>&headers,const std::string &body,int timeout) {
    assert(method=="GET"&&body.empty()&&timeout==12);
    assert(url=="https://api.github.com/repos/bornaradusin/Emby5/releases/latest");
    for(const auto &h:headers) assert(h.find("Authorization")==std::string::npos);
    std::unique_lock<std::mutex> l(mutex);
    const auto data=reply;const auto code=status;const bool error=throws;
    ++calls;++active;max_active=std::max(max_active,active);cv.notify_all();
    cv.wait(l,[]{return !block;});--active;
    if(error)throw std::runtime_error("transport exception");
    HttpResponse r;r.status=code;r.body=data;return r;
}
}
int main() {
    using namespace update_service;
    assert(!snapshot().checking&&generation()==0&&count()==0);
    set_automatic_enabled(false);automatic_check();automatic_check();assert(count()==0);
    // Manual notes work with notifications disabled, and do not turn them on.
    check();idle(1);assert(snapshot().notes=="Release five"&&take_notification().empty());
    config(R"({"tag_name":"1.2.5","body":"Release five"})",200,true);
    set_automatic_enabled(true);wait([]{return count()==2;});
    for(int i=0;i<100;++i) {automatic_check();check();}
    assert(count()==2);
    set_automatic_enabled(false);release();idle(2);assert(take_notification().empty());
    set_automatic_enabled(true);idle(3);
    assert(take_notification()=="Emby5 1.2.5 is available. See Settings > System > What's New.");
    assert(take_notification().empty());
    for(int i=0;i<100;++i)automatic_check();assert(count()==3);
    cycle();idle(4);assert(take_notification().empty()); // same release already delivered
    config(R"({"tag_name":"1.2.6","body":"Release six"})");
    check();idle(5);assert(take_notification().empty()&&snapshot().available); // manual is silent
    cycle();idle(6);set_automatic_enabled(false);assert(take_notification().empty()); // queued notice removed
    set_automatic_enabled(true);idle(7);assert(take_notification().find("1.2.6")!=std::string::npos);
    assert(take_notification().empty());
    config("unavailable",503);cycle();idle(8);
    assert(take_notification().empty()&&snapshot().notes=="Release six");
    config(R"({"tag_name":"1.2.7","body":"Release seven"})",200,true);
    cycle();wait([]{return count()==9;});set_automatic_enabled(false);set_automatic_enabled(true);
    assert(count()==9);release();idle(10);assert(take_notification().find("1.2.7")!=std::string::npos);
    config(R"({"tag_name":"v01.002.007","body":"Same numeric release"})");
    cycle();idle(11);assert(take_notification().empty());
    const auto retained=snapshot().notes;
    config("oops",200,false,true);cycle();idle(12);assert(take_notification().empty()&&!snapshot().checking);
    // Invalid/draft/prerelease tags never make false update notifications.
    int expected=12;
    for(const std::string &body:{"{}","broken",R"({"tag_name":"Release 999"})",R"({"tag_name":"1.2.9-beta"})",
        R"({"tag_name":"9.0.0","draft":true})",R"({"tag_name":"9.0.0","prerelease":true})"}) {
        config(body);cycle();idle(++expected);assert(take_notification().empty());assert(snapshot().notes==retained);
    }
    config(R"({"tag_name":"1.2.4","body":"Current version"})");cycle();idle(++expected);
    assert(!snapshot().available&&take_notification().empty());
    // A manual request already running while enabling cannot itself notify. The
    // separately queued automatic check is the only candidate for delivery.
    set_automatic_enabled(false);config(R"({"tag_name":"2.0.0","body":"Major release"})",200,true);
    check();wait([&]{return count()==expected+1;});set_automatic_enabled(true);release();expected+=2;idle(expected);
    assert(take_notification().find("2.0.0")!=std::string::npos);
    set_automatic_enabled(false);assert(take_notification().empty());
    assert(max_active==1);
    std::cout<<"PASS: actual update notifier: manual/off, enable, in-flight/queued disable, coalescing, once-per-version, account checks, errors and version validation ("<<count()<<" HTTP checks)\n";
}
