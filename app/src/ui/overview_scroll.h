/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ui {
namespace overview {
// Advance by a complete UTF-8 code point. Invalid/truncated input still advances.
inline size_t next(const std::string &s, size_t i) {
    if (i >= s.size()) return s.size();
    size_t n = (static_cast<unsigned char>(s[i]) < 0x80) ? 1 :
        ((static_cast<unsigned char>(s[i]) & 0xe0) == 0xc0) ? 2 :
        ((static_cast<unsigned char>(s[i]) & 0xf0) == 0xe0) ? 3 :
        ((static_cast<unsigned char>(s[i]) & 0xf8) == 0xf0) ? 4 : 1;
    for (size_t k = 1; k < n; ++k)
        if (i + k >= s.size() || (static_cast<unsigned char>(s[i+k]) & 0xc0) != 0x80) return i + 1;
    return i + n;
}
inline unsigned codepoint(const std::string &s, size_t a, size_t b) {
    unsigned c = static_cast<unsigned char>(s[a]);
    if (b-a > 1) { c &= (1u << (7-(b-a))) - 1; for (size_t i=a+1;i<b;++i) c=(c<<6)|(static_cast<unsigned char>(s[i])&63); }
    return c;
}
inline bool cjk(unsigned c) {
    return (c >= 0x2e80 && c <= 0x9fff) || (c >= 0xac00 && c <= 0xd7af) ||
           (c >= 0xf900 && c <= 0xfaff) || (c >= 0x20000 && c <= 0x323af);
}
// Pure layout; measure is called only when the text changes, never every frame.
// Draw single wrapped lines so a long synopsis cannot allocate a giant GPU texture.
template<class Measure>
std::vector<std::string> wrap(const std::string &text, float width, Measure measure) {
    std::vector<std::string> lines;
    std::string line, word;
    bool space = false;
    auto flush = [&] { if (!line.empty()) { lines.push_back(line); line.clear(); } };
    auto put = [&](const std::string &token) {
        if (token.empty()) return;
        const std::string joined = line + (space && !line.empty() ? " " : "") + token;
        if (width <= 0 || measure(joined) <= width) line = joined;
        else {
            flush();
            // Split an overlong word safely at UTF-8 boundaries, without losing it.
            for (size_t i=0;i<token.size();) {
                const size_t n=next(token,i);
                const auto piece=token.substr(i,n-i);
                if (!line.empty() && measure(line+piece)>width) flush();
                line += piece; i=n;
            }
        }
        space=false;
    };
    auto put_word = [&] { put(word); word.clear(); };
    for (size_t i=0;i<text.size();) {
        const size_t n=next(text,i);
        const unsigned c=codepoint(text,i,n);
        if (c=='\r' || c=='\n') {
            put_word(); flush(); space=false;
            if (c=='\r' && n<text.size() && text[n]=='\n') i=n+1; else i=n;
            continue;
        }
        if (c==' ' || c=='\t' || c==0xa0) { put_word(); space=true; }
        else if (cjk(c)) { put_word(); put(text.substr(i,n-i)); }
        else word += text.substr(i,n-i);
        i=n;
    }
    put_word(); flush();
    return lines;
}

class Scroll {
public:
    void reset() { elapsed=0; position=0; }
    float offset() const { return position; }
    bool step(double dt, size_t count, bool reading, float line_height=37.7f, size_t visible=3, double pixels_per_second=18.0) {
        if (!std::isfinite(pixels_per_second) || pixels_per_second<=0) {reset();return false;}
        if (count<=visible) { reset(); return false; }
        if (!reading) return false; // pause in episode rows and while a sheet is open
        if (std::isfinite(dt) && dt>0) elapsed += std::min(dt,0.1);
        const double end=(count-visible)*line_height;
        constexpr double delay=4.0, end_delay=5.0;
        const double speed=pixels_per_second;
        const double cycle=delay+end/speed+end_delay;
        elapsed=std::fmod(elapsed,cycle);
        position=static_cast<float>(std::max(0.0,std::min(end,(elapsed-delay)*speed)));
        return true; // keep the idle renderer ticking even during the reading pauses
    }
private:
    double elapsed=0;
    float position=0;
};
}
}
