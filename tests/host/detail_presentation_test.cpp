/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui/overview_scroll.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace ui::overview;
static size_t length(const std::string &s) { size_t n=0; for(size_t i=0;i<s.size();i=next(s,i))++n;return n; }
static std::string letters(const std::string &s) {
    std::string out; for(size_t i=0;i<s.size();) {size_t n=next(s,i);unsigned c=codepoint(s,i,n);
        if(c!=' ' && c!='\t' && c!='\r' && c!='\n' && c!=0xa0)out+=s.substr(i,n-i);i=n;}return out;
}
int main() {
    auto measure=[](const std::string &s) {return float(length(s)*10);};
    const std::string samples[]={"One two three four five six seven eight.",
        u8"\u4eca\u5929\u662f\u661f\u671f\u4e00\uff0c\u6211\u4eec\u770b\u7535\u5f71\u548c\u7535\u89c6\u5267\u3002",
        u8"\u041e\u0432\u043e \u0458\u0435 \u043e\u043f\u0438\u0441 \u0444\u0438\u043b\u043c\u0430. \u010c\u0107\u017e\u0111\u0161.",
        "First\r\nSecond\nThird\tword",std::string(5000,'x'),"end "};
    for(const auto &text:samples) {
        const auto lines=wrap(text,80,measure);std::string joined;
        for(const auto &line:lines) {assert(measure(line)<=80);joined+=line;}
        assert(letters(joined)==letters(text));
    }
    assert(wrap("",80,measure).empty());
    assert(wrap("short",80,measure).size()==1);
    assert(wrap("  one  two  ",0,measure).size()==1);
    const auto joined=wrap("   \n\t",80,measure); assert(joined.empty());
    std::string invalid="a";invalid.push_back(char(0xf0));invalid+='z';
    assert(!wrap(invalid,10,measure).empty());
    Scroll s;
    for(int i=0;i<30;++i) {assert(s.step(.1,6,true));assert(s.offset()==0);}
    for(int i=0;i<30;++i) assert(s.step(.1,6,true));
    assert(s.offset()>30 && s.offset()<40);
    const float paused=s.offset();for(int i=0;i<100;++i)assert(!s.step(.1,6,false));assert(s.offset()==paused);
    s.step(std::numeric_limits<double>::quiet_NaN(),6,true);assert(s.offset()==paused);
    s.step(-1,6,true);assert(s.offset()==paused);
    for(int i=0;i<60;++i)s.step(.1,6,true);
    assert(std::fabs(s.offset()-3*37.7f)<.01f);
    for(int i=0;i<40;++i)s.step(.1,6,true);assert(s.offset()==0);
    assert(!s.step(.1,3,true) && s.offset()==0);
    Scroll slow,normal,off;
    for(int i=0;i<100;++i){slow.step(.1,100,true,37.7f,3,9);normal.step(.1,100,true,37.7f,3,18);off.step(.1,100,true,37.7f,3,0);}
    assert(slow.offset()>0 && std::fabs(normal.offset()-slow.offset()*2)<.01f && off.offset()==0);
    normal.step(.1,100,true,37.7f,3,0);assert(normal.offset()==0);
    std::cout<<"PASS: complete long/UTF-8/CJK synopsis wrapping, no truncation, start/end pauses, scroll loop, focus pause and Off/Slow/Normal speeds\n";
}
