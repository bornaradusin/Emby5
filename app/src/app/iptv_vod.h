#pragma once
#include "app/iptv_xtream.h"
#include <cstdint>
#include <string>
#include <vector>
namespace iptv_vod {
struct Item { std::string id,name,poster,group,url,extension,source; bool series=false; std::string category_id; };
struct Episode { std::string id,name,url,extension; int season=0,number=0; };
struct Snapshot { size_t movie_count=0,show_count=0,m3u_count=0; bool loading=false; uint64_t generation=0; std::string error; size_t categories_done=0,categories_total=0;
    iptv_catalogue::State movies, shows, playlist;
};
struct Group { std::string name; size_t count=0; };
Snapshot snapshot();
std::vector<Group> groups(bool series,const std::string &search="");
std::vector<Item> page(bool series,const std::string &group,size_t first,size_t limit,const std::string &search="");
void refresh(bool force=false, bool retry_failed=false);
int refresh_interval_hours();
void set_refresh_interval_hours(int hours);
std::vector<Episode> episodes(const Item &series, std::string *error=nullptr);
}
