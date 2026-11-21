/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/collections_index.h"
#include "app/io_worker.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace collections_index {
namespace {

constexpr double kFreshSeconds = 600;   /* a collection changed on the server shows within ten minutes */
constexpr double kRetrySeconds = 60;    /* after a failed read */
constexpr int kMaxCollections = 500, kMaxTitles = 1000;

struct Index {
    std::string account;
    double built = 0;
    std::vector<jf::Item> collections;
    std::multimap<std::string, size_t> of;   /* title id -> its collections' places */
};

std::mutex s_lock;                    /* s_index, s_failed */
std::mutex s_build;                   /* one read at a time */
std::shared_ptr<const Index> s_index;
std::string s_failed_account;
double s_failed_at = -1e9;

double now_s()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string account_of(const jf::Client &c) { return c.server() + "|" + c.user_id(); }

std::shared_ptr<const Index> current(const std::string &account)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (s_index && s_index->account == account && now_s() - s_index->built < kFreshSeconds)
        return s_index;
    return nullptr;
}

} // namespace

bool lookup(jf::Client &client, const std::string &item_id, std::vector<jf::Item> *out)
{
    out->clear();
    const std::shared_ptr<const Index> index = current(account_of(client));
    if (!index)
        return false;
    const auto range = index->of.equal_range(item_id);
    for (auto it = range.first; it != range.second; ++it)
        out->push_back(index->collections[it->second]);
    return true;
}

bool build(jf::Client &client)
{
    if (!client.signed_in())
        return false;
    const std::string account = account_of(client);
    std::lock_guard<std::mutex> b(s_build);
    if (current(account))
        return true;   /* read while this one waited */
    {
        std::lock_guard<std::mutex> g(s_lock);
        if (s_failed_account == account && now_s() - s_failed_at < kRetrySeconds)
            return false;
    }
    const jf::Page page = client.library("", "BoxSet", "SortName", false, 0, kMaxCollections);
    if (!page.ok) {
        std::lock_guard<std::mutex> g(s_lock);
        s_failed_account = account;
        s_failed_at = now_s();
        return false;
    }
    auto index = std::make_shared<Index>();
    index->account = account;
    index->collections = page.items;
    std::vector<std::vector<std::string>> titles(index->collections.size());
    std::vector<std::function<void()>> jobs;
    for (size_t i = 0; i < index->collections.size(); i++)
        jobs.push_back([&client, &titles, &index, i] {
            titles[i] = client.child_ids(index->collections[i].id, kMaxTitles);
        });
    emby5::run_io_jobs(jobs);
    for (size_t i = 0; i < titles.size(); i++)
        for (const std::string &id : titles[i])
            index->of.emplace(id, i);
    index->built = now_s();
    std::lock_guard<std::mutex> g(s_lock);
    s_index = std::move(index);
    return true;
}

} // namespace collections_index
