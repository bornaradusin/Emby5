/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
namespace emby5 {
// Opaque identity label, not an authentication token or cryptographic proof.
inline std::string identity_key(const std::string &s){
    uint64_t a=1469598103934665603ULL,b=1099511628211ULL;
    for(unsigned char c:s){a=(a^c)*1099511628211ULL;b=(b+c+1)*0x9e3779b185ebca87ULL;}
    char out[40];std::snprintf(out,sizeof out,"%016llx%016llx",(unsigned long long)a,(unsigned long long)b);return out;
}
inline std::string playlist_item_key(const std::string &url){return "playlist:"+identity_key(url);}
}
