/* Emby5 IPTV Xtream client. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "app/catalogue_state.h"
#include <string>
#include <functional>
#include <cstdint>
#include <vector>
namespace iptv_xtream {
struct Credentials { std::string server, username, password; };
struct Channel { std::string id, name, category_id, logo, extension;
    std::string source = "Xtream", emby_id, now, next, guide_time;
    int64_t now_start=0, now_end=0, next_start=0, next_end=0;
    std::string category_key; // Stable provider ID, separate from the displayed group name.
};
using Category = iptv_catalogue::Category;
struct Catalog { std::vector<Category> categories; std::vector<Channel> channels; iptv_catalogue::Scan scan; };
std::string api_url(const Credentials &credentials, const std::string &action);
std::string live_url(const Credentials &credentials, const Channel &channel);
bool authenticate(const Credentials &credentials, std::string *error);
bool load_catalog(const Credentials &credentials, Catalog *catalog, std::string *error, int budget_seconds=120, std::function<bool()> cancelled={}, bool retry_failed=false,
                  std::function<void(const iptv_catalogue::Scan &)> progress={});
bool save_credentials(const Credentials &credentials);
bool load_credentials(Credentials *credentials);
}
