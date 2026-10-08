/* Emby5 IPTV Xtream client. SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/iptv_xtream.h"
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
    if (!r.ok()) { if (error) *error = r.error.empty() ? "Xtream HTTP " + std::to_string(r.status) : r.error; return false; }
    if (r.body.size() > (32u << 20)) { if (error) *error = "Channel response too large"; return false; }
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
    if (!valid(c)) { if (error) *error = "Enter an HTTP(S) server URL, username and password"; return false; }
    std::string body;
    if (!response(c, "", &body, error)) return false;
    cJSON *j = cJSON_Parse(body.c_str());
    if (!j) { if (error) *error = "Invalid Xtream response"; return false; }
    const cJSON *u = cJSON_GetObjectItemCaseSensitive(j, "user_info");
    const std::string auth = field(u, "auth");
    const std::string status = field(u, "status");
    const bool ok = auth == "1" && (status.empty() || status == "Active");
    if (!ok && error) *error = "Xtream authentication failed or subscription inactive";
    cJSON_Delete(j);
    return ok;
}
bool load_catalog(const Credentials &c, Catalog *catalog, std::string *error) {
    if (!catalog || !valid(c)) return false;
    Catalog fresh;
    std::string body;
    if (!response(c, "get_live_categories", &body, error)) return false;
    cJSON *j = cJSON_Parse(body.c_str());
    if (!cJSON_IsArray(j)) { cJSON_Delete(j); if (error) *error = "Invalid category list"; return false; }
    const cJSON *v;
    cJSON_ArrayForEach(v, j) {
        Category cat{field(v, "category_id"), field(v, "category_name")};
        if (!cat.id.empty()) fresh.categories.push_back(std::move(cat));
    }
    cJSON_Delete(j);
    if (!response(c, "get_live_streams", &body, error)) return false;
    j = cJSON_Parse(body.c_str());
    if (!cJSON_IsArray(j)) { cJSON_Delete(j); if (error) *error = "Invalid channel list"; return false; }
    cJSON_ArrayForEach(v, j) {
        Channel ch{field(v, "stream_id"), field(v, "name"), field(v, "category_id"), field(v, "stream_icon"), field(v, "container_extension")};
        if (!ch.id.empty() && !ch.name.empty()) fresh.channels.push_back(std::move(ch));
    }
    cJSON_Delete(j);
    *catalog = std::move(fresh);
    return true;
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
