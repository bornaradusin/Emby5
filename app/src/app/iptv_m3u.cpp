/* Emby5 external M3U playlist support. SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/iptv_m3u.h"
#include "jf/jf_http.h"
#include "evo_data_path.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
namespace iptv_m3u {
namespace {
std::string path() { return evo_data_path("emby5/iptv-m3u.txt"); }
std::string strip(const std::string &s) {
    size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos?std::string():s.substr(a,b-a+1);
}
bool valid(const std::string &s) { return s.size()<4096 && (s.rfind("https://",0)==0 || s.rfind("http://",0)==0); }
std::string attribute(const std::string &line,const std::string &key) {
    const std::string mark=key+"="; size_t pos=line.find(mark);
    while(pos!=std::string::npos) {
        if(pos==0 || std::isspace((unsigned char)line[pos-1])) break;
        pos=line.find(mark,pos+1);
    }
    if(pos==std::string::npos) return {};
    pos+=mark.size(); if(pos>=line.size()) return {};
    if(line[pos]=='"' || line[pos]=='\'') {
        const char quote=line[pos++]; const size_t end=line.find(quote,pos);
        return line.substr(pos,end==std::string::npos?line.size()-pos:end-pos);
    }
    const size_t end=line.find_first_of(" \t,",pos);
    return line.substr(pos,end==std::string::npos?line.size()-pos:end-pos);
}
std::string origin(const std::string &url) {
    const auto start=url.find("://"); if(start==std::string::npos) return {};
    const auto end=url.find('/',start+3); return end==std::string::npos?url:url.substr(0,end);
}
std::string resolve(const std::string &base,const std::string &url) {
    if(valid(url)) return url;
    if(url.rfind("//",0)==0) return base.substr(0,base.find(':'))+":"+url;
    if(url.empty()) return {};
    if(url.front()=='/') return origin(base)+url;
    auto pos=base.find_last_of('/'); return pos==std::string::npos?url:base.substr(0,pos+1)+url;
}
}
std::string playlist_url() {
    FILE *f=std::fopen(path().c_str(),"rb"); if(!f) return {};
    char buf[4096]={}; const size_t n=std::fread(buf,1,sizeof(buf)-1,f); std::fclose(f);
    const std::string url=strip(std::string(buf,n)); return valid(url)?url:std::string();
}
bool save_playlist_url(const std::string &url) {
    const std::string value=strip(url);
    if(!value.empty()&&!valid(value)) return false;
    evo_mkdir(evo_data_dir()); evo_mkdir(evo_data_path("emby5"));
    const std::string tmp=path()+".tmp";
    FILE *f=std::fopen(tmp.c_str(),"wb"); if(!f) return false;
    bool ok=std::fwrite(value.data(),1,value.size(),f)==value.size();
    if(std::fclose(f)!=0) ok=false;
    if(ok) ok=std::rename(tmp.c_str(),path().c_str())==0;
    if(!ok) std::remove(tmp.c_str()); return ok;
}
bool load_catalog(const std::string &url,iptv_xtream::Catalog *out,std::string *error) {
    if(!out||!valid(url)) {if(error) *error="Invalid playlist URL";return false;}
    auto reply=jf::http_request("GET",url,{},"",30);
    if(!reply.ok()) {if(error) *error=reply.error.empty()?"HTTP "+std::to_string(reply.status):reply.error;return false;}
    if(reply.body.size()>32u*1024u*1024u) {if(error)*error="Playlist too large";return false;}
    std::istringstream stream(reply.body); std::string line,extinf;
    iptv_xtream::Catalog fresh; std::map<std::string,std::string> categories;
    bool first=true;
    while(std::getline(stream,line)) {
        if(!line.empty()&&line.back()=='\r') line.pop_back();
        line=strip(line); if(line.empty()) continue;
        if(first) {first=false; if(line.rfind("#EXTM3U",0)!=0) {if(error)*error="Not an extended M3U channel playlist";return false;}continue;}
        if(line.rfind("#EXTINF",0)==0) {extinf=line;continue;}
        if(line.front()=='#') continue;
        if(extinf.empty() || fresh.channels.size()>=20000) {extinf.clear();continue;}
        iptv_xtream::Channel ch;
        const auto comma=extinf.find_last_of(',');
        ch.name=strip(comma==std::string::npos?"":extinf.substr(comma+1));
        if(ch.name.empty()) ch.name=attribute(extinf,"tvg-name");
        ch.logo=resolve(url,attribute(extinf,"tvg-logo"));
        ch.category_id=attribute(extinf,"group-title");
        if(ch.category_id.empty()) ch.category_id="Other Channels";
        ch.extension=resolve(url,line); if (!valid(ch.extension)) { extinf.clear(); continue; } ch.id="m3u:"+std::to_string(fresh.channels.size()); ch.source="M3U";
        if(!ch.name.empty()&&!ch.extension.empty()) {
            if(!categories.count(ch.category_id)) {
                categories[ch.category_id]=ch.category_id;
                fresh.categories.push_back({ch.category_id,ch.category_id});
            }
            fresh.channels.push_back(std::move(ch));
        }
        extinf.clear();
    }
    if(fresh.channels.empty() && reply.body.find("#EXT-X-")!=std::string::npos) {
        // A media/variant HLS playlist is one channel, not a channel catalog.
        iptv_xtream::Channel ch;
        ch.id="m3u:0";ch.name="HLS Stream";ch.source="M3U";
        ch.category_id="Other Channels";ch.extension=url;
        fresh.categories.push_back({ch.category_id,ch.category_id});
        fresh.channels.push_back(std::move(ch));
    }
    if(fresh.channels.empty()) {if(error)*error="No playable channels found";return false;}
    *out=std::move(fresh);return true;
}
}
