/* Emby5 IPTV Xtream client. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <string>
#include <cstdint>
#include <vector>
namespace iptv_xtream {
struct Credentials { std::string server, username, password; };
struct Channel { std::string id, name, category_id, logo, extension;
    std::string source = "Xtream", emby_id, now, next, guide_time;
    int64_t now_start=0, now_end=0, next_start=0, next_end=0; };
struct Category { std::string id, name; };
struct Catalog { std::vector<Category> categories; std::vector<Channel> channels; };
std::string api_url(const Credentials &credentials, const std::string &action);
std::string live_url(const Credentials &credentials, const Channel &channel);
bool authenticate(const Credentials &credentials, std::string *error);
bool load_catalog(const Credentials &credentials, Catalog *catalog, std::string *error);
bool save_credentials(const Credentials &credentials);
bool load_credentials(Credentials *credentials);
}
