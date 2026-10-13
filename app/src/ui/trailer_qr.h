/* Public library trailer QR, independent of Seerr. GPL-3.0-or-later */
#pragma once
#include "ui/screen.h"
#include "nuvio_input.h"
#include "qrcodegen.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace ui {
// Do not place authenticated playback URLs, LAN addresses or credentials in a QR.
inline bool public_trailer_url(const std::string &url) {
    if (url.empty() || url.size() > 1024) return false;
    const size_t first = url.rfind("https://", 0) == 0 ? 8 : url.rfind("http://", 0) == 0 ? 7 : 0;
    if (!first) return false;
    for (unsigned char ch : url) if (ch <= 32 || ch == 127 || ch == '\\') return false;
    const auto end = url.find_first_of("/?#", first);
    std::string host = url.substr(first, end == std::string::npos ? end : end-first);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (host.empty() || host.find_first_of("@:%[]") != std::string::npos || host.find('.') == std::string::npos)
        return false;
    if (host.back()=='.' || host.find("..")!=std::string::npos) return false;
    for (const char *suffix : {".local", ".localhost", ".lan", ".internal"}) {
        const std::string v(suffix);
        if (host.size() >= v.size() && host.compare(host.size()-v.size(),v.size(),v)==0) return false;
    }
    if (std::all_of(host.begin(),host.end(),[](unsigned char c) {return std::isdigit(c) || c=='.';})) return false;
    // Encoded query strings can conceal credential keys. Remote trailer links
    // need no encoded parameter names; reject them rather than leaking a token.
    const size_t q=url.find('?');
    if (q!=std::string::npos) {
        for (size_t at=q+1;at<url.size();) {
            size_t stop=url.find_first_of("&#",at); if (stop==std::string::npos) stop=url.size();
            const auto eq=url.find('=',at);
            std::string key=url.substr(at,(eq!=std::string::npos&&eq<stop?eq:stop)-at);
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return (char)std::tolower(c);});
            if (key.find('%')!=std::string::npos || key.find("token")!=std::string::npos ||
                key.find("key")!=std::string::npos || key.find("password")!=std::string::npos ||
                key=="username" || key=="auth" || key=="authorization") return false;
            if(stop==url.size()||url[stop]=='#')break;
            at=stop+1;
        }
    }
    return true;
}
inline std::string first_trailer_url(const std::vector<std::string> &links) {
    for (const auto &url:links) if (public_trailer_url(url)) return url;
    return {};
}
class TrailerQr {
    bool shown=false;
    std::string link;
    std::vector<uint8_t> code;
public:
    bool active() const { return shown; }
    bool open(const std::string &url) {
        if (!public_trailer_url(url)) return false;
        code.assign(qrcodegen_BUFFER_LEN_MAX,0);
        std::vector<uint8_t> work(qrcodegen_BUFFER_LEN_MAX);
        if (!qrcodegen_encodeText(url.c_str(),work.data(),code.data(),qrcodegen_Ecc_MEDIUM,
                qrcodegen_VERSION_MIN,qrcodegen_VERSION_MAX,qrcodegen_Mask_AUTO,true)) return false;
        link=url;shown=true;return true;
    }
    void input(uint32_t p) { if(p&NUVIO_BTN_CIRCLE) shown=false; }
    void draw() const {
        if(!shown)return;
        gfx::fill({0,0,gfx::W,gfx::H},0xd9000000u);
        const gfx::Rect panel{(gfx::W-760)/2,(gfx::H-770)/2,760,770};
        // Opaque panel: backdrop refraction must not affect QR contrast.
        gfx::fill(panel,0xff151b29u,24);
        gfx::text(gfx::W/2,panel.y+64,"Trailer",{gfx::Bold,34},0xffffffffu,1);
        gfx::text(gfx::W/2,panel.y+110,"Scan with your phone to watch the trailer",{gfx::Medium,23,700},0xffdedee5u,1);
        const int n=qrcodegen_getSize(code.data());
        const float px=std::floor(450.f/(n+8));
        const float side=px*(n+8),left=std::floor((gfx::W-side)/2),top=std::floor(panel.y+150);
        gfx::fill({left,top,side,side},0xffffffffu);
        for(int y=0;y<n;++y)for(int x=0;x<n;++x)
            if(qrcodegen_getModule(code.data(),x,y))gfx::fill({left+(x+4)*px,top+(y+4)*px,px,px},0xff000000u);
        gfx::text(gfx::W/2,panel.y+670,link,{gfx::Regular,20,680},0xffd0d0d8u,1);
        draw_pad_hint(panel.x+270,panel.y+730,PadButton::Circle,"Close",26,1.f);
    }
};
}
