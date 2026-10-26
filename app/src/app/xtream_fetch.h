/* Bounded, partial-safe Xtream catalogue requests. GPL-3.0-or-later */
#pragma once
#include "app/iptv_xtream.h"
#include "app/i18n.h"
#include "app/request_budget.h"
#include "jf/jf_catalogue_http.h"
#include "app/catalogue_request_queue.h"
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
using Consume=std::function<void(const cJSON *,const std::string &,const std::vector<Category> &)>;
// Valid objects are emitted in bounded batches. Failed or interrupted categories
// remain incomplete; callers merge them with previous data, never erase it.
inline Scan fetch(const iptv_xtream::Credentials &creds, const std::string &category_action,
                  const std::string &item_action,const char *id_field,
                  emby5::RequestBudget &budget,const Consume &consume,bool prefer_whole,
                  const Scan *retry=nullptr,std::function<void(const Scan &)> progress={}) {
    (void)prefer_whole; // Always prefer smaller categories when the provider has them.
    Scan out;std::vector<std::string> ids;std::string error;
    const auto cancelled=[&]{return budget.stopped();};
    const auto key=creds.server+"\n"+creds.username;
    const auto notify=[&]{if(progress)progress(out);};
    auto transfer=[&](const std::string &url,const std::function<bool(const std::string&)> &accept) {
        const auto stop=[&]{return cancelled()||playback_active();};
        RequestTurn turn(key,stop);
        if(!turn){jf::CatalogueHttpResult r;r.cancelled=true;r.error=playback_active()?N_("Refresh paused during playback"):N_("Catalogue request cancelled");return r;}
        // Waiting for another section is not network inactivity for this request.
        budget.progress();
        auto r=jf::http_catalogue_array(url,accept,stop,[&](uint64_t){budget.progress();},90);
        if(r.cancelled&&playback_active())r.error=N_("Refresh paused during playback");
        return r;
    };
    bool resuming=retry&&retry->list_ok&&!retry->categories.empty()&&!retry->failed.empty();
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
        std::unordered_set<std::string> seen;
        auto cats=transfer(iptv_xtream::api_url(creds,category_action),[&](const std::string &object) {
            Json j(cJSON_ParseWithOpts(object.c_str(),nullptr,1),cJSON_Delete);
            Category c{field(j.get(),"category_id"),field(j.get(),"category_name")};
            if(!cJSON_IsObject(j.get())||c.id.empty())return false;
            if(seen.insert(c.id).second)out.categories.push_back(std::move(c));
            return out.categories.size()<=10000;
        });
        out.list_ok=cats.complete;
        if(!out.list_ok) {
            out.categories.clear();error=cats.error;
            // A rejected connection or credential does not justify downloading
            // an even larger whole-catalogue endpoint next.
            if(cats.transport_error||cats.cancelled||cats.status==401||cats.status==403) {
                out.total=1;out.failed={""};out.error=error;notify();return out;
            }
        }
        for(const auto &cat:out.categories)ids.push_back(cat.id);
        out.total=ids.size();
    }
    if(ids.empty()){ids.push_back("");out.total=1;}
    // Unvisited categories stay in failed until their COMPLETE response arrives.
    // A progressive snapshot can therefore never masquerade as a full refresh.
    out.failed=ids;notify();unsigned consecutive_transport_failures=0;
    for(const auto &id:ids) {
        if(cancelled()){error=N_("Catalogue request cancelled");break;}
        auto url=iptv_xtream::api_url(creds,item_action);
        if(!id.empty())url+="&category_id="+encode(id);
        Json batch(cJSON_CreateArray(),cJSON_Delete);if(!batch)throw std::bad_alloc();
        size_t pending=0;
        auto flush=[&] {
            if(!pending)return;
            if(cancelled())return;
            consume(batch.get(),id,out.categories);
            batch.reset(cJSON_CreateArray());if(!batch)throw std::bad_alloc();
            pending=0;out.changed=true;notify();
        };
        auto response=transfer(url,[&](const std::string &object) {
            Json item(cJSON_ParseWithOpts(object.c_str(),nullptr,1),cJSON_Delete);
            if(!cJSON_IsObject(item.get())||field(item.get(),id_field).empty()||field(item.get(),"name").empty())return false;
            // Keep the provider's category metadata where supplied. Some
            // catalogues return multi-category items; valid entries must not
            // be rejected merely because their primary category differs.
            cJSON_AddItemToArray(batch.get(),item.release());
            if(++pending>=256)flush();
            return !cancelled();
        });
        if(cancelled()){error=N_("Catalogue request cancelled");break;}
        flush(); // verified complete objects remain usable after a truncated reply
        if(response.complete) {
            // A valid empty array authoritatively clears just this category.
            if(!response.items)consume(batch.get(),id,out.categories);
            out.changed=true;out.completed.insert(id);++out.done;
            out.failed.erase(std::remove(out.failed.begin(),out.failed.end(),id),out.failed.end());
            if(id.empty())out.whole=true;
            consecutive_transport_failures=0;
        } else {
            error=response.error;
            if(response.transport_error)++consecutive_transport_failures;else consecutive_transport_failures=0;
        }
        budget.progress();out.error=error;notify();
        if(response.cancelled||response.status==401||response.status==403||consecutive_transport_failures>=3)break;
    }
    out.complete=out.failed.empty()&&(out.list_ok||out.whole);
    out.error=out.complete?"":error.empty()?N_("Catalogue refresh incomplete"):error;
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
    if(!s.complete&&s.error.empty())s.error=N_("Cached catalogue requires refresh");
    return s;
}
}
