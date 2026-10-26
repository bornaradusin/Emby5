#pragma once
#include "app/iptv_xtream.h"
#include <string>
namespace iptv_m3u {
std::string playlist_url();
bool save_playlist_url(const std::string &url);
bool load_catalog(const std::string &url, iptv_xtream::Catalog *out, std::string *error);
/* The M3U channels' TV guide (XMLTV): the address entered in Settings, else
 * the one the playlist names (url-tvg), else empty. */
std::string guide_url();
std::string user_guide_url();      /* Settings › Live TV, may be empty */
std::string playlist_guide_url();  /* what the playlist last named, may be empty */
bool save_guide_url(const std::string &url);   /* empty: use the playlist's */
}
