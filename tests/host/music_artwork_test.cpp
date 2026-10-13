/* Execute actual Library/Album workers and draw state without tab-switch input. */
#include "music_ui_stubs.h"
#include "ui/album.h"
#include "jf/jf_http.h"
#include "nuvio_input.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
namespace {
std::mutex net_lock;
std::condition_variable net_cv;
const std::thread::id ui_thread=std::this_thread::get_id();
std::vector<std::string> urls;
std::unordered_map<std::string,std::string> details,tracks;
std::string albums,artists=R"({"Items":[{"Id":"artist1","Name":"Artist one","Type":"MusicArtist","ImageTags":{"Primary":"artist-tag"}}],"TotalRecordCount":1})";
std::string blocked_id;
bool blocked=false,release_block=false;
int active=0;
int count(const std::string &part){std::lock_guard<std::mutex> g(net_lock);int n=0;for(const auto &u:urls)n+=u.find(part)!=std::string::npos;return n;}
template<class F>void wait(F pred){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(7);while(!pred()){assert(std::chrono::steady_clock::now()<until);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
void release(){ {std::lock_guard<std::mutex> g(net_lock);release_block=true;}net_cv.notify_all(); }
std::string album_json(const std::string &id,const std::string &tag=""){
    return "{\"Id\":\""+id+"\",\"Name\":\""+id+"\",\"Type\":\"MusicAlbum\",\"ParentId\":\"music-folder\""+
        (tag.empty()?"":",\"ImageTags\":{\"Primary\":\""+tag+"\"}")+"}";
}
std::string track_json(const std::string &id,const std::string &tag){return "{\"Items\":[{\"Id\":\""+id+"\",\"Type\":\"Audio\",\"Name\":\"Track\",\"ImageTags\":{\"Primary\":\""+tag+"\"}}],\"TotalRecordCount\":1}";}
void fixture(){
    std::lock_guard<std::mutex> g(net_lock);assert(active==0);urls.clear();details.clear();tracks.clear();blocked_id.clear();blocked=false;release_block=false;
    albums="{\"Items\":[";
    for(int i=0;i<60;++i){const std::string id="album"+std::to_string(i);
        const std::string tag=i==1||i==2||i==4||i==25?"":i==3?"dead":"listed"+std::to_string(i);
        albums+=(i?",":"")+album_json(id,tag);details[id]=album_json(id,tag);tracks[id]="{\"Items\":[]}";
    }
    albums+="],\"TotalRecordCount\":60}";
    details["album1"]=album_json("album1","resolved1");details["album25"]=album_json("album25","resolved25");
    tracks["album2"]=track_json("track2","embedded2");tracks["album3"]=track_json("track3","embedded3");
}
}
namespace ui {
struct MusicRegressionAccess {
 static bool loaded(const Library &l){std::lock_guard<std::mutex> g(l.m_data->lock);return !l.m_data->loading&&l.m_data->total>=0;}
 static bool art_idle(const Library &l){std::lock_guard<std::mutex> g(l.m_data->lock);return !l.m_data->artwork_loading;}
 static unsigned attempts(const Library &l,const std::string &id){std::lock_guard<std::mutex> g(l.m_data->lock);auto it=l.m_data->artwork.find(id);return it==l.m_data->artwork.end()?0:it->second.attempts;}
 static void retry_now(Library &l){std::lock_guard<std::mutex> g(l.m_data->lock);for(auto &e:l.m_data->artwork)e.second.retry_at=music_art_now()-1;++l.m_data->revision;}
 static void focus(Library &l,int i){l.m_index=i;}
 static void source(Library &l,int i){l.switch_source(i);}
 static std::weak_ptr<void> lifetime(const Library &l){return l.m_data;}
 static bool loaded(const Album &l){std::lock_guard<std::mutex> g(l.m_data->lock);return l.m_data->loaded;}
 static size_t tracks(const Album &l){return l.m_tracks.size();}
};
}
namespace jf {
HttpResponse http_request(const std::string &method,const std::string &url,const std::vector<std::string>&,
                          const std::string&,int){
    assert(std::this_thread::get_id()!=ui_thread && "Network must not run in input or draw");
    assert(method=="GET");std::unique_lock<std::mutex> g(net_lock);urls.push_back(url);++active;
    std::string id;bool child=false;
    const auto items=url.find("/Items/");
    if(items!=std::string::npos)id=url.substr(items+7,url.find('?',items+7)-(items+7));
    else if(url.find("&parentId=album")!=std::string::npos){auto p=url.find("&parentId=")+10;id=url.substr(p,url.find('&',p)-p);child=true;}
    if(!child&&!blocked_id.empty()&&id==blocked_id&&!release_block){blocked=true;net_cv.notify_all();net_cv.wait(g,[]{return release_block;});}
    HttpResponse r;r.status=200;
    if(!id.empty()) {auto &map=child?tracks:details;auto it=map.find(id);if(it==map.end()){r.status=404;r.body="{}";}else r.body=it->second;}
    else if(url.find("/Artists/AlbumArtists?")!=std::string::npos)r.body=artists;
    else if(url.find("IncludeItemTypes=MusicAlbum")!=std::string::npos)r.body=albums;
    else {r.status=404;r.body="{}";}
    --active;return r;
}
}
namespace {
using A=ui::MusicRegressionAccess;
void draw(ui::Screen &s,double now=2){music_test::posters.clear();music_test::covers.clear();s.draw(now,.1f);}
bool displayed(const std::string &id,const std::string &owner,const std::string &tag){for(const auto &p:music_test::posters)if(p.id==id&&p.primary_owner==owner&&p.primary_tag==tag)return true;return false;}
void drive(ui::Library &l,const std::function<bool()> &done) {
    int frames=0;
    wait([&]{if(l.needs_redraw()||l.animating()) {draw(l,3+(++frames)*.1);}
             return done();});
}
void run(jf::ServerKind backend){
 fixture();music_test::failed_images.clear();
 jf::Client client("http://music.test:8096/proxy","dev","PS5",backend);client.set_session("secret","user","Listener");
 for(const std::string id:{"album1","album2","album4"}) music_test::failed_images.insert(client.image_url(id,"Primary","",800,true));
 music_test::failed_images.insert(client.image_url("album3","Primary","dead",800,true));
 ui::Library grid(client,"Music","MusicAlbum","music-folder");
 grid.set_sources({{"Albums","music-folder","MusicAlbum",""},{"Artists","music-folder","MusicArtist",""}});
 {std::lock_guard<std::mutex> g(net_lock);blocked_id="album1";}
 grid.activate();wait([&]{return A::loaded(grid);});assert(grid.needs_redraw());draw(grid);
 assert(displayed("album1","album1","") && "First draw must use album ID, not music-folder ParentId");
 for(const auto &url:music_test::covers) assert(url.find("/Items/music-folder/Images/")==std::string::npos);
 wait([]{std::lock_guard<std::mutex> g(net_lock);return blocked;});
 // Simulate the renderer settling: there is no need to switch tabs or press a button.
 for(int i=0;i<60;++i)draw(grid,3+i*.1);
 assert(!grid.needs_redraw());release();wait([&]{return grid.needs_redraw();});draw(grid);
 assert(displayed("album1","album1","resolved1"));
 drive(grid,[&]{return displayed("album2","track2","embedded2")&&displayed("album3","track3","embedded3")&&A::attempts(grid,"album4")==1&&A::art_idle(grid);});
 draw(grid);
 assert(displayed("album0","album0","listed0"));assert(displayed("album4","album4",""));
 assert(count("/Items/album0?")==0&&count("parentId=album0&")==0);
 assert(count("/Items/album25?")==0); // invisible albums were not scanned eagerly
 const int resolved_requests=count("/Items/album1?");
 grid.refresh_user_data();wait([&]{return A::loaded(grid);});assert(grid.needs_redraw());draw(grid);
 assert(displayed("album1","album1","resolved1")&&displayed("album2","track2","embedded2"));
 assert(count("/Items/album1?")==resolved_requests); // refresh did not erase metadata
 // Explicit new server tag supersedes a resolved old cover.
 {std::lock_guard<std::mutex> g(net_lock);const auto a=albums.find("\"album1\"");const auto end=albums.find('}',a);albums.insert(end,",\"ImageTags\":{\"Primary\":\"new-cover\"}");}
 grid.refresh_user_data();wait([&]{return A::loaded(grid);});draw(grid);assert(displayed("album1","album1","new-cover"));
 A::focus(grid,25);draw(grid);drive(grid,[&]{return displayed("album25","album25","resolved25")&&A::art_idle(grid);});
 // Available primary images are not replaced merely because tracks have artwork.
 jf::Item direct;direct.id="direct";direct.type="MusicAlbum";direct.primary_tag="album-cover";
 ui::MusicArtwork art;art.listing=ui::music_primary(direct);art.primary=art.listing;art.alternatives={{"track","track-cover",""}};
 assert(ui::music_display_item(client,direct,art,[](const auto&){return false;}).primary_owner=="direct");
 // A destroyed/old-source screen cannot receive a late result for another account.
 {std::lock_guard<std::mutex> g(net_lock);blocked_id="album1";blocked=false;release_block=false;}
 {std::lock_guard<std::mutex> g(net_lock);albums="{\"Items\":["+album_json("album1")+"],\"TotalRecordCount\":1}";}
 auto old=std::make_unique<ui::Library>(client,"Old account","MusicAlbum","music-folder");
 old->set_sources({{"Albums","music-folder","MusicAlbum",""},{"Artists","music-folder","MusicArtist",""}});
 old->activate();wait([&]{return A::loaded(*old);});draw(*old);wait([]{std::lock_guard<std::mutex> g(net_lock);return blocked;});
 A::source(*old,1);wait([&]{return A::loaded(*old);});draw(*old);assert(music_test::posters.size()==1&&music_test::posters.front().type=="MusicArtist");
 auto lifetime=A::lifetime(*old);old.reset();release();wait([&]{return lifetime.expired();});
 // Fresh account/server screen starts with its own cover map, even for equal IDs.
 jf::Client other("http://other.test:8096","dev","PS5",backend);other.set_session("different-secret","other-user","Other");
 {ui::Library next(other,"Other","MusicAlbum");next.activate();wait([&]{return A::loaded(next);});draw(next);
  assert(A::attempts(next,"album1")==0);wait([&]{return A::art_idle(next);});assert(next.needs_redraw());draw(next);
  for(const auto &url:music_test::covers)assert(url.find("http://other.test:8096")==0);}
 // Transient metadata failures retry only on the bounded schedule, not each frame.
 {std::lock_guard<std::mutex> g(net_lock);albums="{\"Items\":["+album_json("unavailable")+"],\"TotalRecordCount\":1}";}
 {ui::Library retry(client,"Retry","MusicAlbum");retry.activate();wait([&]{return A::loaded(retry);});draw(retry);
  wait([&]{return A::art_idle(retry);});draw(retry);assert(A::attempts(retry,"unavailable")==1);
  const int before=count("/Items/unavailable?");for(int i=0;i<40;++i)draw(retry);
  assert(count("/Items/unavailable?")==before);
  for(unsigned n=2;n<=3;++n){A::retry_now(retry);draw(retry);wait([&]{return A::art_idle(retry);});draw(retry);assert(A::attempts(retry,"unavailable")==n);}
  A::retry_now(retry);draw(retry);assert(A::attempts(retry,"unavailable")==3);
  {std::lock_guard<std::mutex> g(net_lock);details["unavailable"]=album_json("unavailable","recovered");}
  retry.activate();draw(retry);wait([&]{return A::art_idle(retry);});draw(retry);
  assert(displayed("unavailable","unavailable","recovered"));
 }
 // Actual album-detail refresh notices late cover and track updates as well.
 {std::lock_guard<std::mutex> g(net_lock);blocked_id="album2";blocked=false;release_block=false;}
 jf::Item album;album.id="album2";album.type="MusicAlbum";album.name="Album two";
 ui::Album detail(client,album);detail.activate();draw(detail);
 wait([]{std::lock_guard<std::mutex> g(net_lock);return blocked;});
 for(int i=0;i<60;++i)draw(detail);assert(!detail.needs_redraw());release();
 wait([&]{return A::loaded(detail);});assert(detail.needs_redraw());draw(detail);assert(A::tracks(detail)==1);
 assert(music_test::covers.front().find("/Items/track2/Images/Primary")!=std::string::npos);
 {std::lock_guard<std::mutex> g(net_lock);tracks["album2"]="{\"Items\":[]}";}
 detail.refresh_user_data();wait([&]{return detail.needs_redraw();});
 wait([&]{if(detail.needs_redraw())draw(detail);return A::tracks(detail)==0;});
}
}
int main(){run(jf::ServerKind::Emby);run(jf::ServerKind::Jellyfin);std::cout<<"PASS: music covers publish without input, visible-first metadata, album/track fallback, refresh retention/new tags, missing art, source/account isolation and album detail (Emby and Jellyfin)\n";}
