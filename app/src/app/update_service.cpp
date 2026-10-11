/* Emby5 release-notes-only notifier. Never downloads or installs application updates. */
#include "app/update_service.h"
#include "jf/jf_http.h"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
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
std::string pending;
bool started=false;
std::vector<int> parts(const std::string &s) {
    std::vector<int> out; size_t i=0;
    while(i<s.size() && out.size()<4) {
        if(s[i]<'0'||s[i]>'9') { ++i; continue; }
        int n=0;
        while(i<s.size() && s[i]>='0' && s[i]<='9') { n=std::min(999999,n*10+s[i]-'0'); ++i; }
        out.push_back(n);
    }
    return out;
}
bool newer(const std::string &remote) {
    auto a=parts(remote), b=parts(EMBY5_VERSION);
    a.resize(std::max(a.size(),b.size()),0); b.resize(a.size(),0);
    return a>b;
}
void fetch() {
    const jf::HttpResponse r=jf::http_request("GET",
        "https://api.github.com/repos/bornaradusin/Emby5/releases/latest",
        {"Accept: application/vnd.github+json", "User-Agent: Emby5-PS5"}, "", 12);
    std::string version, notes;
    if(r.ok()) {
        cJSON *j=cJSON_Parse(r.body.c_str());
        if(j) {
            const cJSON *tag=cJSON_GetObjectItemCaseSensitive(j,"tag_name");
            const cJSON *body=cJSON_GetObjectItemCaseSensitive(j,"body");
            if(cJSON_IsString(tag) && tag->valuestring) version=tag->valuestring;
            if(cJSON_IsString(body) && body->valuestring) notes=body->valuestring;
            cJSON_Delete(j);
        }
    }
    std::lock_guard<std::mutex> guard(lock);
    state.checking=false;
    state.available=!version.empty() && newer(version);
    state.latest=version;
    state.notes=notes; /* What's New can also show the current release. */
    state.message=state.available?"New version available - What's new":
        (version.empty()?"Could not check for new releases":"You're up to date");
    if(state.available) pending="Emby5 " + version + " available - What's new in Settings";
}
}
void check() {
    std::lock_guard<std::mutex> guard(lock);
    if(started) return;
    started=true; state.checking=true;
    std::thread(fetch).detach();
}
std::string take_notification() { std::lock_guard<std::mutex> guard(lock); std::string result; result.swap(pending); return result; }
Snapshot snapshot() { std::lock_guard<std::mutex> guard(lock); return state; }
}
