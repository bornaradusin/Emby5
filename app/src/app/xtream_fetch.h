/* Bounded, partial-safe Xtream catalogue requests. GPL-3.0-or-later */
#pragma once
#include "app/iptv_xtream.h"
#include "app/request_budget.h"
#include "jf/jf_http.h"
extern "C" {
#include "cJSON.h"
}
#include <functional>
#include <memory>
#include <utility>
namespace iptv_catalogue {
using Json=std::unique_ptr<cJSON,decltype(&cJSON_Delete)>;
inline std::string field(const cJSON *j,const char *name) {
    const auto *v=cJSON_GetObjectItemCaseSensitive(j,name);
    if(cJSON_IsString(v)&&v->valuestring)return v->valuestring;
    if(cJSON_IsNumber(v))return std::to_string(v->valueint);
    return {};
}
inline std::string encode(const std::string &s) {
    const char *hex="0123456789ABCDEF";std::string out;
    for(unsigned char c:s) {
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')out+=(char)c;
        else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
    }
    return out;
}
inline Json read_array(const std::string &url, emby5::RequestBudget &budget, std::string &error) {
    const int seconds=budget.timeout();
    if(!seconds){error="Refresh deadline reached or cancelled";return Json(nullptr,cJSON_Delete);}
    // Do not put raw URLs, tokens, passwords or transport exception text in UI/logs.
    try {
        auto r=jf::http_request("GET",url,{},"",seconds);
        if(!r.ok()||!r.error.empty()) {
            error=(!r.ok()&&r.status)?"HTTP "+std::to_string(r.status):"Connection failed, incomplete or timed out";
            return Json(nullptr,cJSON_Delete);
        }
        if(r.body.size()>(32u<<20)){error="Catalogue response too large";return Json(nullptr,cJSON_Delete);}
        Json j(cJSON_ParseWithOpts(r.body.c_str(),nullptr,1),cJSON_Delete);
        if(!cJSON_IsArray(j.get())){error="Invalid catalogue JSON";return Json(nullptr,cJSON_Delete);}
        if(budget.expired()){error="Refresh deadline reached or cancelled";return Json(nullptr,cJSON_Delete);}
        return j;
    } catch(...) {error="Catalogue request failed";return Json(nullptr,cJSON_Delete);}
}
using Consume=std::function<void(const cJSON *,const std::string &,const std::vector<Category> &)>;
// Consume is transactional at category granularity: validate the entire array
// before invoking it. A storage exception propagates to the owner; no partial
// file/index is published. A valid empty array IS an authoritative empty result.
inline Scan fetch(const iptv_xtream::Credentials &creds, const std::string &category_action,
                  const std::string &item_action,const char *id_field,
                  emby5::RequestBudget &budget,const Consume &consume,bool prefer_whole,
                  const Scan *retry=nullptr,std::function<void(const Scan &)> progress={}) {
    Scan out;std::string error;std::vector<std::string> ids;
    bool resuming=retry && retry->list_ok && !retry->categories.empty() && !retry->failed.empty();
    if(resuming) {
        std::unordered_set<std::string> known;
        for(const auto &cat:retry->categories)known.insert(cat.id);
        for(const auto &id:retry->failed)if(id.empty()||!known.count(id))resuming=false;
    }
    out.incremental=resuming;
    if(resuming) {
        out.categories=retry->categories;out.list_ok=true;ids=retry->failed;
        out.total=out.categories.size();out.done=out.total>=ids.size()?out.total-ids.size():0;
    } else {
        auto cats=read_array(iptv_xtream::api_url(creds,category_action),budget,error);
        out.list_ok=cJSON_IsArray(cats.get());std::unordered_set<std::string> seen;
        const cJSON *cat;
        if(out.list_ok)cJSON_ArrayForEach(cat,cats.get()) {
            Category v{field(cat,"category_id"),field(cat,"category_name")};
            if(v.id.empty()){out.list_ok=false;continue;}
            if(seen.insert(v.id).second)out.categories.push_back(std::move(v));
        }
        if(!out.list_ok)out.categories.clear();
        for(const auto &cat:out.categories)ids.push_back(cat.id);
        out.total=ids.size();
    }
    const auto notify=[&]{if(progress)progress(out);};notify();
    auto part=[&](const std::string &id)->bool {
        auto url=iptv_xtream::api_url(creds,item_action);
        if(!id.empty())url+="&category_id="+encode(id);
        auto arr=read_array(url,budget,error);if(!arr)return false;
        size_t checked=0;const cJSON *v;
        cJSON_ArrayForEach(v,arr.get()) {
            if(field(v,id_field).empty()||field(v,"name").empty()) {
                error="Invalid catalogue item";return false;
            }
            if((++checked%256)==0&&budget.expired()) {error="Refresh deadline reached or cancelled";return false;}
        }
        if(budget.expired()){error="Refresh deadline reached or cancelled";return false;}
        consume(arr.get(),id,out.categories);out.changed=true;return true;
    };
    if(!resuming && (prefer_whole||ids.empty())) {
        if(part("")) {
            out.whole=out.complete=true;out.done=out.total?out.total:1;out.total=out.done;
            out.error.clear();notify();return out;
        }
        if(ids.empty()) {
            out.total=1;out.failed.push_back("");out.error=error;notify();return out;
        }
    }
    for(const auto &id:ids) {
        if(part(id)){out.completed.insert(id);++out.done;}
        else out.failed.push_back(id);
        notify();
    }
    out.complete=out.list_ok&&out.failed.empty();
    if(!out.complete)out.error=error.empty()?"Catalogue refresh incomplete":error;
    notify();return out;
}
// Footer metadata travels atomically with a VOD cache; Live keeps it in its JSON.
// No credential, authentication token or playback URL is stored here.
inline cJSON *scan_json(const Scan &s) {
    cJSON *j=cJSON_CreateObject();if(!j)return nullptr;
    cJSON_AddBoolToObject(j,"complete",s.complete);cJSON_AddBoolToObject(j,"listOk",s.list_ok);
    cJSON_AddStringToObject(j,"error",s.error.c_str());
    cJSON_AddNumberToObject(j,"done",(double)s.done);cJSON_AddNumberToObject(j,"total",(double)s.total);
    auto *cats=cJSON_AddArrayToObject(j,"categories");
    for(const auto &c:s.categories){auto *v=cJSON_CreateObject();cJSON_AddStringToObject(v,"id",c.id.c_str());cJSON_AddStringToObject(v,"name",c.name.c_str());cJSON_AddItemToArray(cats,v);}
    auto *failed=cJSON_AddArrayToObject(j,"failed");for(const auto &id:s.failed)cJSON_AddItemToArray(failed,cJSON_CreateString(id.c_str()));
    return j;
}
inline Scan read_scan(const cJSON *j) {
    Scan s;if(!cJSON_IsObject(j))return s;
    s.complete=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"complete"));
    s.list_ok=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"listOk"));
    s.error=field(j,"error").substr(0,160);
    const cJSON *v;std::unordered_set<std::string> seen;
    cJSON_ArrayForEach(v,cJSON_GetObjectItemCaseSensitive(j,"categories")) {
        Category c{field(v,"id"),field(v,"name")};if(!c.id.empty()&&seen.insert(c.id).second)s.categories.push_back(std::move(c));
    }
    seen.clear();
    cJSON_ArrayForEach(v,cJSON_GetObjectItemCaseSensitive(j,"failed"))if(cJSON_IsString(v)&&v->valuestring&&seen.insert(v->valuestring).second)s.failed.push_back(v->valuestring);
    s.total=s.categories.empty()?1:s.categories.size();
    s.done=s.total>=s.failed.size()?s.total-s.failed.size():0;
    // Missing/contradictory metadata must not suppress a refresh as a fresh cache.
    if(!s.failed.empty())s.complete=false;
    if(!s.complete&&s.error.empty())s.error="Cached catalogue requires refresh";
    return s;
}
}
