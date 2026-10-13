/* Emby5 release information only. Never downloads or installs application updates. */
#include "app/update_service.h"
#include "app/io_worker.h"
#include "jf/jf_http.h"
#include <array>
#include <cctype>
#include <mutex>
#include <set>
#include <string>
extern "C" {
#include "cJSON.h"
}
#ifndef EMBY5_VERSION
#define EMBY5_VERSION "0.0.0"
#endif
namespace update_service {
namespace {
std::mutex lock;
Snapshot state;
bool automatic_enabled=true, signed_in=false, automatic_attempted=false;
uint64_t automatic_epoch=0, pending_epoch=0;
std::string pending_version;
std::set<std::array<unsigned,3>> delivered;

// Compare normal and padded content versions numerically. Ignore draft/prerelease
// and malformed tags rather than advertising an arbitrary number from a title.
bool version_parts(const std::string &text, std::array<unsigned,3> &parts) {
    size_t i=0;
    if(i<text.size() && (text[i]=='v'||text[i]=='V')) ++i;
    for(size_t part=0; part<parts.size(); ++part) {
        if(i==text.size() || text[i]<'0'||text[i]>'9') return false;
        unsigned n=0;
        do {
            if(n>99999) return false;
            n=n*10+(unsigned)(text[i++]-'0');
        } while(i<text.size() && text[i]>='0' && text[i]<='9');
        parts[part]=n;
        if(part+1<parts.size() && (i==text.size()||text[i++]!='.')) return false;
    }
    return i==text.size();
}
bool newer(const std::string &remote) {
    std::array<unsigned,3> a{},b{};
    return version_parts(remote,a) && version_parts(EMBY5_VERSION,b) && a>b;
}
void maybe_start_automatic();
void failed_request() {
    {
        std::lock_guard<std::mutex> guard(lock);
        state.checking=false;
        state.message="Could not read release information. Close and reopen What's New to retry.";
        ++state.revision;
    }
    // A fresh enable while the failed request ran may have queued one new check.
    maybe_start_automatic();
}
void fetch(bool automatic, uint64_t epoch) {
    const auto r=jf::http_request("GET",
        "https://api.github.com/repos/bornaradusin/Emby5/releases/latest",
        {"Accept: application/vnd.github+json", "User-Agent: Emby5-PS5"}, "", 12);
    std::string version,notes;
    if(r.ok()) {
        cJSON *j=cJSON_Parse(r.body.c_str());
        if(j) {
            const auto *tag=cJSON_GetObjectItemCaseSensitive(j,"tag_name");
            const auto *body=cJSON_GetObjectItemCaseSensitive(j,"body");
            std::array<unsigned,3> parsed{};
            if(!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"draft")) &&
               !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"prerelease")) &&
               cJSON_IsString(tag) && tag->valuestring && version_parts(tag->valuestring,parsed)) {
                version=tag->valuestring;
                if(cJSON_IsString(body) && body->valuestring) notes=body->valuestring;
            }
            cJSON_Delete(j);
        }
    }
    {
        std::lock_guard<std::mutex> guard(lock);
        state.checking=false;
        if(!version.empty()) {
            state.latest=version;
            state.notes=notes;
            state.available=newer(version);
            state.message=state.available?"New version available - see What's New":"You're up to date";
            // Check again at publication, not just when the request started.
            if(automatic && automatic_enabled && signed_in && epoch==automatic_epoch && state.available) {
                std::array<unsigned,3> parsed{};
                if(version_parts(version,parsed) && !delivered.count(parsed)) {
                    pending_version=version;
                    pending_epoch=epoch;
                }
            }
        } else {
            // Last valid notes remain readable; network failures never generate
            // an unsolicited kernel notification and do not erase usable notes.
            state.message="Could not read release information. Close and reopen What's New to retry.";
        }
        ++state.revision;
    }
    maybe_start_automatic();
}
void launch(bool automatic, uint64_t epoch) {
    if(!emby5::spawn_io([automatic,epoch]{fetch(automatic,epoch);},failed_request))
        failed_request();
}
void maybe_start_automatic() {
    uint64_t epoch;
    {
        std::lock_guard<std::mutex> guard(lock);
        if(!automatic_enabled || !signed_in || automatic_attempted || state.checking) return;
        automatic_attempted=true;
        state.checking=true;
        ++state.revision;
        epoch=automatic_epoch;
    }
    launch(true,epoch);
}
}
void set_automatic_enabled(bool enabled) {
    {
        std::lock_guard<std::mutex> guard(lock);
        if(automatic_enabled==enabled) return;
        automatic_enabled=enabled;
        ++automatic_epoch;
        pending_version.clear();
        automatic_attempted=false;
        ++state.revision;
    }
    // Never wait for HTTP on the controller thread. Old requests may finish
    // their bounded read, but cannot send a notification after being disabled.
    if(enabled) maybe_start_automatic();
}
void automatic_check() {
    { std::lock_guard<std::mutex> guard(lock); signed_in=true; }
    maybe_start_automatic();
}
void check() {
    uint64_t epoch;
    {
        std::lock_guard<std::mutex> guard(lock);
        if(state.checking) return; // reuse an already running notes request
        state.checking=true;
        ++state.revision;
        epoch=automatic_epoch;
    }
    launch(false,epoch);
}
std::string take_notification() {
    std::lock_guard<std::mutex> guard(lock);
    if(pending_version.empty()) return {};
    std::string version;
    version.swap(pending_version);
    if(!automatic_enabled || !signed_in || pending_epoch!=automatic_epoch) return {};
    std::array<unsigned,3> parsed{};
    if(!version_parts(version,parsed) || !delivered.insert(parsed).second) return {};
    return "Emby5 " + version + " is available. See Settings > System > What's New.";
}
uint64_t generation() { std::lock_guard<std::mutex> guard(lock); return state.revision; }
Snapshot snapshot() { std::lock_guard<std::mutex> guard(lock); return state; }
}
