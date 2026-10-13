/* Production group/cursor logic, no providers or player dependencies. */
#include "app/iptv_navigation.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace iptv_navigation;
static iptv_xtream::Channel ch(const std::string&id,const std::string&source="Xtream",const std::string &group="Provider"){
    iptv_xtream::Channel c;c.id=id;c.name="Channel "+id;c.source=source;c.category_id=group;return c;
}
static Selection at(){Selection s;assert(current(&s));return s;}
static bool move(int g,int c){auto s=at();input_ready(s.token,false);return request_move(s.token,s.channel.id,g,c);}
int main(){
    auto channels=std::vector<iptv_xtream::Channel>{ch("xtream:1"),ch("xtream:2"),ch("xtream:3"),ch("xtream:4"),ch("xtream:5"),ch("server:6","Emby"),ch("m3u:7","M3U")};
    std::vector<iptv_categories::Category> custom={
        {"f1","F1",{"1","2"}}, {"cro","Cro",{"xtream:3","4","4"}},
        {"empty","Empty",{"deleted"}}, {"srb","Srb",{"5","1"}}};
    auto groups=std::make_shared<const Groups>(build_groups(channels,custom));
    assert(groups->size()==6); // three custom + three separate provider rows
    assert((*groups)[0].title=="F1"&&(*groups)[1].title=="Cro"&&(*groups)[2].title=="Srb");
    assert((*groups)[1].channels.size()==2); // membership duplicates are ignored
    assert((*groups)[3].id!=(*groups)[4].id); // same provider group name, different source
    assert(begin(groups,1,1));assert(at().channel.id=="xtream:4");
    assert(move(-1,0));assert(at().channel.id=="xtream:4"); // not published until teardown
    assert(!move(0,1));assert(advance());assert(at().group_name=="F1"&&at().channel.id=="xtream:1");
    const auto token=at().token;
    assert(!input_ready(token,true)); // held input across reopened controller blocked
    assert(!request_move(token,at().channel.id,1,0));
    assert(input_ready(token,false));assert(move(1,0));assert(advance());
    assert(at().group_name=="Cro"&&at().channel.id=="xtream:3");
    assert(move(1,0));assert(advance());assert(at().group_name=="Srb"&&at().channel.id=="xtream:5");
    assert(!request_move(token,"xtream:1",0,1)); // old session cannot control current channel
    assert(move(0,1));assert(advance());assert(at().group_name=="Srb"&&at().channel.id=="xtream:1");
    assert(move(0,1));assert(advance());assert(at().group_name=="Srb"&&at().channel.id=="xtream:5");
    assert(move(0,-1));assert(advance());assert(at().group_name=="Srb"&&at().channel.id=="xtream:1");
    // Explicit empty groups from a caller are safely skipped, with wraparound.
    groups=std::make_shared<const Groups>(Groups{{"a","A",{ch("1")}}, {"b","B",{}}, {"c","C",{ch("2"),ch("3")}}});
    assert(begin(groups,0,0));assert(!move(0,1));assert(move(-1,0));assert(advance());assert(at().group_name=="C"&&at().channel.id=="2");
    assert(move(1,0));assert(advance());assert(at().group_name=="A");
    auto old=at();end();assert(!current(&old));assert(!request_move(old.token,old.channel.id,1,0));assert(!advance());
    assert(!begin({},0,0));assert(!begin(groups,1,0));assert(!begin(groups,0,1));
    assert(begin(groups,2,0));auto initial=at();assert(!request_move(initial.token,initial.channel.id,1,1));
    assert(!request_move(initial.token,initial.channel.id,0,2));assert(!request_move("","2",0,1));
    for(int i=0;i<1000;++i){assert(move(0,1));assert(advance());assert(at().group_id=="c");}
    assert(at().channel.id=="2");
    // Large catalogues: group creation uses indexed membership, not N*M scans.
    channels.clear();custom.clear();for(int i=0;i<50000;++i)channels.push_back(ch("xtream:"+std::to_string(i),"Xtream","G"+std::to_string(i/500)));
    custom.push_back({"large","Ordered",{"49999","0","25000"}});
    const auto large=build_groups(channels,custom);assert(large.size()==101);
    assert(large.front().channels[0].id=="xtream:49999"&&large.front().channels[1].id=="xtream:0");
    const auto search=build_groups(channels,custom,"49999");assert(search.size()==2 && search[0].channels.size()==1);
    end();std::cout<<"PASS: real Live TV groups/cursor, F1/Cro/Srb, first-channel group jumps, within-group wrap, empty/source/duplicate handling, pending and stale-token isolation, held-input release, 1000 switches and 50000 channels\n";
}
