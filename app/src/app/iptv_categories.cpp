/* Emby5 user-managed IPTV categories. SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/iptv_categories.h"
#include "evo_data_path.h"
extern "C" {
#include "cJSON.h"
}
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <string>
namespace iptv_categories {
namespace {
std::string read(const std::string &path) {
    std::string result;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return result;
    char buf[4096]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) && result.size() < (4u << 20)) result.append(buf, n);
    std::fclose(f); return result;
}
std::string field(const cJSON *j, const char *key) {
    const char *v = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, key));
    return v ? v : "";
}
bool valid(const std::string &name) { return !name.empty() && name.size() <= 80 && name.find_first_of("\r\n") == std::string::npos; }
std::string key_hash(const std::string &key) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) h = (h ^ c) * 1099511628211ULL;
    char b[20]; std::snprintf(b, sizeof b, "%016llx", (unsigned long long)h); return b;
}
}
bool Store::load(const std::string &account_key) {
    if (account_key.empty()) return false;
    path_ = std::string(evo_data_path(("emby5/iptv-categories-" + key_hash(account_key) + ".json").c_str()));
    categories_.clear();
    std::string body = read(path_);
    if (body.empty()) return true;
    cJSON *root = cJSON_Parse(body.c_str());
    if (!root) return false;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(root, "categories")) {
        Category c; c.id = field(entry, "id"); c.name = field(entry, "name");
        if (c.id.empty() || !valid(c.name)) continue;
        const cJSON *ch;
        cJSON_ArrayForEach(ch, cJSON_GetObjectItemCaseSensitive(entry, "channels")) {
            const char *v = cJSON_GetStringValue(ch);
            if (v && *v) c.channels.emplace_back(v);
        }
        categories_.push_back(std::move(c));
    }
    cJSON_Delete(root); return true;
}
bool Store::save() const {
    if(path_.empty() || evo_mkdir(evo_data_dir())!=0 || evo_mkdir(evo_data_path("emby5"))!=0)return false;
    cJSON *root=cJSON_CreateObject();if(!root)return false;
    struct Delete {cJSON *p;~Delete(){cJSON_Delete(p);}} cleanup{root};
    cJSON *arr=cJSON_AddArrayToObject(root,"categories");if(!arr)return false;
    for(const auto &c:categories_) {
        cJSON *item=cJSON_CreateObject();if(!item)return false;
        if(!cJSON_AddItemToArray(arr,item)){cJSON_Delete(item);return false;}
        if(!cJSON_AddStringToObject(item,"id",c.id.c_str()) ||
           !cJSON_AddStringToObject(item,"name",c.name.c_str()))return false;
        cJSON *chs=cJSON_AddArrayToObject(item,"channels");if(!chs)return false;
        for(const auto &ch:c.channels) {
            cJSON *v=cJSON_CreateString(ch.c_str());if(!v)return false;
            if(!cJSON_AddItemToArray(chs,v)){cJSON_Delete(v);return false;}
        }
    }
    char *json=cJSON_PrintUnformatted(root);if(!json)return false;
    const std::string tmp=path_+".tmp";
    FILE *f=std::fopen(tmp.c_str(),"wb");bool ok=false;
    if(f){ok=std::fputs(json,f)>=0;if(std::fclose(f))ok=false;}
    std::free(json);
    if(ok)ok=std::rename(tmp.c_str(),path_.c_str())==0;
    if(!ok)std::remove(tmp.c_str());
    return ok;
}
bool Store::create(const std::string &name) {
    if (!valid(name)) return false;
    uint64_t i = 1;
    std::string id;
    do { id = "category-" + std::to_string(i++); }
    while (std::any_of(categories_.begin(), categories_.end(), [&](const Category &c){return c.id == id;}));
    categories_.push_back({id,name,{}}); return save();
}
bool Store::rename(const std::string &id, const std::string &name) {
    if (!valid(name)) return false;
    for (auto &c : categories_) if (c.id == id) { c.name = name; return save(); }
    return false;
}
bool Store::erase(const std::string &id) {
    auto it = std::find_if(categories_.begin(),categories_.end(),[&](const Category &c){return c.id==id;});
    if (it == categories_.end()) return false;
    categories_.erase(it); return save();
}
bool Store::move(const std::string &id, int direction) {
    for (size_t i=0;i<categories_.size();++i) if (categories_[i].id==id) {
        if ((direction<0 && i==0) || (direction>0 && i+1==categories_.size()) || direction==0) return false;
        std::swap(categories_[i], categories_[(size_t)((int)i+(direction<0?-1:1))]); return save();
    }
    return false;
}
std::string Store::channel_key(const std::string &id) {
    return id.empty()||id.find(':')!=std::string::npos?id:"xtream:"+id;
}
bool Store::contains(const std::string &id,const std::string &channel) const {
    const auto key=channel_key(channel);
    for(const auto &c:categories_)if(c.id==id)
        return std::any_of(c.channels.begin(),c.channels.end(),[&](const std::string &v){return channel_key(v)==key;});
    return false;
}
bool Store::add_channels(const std::string &id,const std::vector<std::string> &channels,size_t *added) {
    if(added)*added=0;
    for(auto &c:categories_)if(c.id==id) {
        // Prepare first: an allocation failure must not leave half an import
        // in memory or allow a later save to publish an incomplete selection.
        std::vector<std::string> next;
        try {
            next=c.channels;std::unordered_set<std::string> present;
            for(const auto &v:next)present.insert(channel_key(v));
            for(const auto &v:channels)if(!v.empty()&&present.insert(channel_key(v)).second)next.push_back(channel_key(v));
        } catch(...) {return false;}
        const size_t count=next.size()-c.channels.size();
        if(!count)return true;
        c.channels.swap(next);
        bool ok=false;try{ok=save();}catch(...){}
        if(!ok){c.channels.swap(next);return false;}
        if(added)*added=count;
        return true;
    }
    return false;
}
bool Store::assign(const std::string &id,const std::string &channel,bool included) {
    if(channel.empty())return false;
    if(included)return add_channels(id,{channel});
    for(auto &c:categories_)if(c.id==id) {
        const auto old=c.channels;const auto key=channel_key(channel);
        c.channels.erase(std::remove_if(c.channels.begin(),c.channels.end(),[&](const std::string &v){return channel_key(v)==key;}),c.channels.end());
        if(c.channels==old)return true;
        if(save())return true;
        c.channels=old;return false;
    }
    return false;
}
bool Store::move_channel(const std::string &id, const std::string &channel_id, int direction) {
    for (auto &c : categories_) if (c.id==id) for (size_t i=0;i<c.channels.size();++i) if (channel_key(c.channels[i])==channel_key(channel_id)) {
        if ((direction<0 && i==0)||(direction>0 && i+1==c.channels.size())||direction==0) return false;
        std::swap(c.channels[i],c.channels[(size_t)((int)i+(direction<0?-1:1))]); return save();
    }
    return false;
}
}
