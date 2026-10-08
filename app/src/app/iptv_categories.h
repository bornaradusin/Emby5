/* Emby5 user-managed IPTV categories. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <string>
#include <vector>
namespace iptv_categories {
struct Category { std::string id, name; std::vector<std::string> channels; };
class Store {
public:
    bool load(const std::string &account_key);
    bool save() const;
    bool create(const std::string &name);
    bool rename(const std::string &id, const std::string &name);
    bool erase(const std::string &id);
    bool move(const std::string &id, int direction);
    bool assign(const std::string &id, const std::string &channel_id, bool included);
    bool move_channel(const std::string &id, const std::string &channel_id, int direction);
    const std::vector<Category> &categories() const { return categories_; }
private:
    std::string path_;
    std::vector<Category> categories_;
};
}
