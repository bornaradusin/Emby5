/* Actual Live TV browser -> shared playback route, using real saved groups. */
#include "ui/iptv.h"
#include "nuvio_input.h"
#include "app/settings.h"
#include "platform/ime.h"
#include "jf/jf_http.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <unistd.h>
static std::string root;
static iptv_live::Snapshot cache;
static std::function<void(const std::string&)> keyboard;
namespace settings {Appearance get_appearance(){return {};}}
extern "C" {
const char *evo_data_dir(){return root.c_str();}
const char *evo_data_path(const char *leaf){static thread_local std::string s;s=root+"/"+leaf;return s.c_str();}
int evo_mkdir(const char *p){std::filesystem::create_directories(p);return 0;}
}
namespace jf {HttpResponse http_request(const std::string&,const std::string&,const std::vector<std::string>&,const std::string&,int){std::abort();}}
namespace iptv_xtream {bool load_credentials(Credentials *c){*c={"https://local.test","user","test"};return true;}}
namespace iptv_live {
Snapshot summary(){auto s=cache;s.channels.clear();return s;}Snapshot snapshot(){return cache;}
void refresh(jf::Client*,bool,bool){}void request_guide(jf::Client*,const iptv_xtream::Channel&){}
}
namespace ime {bool active(){return false;}void request(Kind,const std::string&,const std::string&,std::function<void(const std::string&)> f){keyboard=std::move(f);}}
namespace gfx {float text(float,float,const std::string&,const TextStyle&,uint32_t,int){return 0;}void image(const Rect&,const Texture*,float,float,bool){}}
namespace art {const gfx::Texture *get(const std::string&,int,int){return nullptr;}}
namespace ui {void glass_panel(const gfx::Rect&,float,float,bool,float){}}
int main(){
 char p[]="/tmp/emby5-live-browse-XXXXXX";assert(mkdtemp(p));root=p;
 iptv_categories::Store store;assert(store.load("https://local.test|user"));
 assert(store.create("F1")&&store.create("Cro")&&store.create("Srb"));
 const auto ids=store.categories();
 for(int i=0;i<6;++i){iptv_xtream::Channel c;c.id="xtream:"+std::to_string(i);c.name="Channel "+std::to_string(i);c.category_id="Provider";c.category_key="prov1";cache.channels.push_back(c);
   assert(store.assign(ids[i/2].id,std::to_string(i),true));}
 cache.generation=1;
 jf::Client client("https://local.test","device","PS5");client.set_session("token","user","User");
 ui::IPTV browser(client);browser.activate();browser.input(NUVIO_BTN_DOWN);browser.input(NUVIO_BTN_RIGHT);
 assert(browser.input(NUVIO_BTN_CROSS).kind==ui::Action::PlayLiveTV);
 iptv_navigation::Selection sel;assert(iptv_navigation::current(&sel));assert(sel.group_name=="Cro"&&sel.channel.id=="xtream:3");
 assert(iptv_navigation::request_move(sel.token,sel.channel.id,-1,0));assert(iptv_navigation::advance());
 assert(iptv_navigation::current(&sel));assert(sel.group_name=="F1"&&sel.channel.id=="xtream:0");
 browser.focus_channel(sel.group_id,sel.channel.id);assert(browser.input(NUVIO_BTN_CROSS).kind==ui::Action::PlayLiveTV);
 assert(iptv_navigation::current(&sel)&&sel.group_name=="F1");iptv_navigation::end();
 // Search starts from the match but zapping uses its whole group, not only matches.
 browser.input(NUVIO_BTN_TRIANGLE);assert(keyboard);keyboard("Channel 3");
 assert(browser.input(NUVIO_BTN_CROSS).kind==ui::Action::PlayLiveTV);
 assert(iptv_navigation::current(&sel)&&sel.group_name=="Cro"&&sel.channel.id=="xtream:3");
 assert(iptv_navigation::request_move(sel.token,sel.channel.id,0,-1));assert(iptv_navigation::advance());
 assert(iptv_navigation::current(&sel)&&sel.channel.id=="xtream:2");
 browser.input(NUVIO_BTN_CIRCLE);browser.focus_channel(sel.group_id,sel.channel.id);
 // A catalogue refresh with the same stable group key does not move to another provider.
 cache.generation++;browser.input(0);assert(browser.input(NUVIO_BTN_CROSS).kind==ui::Action::PlayLiveTV);
 assert(iptv_navigation::current(&sel)&&sel.channel.id=="xtream:2");
 iptv_navigation::end();std::filesystem::remove_all(root);
 std::cout<<"PASS: actual IPTV browser uses saved F1/Cro/Srb groups/order, returns focus to played channel, search launches full group, refresh retains stable identity\n";
}
