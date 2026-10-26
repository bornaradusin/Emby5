/* Emby5 IPTV Xtream client. SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/iptv_xtream.h"
#include "app/i18n.h"
#include "app/request_budget.h"
#include "app/xtream_fetch.h"
#include <memory>
#include <unordered_set>
#include "jf/jf_http.h"
#include "evo_data_path.h"
extern "C" {
#include "cJSON.h"
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
namespace iptv_xtream {
namespace {
std::string field(const cJSON *j, const char *name) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, name);
    if (cJSON_IsString(v)) return v->valuestring ? v->valuestring : "";
    if (cJSON_IsNumber(v)) return std::to_string(v->valueint);
    return "";
}
std::string escape(const std::string &s) {
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') out += (char)c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
std::string root(const Credentials &c) {
    std::string s = c.server;
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}
bool valid(const Credentials &c) {
    return (c.server.rfind("http://", 0) == 0 || c.server.rfind("https://", 0) == 0) &&
           c.server.size() < 1024 && !c.username.empty() && !c.password.empty() &&
           c.username.size() <= 255 && c.password.size() <= 255;
}
std::string path() { return evo_data_path("emby5/iptv-xtream.json"); }
bool response(const Credentials &c, const std::string &action, std::string *body, std::string *error) {
    auto r = jf::http_request("GET", api_url(c, action), {}, "", 25);
    if (!r.ok() || !r.error.empty()) { if (error) *error = (!r.ok() && r.status) ? "Xtream HTTP " + std::to_string(r.status) : N_("Xtream connection failed or timed out"); return false; }
    if (r.body.size() > (32u << 20)) { if (error) *error = N_("Channel response too large"); return false; }
    *body = std::move(r.body);
    return true;
}
}
std::string api_url(const Credentials &c, const std::string &action) {
    if (!valid(c)) return {};
    std::string url = root(c) + "/player_api.php?username=" + escape(c.username) + "&password=" + escape(c.password);
    if (!action.empty()) url += "&action=" + escape(action);
    return url;
}
std::string live_url(const Credentials &c, const Channel &ch) {
    if (!valid(c) || ch.id.empty()) return {};
    const std::string ext = ch.extension == "m3u8" ? "m3u8" : "ts";
    return root(c) + "/live/" + escape(c.username) + "/" + escape(c.password) + "/" + escape(ch.id) + "." + ext;
}
bool authenticate(const Credentials &c, std::string *error) {
    if (!valid(c)) { if (error) *error = N_("Enter an HTTP(S) server URL, username and password"); return false; }
    std::string body;
    if (!response(c, "", &body, error)) return false;
    cJSON *j = cJSON_Parse(body.c_str());
    if (!j) { if (error) *error = N_("Invalid Xtream response"); return false; }
    const cJSON *u = cJSON_GetObjectItemCaseSensitive(j, "user_info");
    const std::string auth = field(u, "auth");
    const std::string status = field(u, "status");
    const bool ok = auth == "1" && (status.empty() || status == "Active");
    if (!ok && error) *error = N_("Xtream authentication failed or subscription inactive");
    cJSON_Delete(j);
    return ok;
}
bool load_catalog(const Credentials &c, Catalog *catalog, std::string *error,
                  int budget_seconds, std::function<bool()> cancelled, bool retry_failed,
                  std::function<void(const iptv_catalogue::Scan &)> progress,
                  std::function<void(const Catalog &)> available) {
    if(error)error->clear();
    if(!catalog || !valid(c)) {if(error)*error=N_("Enter an HTTP(S) server URL, username and password");return false;}
    emby5::RequestBudget budget(budget_seconds,cancelled);
    std::vector<Channel> fresh;std::unordered_set<std::string> seen;
    auto collect=[&](const cJSON *array,const std::string &category,const std::vector<Category> &) {
        const cJSON *v;
        cJSON_ArrayForEach(v,array) {
            Channel ch{field(v,"stream_id"),field(v,"name"),field(v,"category_id"),field(v,"stream_icon"),field(v,"container_extension")};
            if(ch.category_id.empty())ch.category_id=category;
            ch.category_key=ch.category_id;
            if(seen.insert(ch.id).second)fresh.push_back(std::move(ch));
        }
    };
    size_t published=0;auto published_at=std::chrono::steady_clock::time_point{};
    const auto report=[&](const iptv_catalogue::Scan &state) {
        if(progress)progress(state);
        if(!available || (cancelled&&cancelled()) || fresh.size()==published)return;
        const auto now=std::chrono::steady_clock::now();
        if(published&&fresh.size()-published<4096&&now-published_at<std::chrono::milliseconds(500))return;
        Catalog partial;partial.categories=state.categories;partial.scan=state;
        partial.scan.complete=false;partial.scan.error.clear();partial.channels=fresh;
        // Keep missing entries until a category has completed successfully.
        for(const auto &old:catalog->channels) {
            const auto &group=old.category_key.empty()?old.category_id:old.category_key;
            if(!seen.count(old.id)&&!state.completed.count(group))partial.channels.push_back(old);
        }
        available(partial);published=fresh.size();published_at=now;
    };
    auto scan=iptv_catalogue::fetch(c,"get_live_categories","get_live_streams","stream_id",budget,
                                   collect,false,retry_failed?&catalog->scan:nullptr,report);
    // Session/provider changes discard the result, including partial responses.
    if(cancelled&&cancelled()) {if(error)*error=N_("Channel refresh cancelled");return false;}
    if(!scan.whole && (!scan.complete || scan.incremental)) {
        std::unordered_set<std::string> known;
        for(const auto &cat:scan.categories)known.insert(cat.id);
        for(auto &old:catalog->channels) {
            std::string key=old.category_key.empty()?old.category_id:old.category_key;
            if(old.category_key.empty()) {
                std::string match;unsigned matches=0;
                for(const auto &cat:scan.categories)if(cat.name==old.category_id){match=cat.id;++matches;}
                if(matches==1){key=match;old.category_key=key;old.category_id=key;}
            }
            if(scan.completed.count(key)||seen.count(old.id))continue;
            if(scan.complete && !known.count(key))continue;
            seen.insert(old.id);fresh.push_back(std::move(old));
        }
    }
    // Preserve old labels for failed/unlisted categories until a full refresh.
    std::vector<Category> categories=scan.categories;
    std::unordered_set<std::string> ids;for(const auto &v:categories)ids.insert(v.id);
    if(!scan.complete)for(const auto &v:catalog->categories)if(ids.insert(v.id).second)categories.push_back(v);
    catalog->categories=std::move(categories);catalog->channels=std::move(fresh);catalog->scan=std::move(scan);
    if(error)*error=catalog->scan.error;
    return catalog->scan.complete;
}

bool save_credentials(const Credentials &c) {
    if (!valid(c)) return false;
    evo_mkdir(evo_data_dir()); evo_mkdir(evo_data_path("emby5"));
    cJSON *j = cJSON_CreateObject();
    if (!j) return false;
    cJSON_AddStringToObject(j, "server", c.server.c_str());
    cJSON_AddStringToObject(j, "username", c.username.c_str());
    cJSON_AddStringToObject(j, "password", c.password.c_str());
    char *body = cJSON_PrintUnformatted(j); cJSON_Delete(j);
    if (!body) return false;
    std::string tmp = path() + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    bool ok = f && std::fputs(body, f) >= 0;
    if (f && std::fclose(f) != 0) ok = false;
    std::free(body);
    if (ok) ok = std::rename(tmp.c_str(), path().c_str()) == 0;
    if (!ok) std::remove(tmp.c_str());
    return ok;
}
bool load_credentials(Credentials *c) {
    if (!c) return false;
    FILE *f = std::fopen(path().c_str(), "rb");
    if (!f) return false;
    std::string body;
    char buf[1024]; size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) && body.size() < 8192) body.append(buf, n);
    std::fclose(f);
    cJSON *j = cJSON_Parse(body.c_str());
    if (!j) return false;
    Credentials fresh{field(j, "server"), field(j, "username"), field(j, "password")};
    cJSON_Delete(j);
    if (!valid(fresh)) return false;
    *c = std::move(fresh);
    return true;
}
}
