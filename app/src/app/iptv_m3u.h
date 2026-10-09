#pragma once
#include "app/iptv_xtream.h"
#include <string>
namespace iptv_m3u {
std::string playlist_url();
bool save_playlist_url(const std::string &url);
bool load_catalog(const std::string &url, iptv_xtream::Catalog *out, std::string *error);
}
