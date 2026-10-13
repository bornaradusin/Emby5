/* Actual Detail page/client regressions. HTTP and graphics are test doubles. */
#include "ui/detail.h"
#include "ui/detail_layout.h"
#include "detail_draw_trace.h"
#include "app/settings.h"
#include "app/io_worker.h"
#include "app/user_data_update.h"
#include "jf/jf_http.h"
#include "nuvio_input.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
std::mutex server;
bool seen=true, favorite=false;
int item_status=200, season_status=200, episode_status=200, resume_status=500, next_status=500, upcoming_status=200;
bool no_episodes=false, no_seasons=false, no_upcoming=false, wrong_id=false;
std::atomic<int> active{0}, peak{0}, mutation_count{0};
std::string episode_json=R"({"Id":"ep1","Type":"Episode","Name":"First episode","SeriesId":"series1","SeasonId":"season1","ParentIndexNumber":1,"IndexNumber":1,"RunTimeTicks":6000000000,"UserData":{"Played":false,"PlaybackPositionTicks":0}})";
std::string current_item(const std::string &id) {
    return std::string("{\"Id\":\"")+(wrong_id?"wrong":id)+"\",\"Type\":\""+(id=="series1"?"Series":"Movie")+
        "\",\"Name\":\"Test\",\"Overview\":\"A complete description for testing the detail screen.\",\"UserData\":{\"Played\":"+
        (seen?"true":"false")+",\"IsFavorite\":"+(favorite?"true":"false")+",\"PlaybackPositionTicks\":0}}";
}
}
namespace jf {
HttpResponse http_request(const std::string &method,const std::string &url,
        const std::vector<std::string> &,const std::string &,int) {
    const int now=++active;
    int prev=peak.load(); while(now>prev && !peak.compare_exchange_weak(prev,now)){}
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    --active;
    std::lock_guard<std::mutex> lock(server);
    HttpResponse r; r.status=200;
    if (method=="POST" || method=="DELETE") {
        ++mutation_count;
        if(url.find("PlayedItems")!=std::string::npos) seen=method=="POST";
        else if(url.find("FavoriteItems")!=std::string::npos) favorite=method=="POST";
        else assert(false);
        r.status=204; return r;
    }
    if(url.find("/Seasons?")!=std::string::npos) {
        r.status=season_status;
        r.body=no_seasons?R"({"Items":[]})":R"({"Items":[{"Id":"season1","Type":"Season","Name":"Season 1","IndexNumber":1}]})";
    } else if(url.find("/Episodes?")!=std::string::npos) {
        r.status=episode_status; r.body=no_episodes?R"({"Items":[]})":"{\"Items\":["+episode_json+"]}";
    } else if(url.find("/Upcoming?")!=std::string::npos) {
        r.status=upcoming_status;
        r.body=no_upcoming?R"({"Items":[]})":R"({"Items":[{"Id":"up1","Type":"Episode","Name":"Upcoming","SeriesId":"series1","SeasonId":"season2","ParentIndexNumber":2,"IndexNumber":1,"PremiereDate":"2099-01-01T00:00:00Z"}]})";
    } else if(url.find("/Resume?")!=std::string::npos) {r.status=resume_status;r.body=R"({"Items":[]})";}
    else if(url.find("/NextUp?")!=std::string::npos) {r.status=next_status;r.body=R"({"Items":[]})";}
    else if(url.find("/Similar?")!=std::string::npos) {r.status=404;}
    else if(url.find("/Items/series1?")!=std::string::npos) {r.status=item_status; r.body=current_item("series1");}
    else if(url.find("/Items/movie1?")!=std::string::npos) {r.status=item_status; r.body=current_item("movie1");}
    else {std::cerr<<"Unexpected HTTP request "<<method<<" "<<url<<"\n"; assert(false);}
    return r;
}
}
namespace ui {
struct DetailRegressionAccess {
    static Detail::Content fetch(jf::Client &c, const jf::Item &item) {return Detail::fetch(c,item);}
    static void merge(Detail::Content &c, Detail::Content fresh) {Detail::merge_content(c,std::move(fresh));}
    static void rebuild(Detail::Content &c) {Detail::rebuild_content(c);}
    static void seed(Detail &p,const Detail::Content &c) {p.m_view=c;p.m_data->c=c;++p.m_data->revision;p.select_episodes();}
    static auto episodes(const Detail &p) {return p.m_eps;}
    static Action watched(Detail &p) {p.m_zone=Detail::Buttons;p.m_button_id=Detail::WatchedButton; return p.input(NUVIO_BTN_CROSS);}
    static Action fav(Detail &p) {p.m_zone=Detail::Buttons;p.m_button_id=Detail::FavouriteButton; return p.input(NUVIO_BTN_CROSS);}
    static bool hidden(Detail &p,const jf::Item &i){return p.spoiler_hidden(i);}
    static void episode_focus(Detail &p){p.m_zone=Detail::Episodes;p.m_episode=0;}
    static bool reading(const Detail &p){return p.m_read_more.active();}
    static int reader_line(const Detail &p){return p.m_read_more.first_line();}
    static bool loading(Detail &p) {std::lock_guard<std::mutex> l(p.m_data->lock);return p.m_data->loading;}
    static bool again(Detail &p) {std::lock_guard<std::mutex> l(p.m_data->lock);return p.m_data->refresh_again;}
    static void season_focus(Detail &p,int n=0) {p.m_zone=Detail::Seasons;p.m_season=n;p.m_season_picked=true;p.season_moved();}
    static int zone(const Detail &p){return p.m_zone;}
    static int season(const Detail &p){return p.m_season;}
    static int episode(const Detail &p){return p.m_episode;}
    static int first(const Detail &p){return p.m_episode_first;}
    static float page(const Detail &p){return p.m_page.value;}
    static float season_top(const Detail &p){return p.zone_top(Detail::Seasons);}
    static void refresh_selection(Detail &p,const Detail::Content &c) {p.m_view=c;p.select_episodes();}
    static auto data(Detail &p) {std::lock_guard<std::mutex> l(p.m_data->lock);return p.m_data;}
};
}
using Access=ui::DetailRegressionAccess;
extern settings::Appearance detail_test_appearance;
template<class Fn> auto on_io(Fn fn) {
    using T=decltype(fn());
    auto promise=std::make_shared<std::promise<T>>(); auto result=promise->get_future();
    assert(emby5::spawn_io([fn,promise] {promise->set_value(fn());},[promise]{promise->set_exception(std::current_exception());}));
    return result.get();
}
int main() {
    jf::Client client("http://server:8096","test","PS5"); client.set_session("token","user","user");
    jf::Item series;series.id="series1";series.type="Series";series.name="Test";
    ui::Detail::Content view;view.item=series;
    auto fresh=on_io([&]{return Access::fetch(client,series);});
    assert(fresh.have_detail && fresh.episodes_ok && fresh.seasons_ok && !fresh.next_ok && !fresh.resume_ok);
    Access::merge(view,std::move(fresh));
    assert(view.all_episodes.size()==2 && view.seasons.size()==2 && view.have_target && view.target.id=="ep1");
    assert(view.all_episodes.back().upcoming && view.seasons.back().type=="EpisodeGroup");
    assert(peak<=3);
    // Metadata failure does not discard a usable episodes response on initial load.
    item_status=500; season_status=500;
    ui::Detail::Content partial; partial.item=series;
    Access::merge(partial,on_io([&]{return Access::fetch(client,series);}));
    assert(!partial.have_detail && partial.all_episodes.size()==2 && partial.seasons.size()==2);
    // Failed optional endpoints leave previous successful rows, not an empty page.
    episode_status=503; upcoming_status=404;
    Access::merge(view,on_io([&]{return Access::fetch(client,series);}));
    assert(view.have_detail && view.all_episodes.size()==2 && view.have_target);
    // A successful empty server response really clears the rows.
    episode_status=200;season_status=200;upcoming_status=200;
    no_seasons=no_episodes=no_upcoming=true;
    Access::merge(view,on_io([&]{return Access::fetch(client,series);}));
    assert(view.all_episodes.empty() && view.seasons.empty() && !view.have_target);
    item_status=200; no_seasons=no_episodes=no_upcoming=false;
    Access::merge(view,on_io([&]{return Access::fetch(client,series);}));
    // A virtual episode must not duplicate an available episode of the same season/number.
    auto duplicate=view.upcoming_episodes.front();duplicate.id="ep1";
    view.upcoming_episodes.push_back(duplicate); Access::rebuild(view);
    assert(view.all_episodes.size()==2);
    // Actual button actions, actual client mutation and reconciliation, repeated.
    for (const char *kind:{"Series","Movie"}) {
        auto item=series;item.type=kind;item.id=std::string(kind)=="Series"?"series1":"movie1";
        ui::Detail page(client,item);
        ui::Detail::Content c;c.item=item;
        Access::merge(c,on_io([&]{return Access::fetch(client,item);}));Access::seed(page,c);
        for(int i=0;i<100;++i) {
            const auto action=i%2 ? Access::fav(page) : Access::watched(page);
            assert(action.kind==ui::Action::Changed && action.change.id==item.id);
            emby5::UserDataRequest req;req.id=action.change.id;
            req.played_set=action.change.played_set;req.played=action.change.played;
            req.favorite_set=action.change.favorite_set;req.favorite=action.change.favorite;
            const auto result=on_io([&]{return emby5::update_user_data(client,req);});
            assert(result.accepted && result.refreshed && result.matched);
            page.reconcile_user_data(result.item);
            assert(page.item().played==seen && page.item().favorite==favorite);
            page.draw(10+i*.016f,.016f);
        }
        // UI page ownership can disappear while its fetch finishes.
        auto dying=std::make_unique<ui::Detail>(client,item); dying->activate();
        auto data=Access::data(*dying);dying.reset();
        for(int i=0;i<2000;++i) {bool pending;{std::lock_guard<std::mutex> l(data->lock); pending=data->loading;}
            if(!pending)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        {std::lock_guard<std::mutex> l(data->lock); assert(!data->loading);}
    }
    // Coalesce repeated invalidations: one fetch plus one deferred repeat, not N threads.
    ui::Detail page(client,series);page.activate();page.activate();page.activate();
    assert(Access::again(page));
    for(int i=0;i<2000 && Access::loading(page);++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    page.draw(20,.016f);
    for(int i=0;i<2000 && Access::loading(page);++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    page.draw(21,.016f);assert(!Access::again(page));assert(!Access::episodes(page).empty());
    // Refuse a mismatched server item, not another title's status.
    wrong_id=true; jf::Item wrong;assert(!client.item("movie1",&wrong));wrong_id=false;
    assert(mutation_count==200);
    // Real Detail input must reveal spoilers locally, never through a server write.
    detail_test_appearance.spoiler_protection=true;
    detail_test_appearance.description_scroll=0;
    ui::Detail protected_page(client,series);
    ui::Detail::Content protected_content;protected_content.item=series;
    Access::merge(protected_content,on_io([&]{return Access::fetch(client,series);}));
    for(auto &episode:protected_content.all_episodes) {
        episode.played=false;episode.overview="";
        for(int n=0;n<80;++n)episode.overview+="Line "+std::to_string(n)+" of a long episode description.\n";
    }
    Access::seed(protected_page,protected_content);
    Access::episode_focus(protected_page);
    auto ep=Access::episodes(protected_page).front();assert(Access::hidden(protected_page,ep));
    auto reveal=protected_page.input(NUVIO_BTN_SQUARE);
    assert(reveal.kind==ui::Action::None && !Access::hidden(protected_page,ep) && !protected_page.modal());
    assert(!Access::episodes(protected_page).front().played && mutation_count==200);
    auto read=protected_page.input(NUVIO_BTN_SQUARE);
    assert(read.kind==ui::Action::None && Access::reading(protected_page) && protected_page.modal());
    protected_page.draw(30,.016f);
    for(int n=0;n<8;++n)protected_page.input(NUVIO_BTN_RIGHT);
    assert(Access::reader_line(protected_page)>0);
    assert(protected_page.input(NUVIO_BTN_CROSS).kind==ui::Action::None && mutation_count==200);
    protected_page.input(NUVIO_BTN_CIRCLE);assert(!protected_page.modal());
    assert(mutation_count==200);detail_test_appearance={};
    // Actual detail screen: one season control per label, not a parent track
    // plus another drop. Thumbnail windows have no clipped previous-card slivers.
    int draws=0;
    for(int scale:{90,100,110,120})for(int size:{0,1,2}) {
        detail_test_appearance.ui_text_percent=scale;detail_test_appearance.poster_size=size;
        detail_test_appearance.reduced_motion=true;
        ui::Detail::Content fixture;fixture.item=series;fixture.have_detail=true;
        for(int season=0;season<12;++season){jf::Item row;row.type="Season";row.id="season-"+std::to_string(season);
            row.index=season;row.name=season==0?"Specials":season==11?std::string(200,'L'):"Season "+std::to_string(season);
            fixture.seasons.push_back(row);
            for(int e=1;e<=5;++e){jf::Item episode;episode.id=row.id+"-"+std::to_string(e);
                episode.type="Episode";episode.season_id=row.id;episode.parent_index=season;episode.index=e;
                episode.name="Episode "+std::to_string(e);episode.overview="Description with enough text to wrap over multiple lines safely.";
                fixture.all_episodes.push_back(episode);}
        }
        ui::Detail layout_page(client,series);Access::seed(layout_page,fixture);layout_page.draw(40,1.f);
        const auto render=[&] {detail_trace::clear();layout_page.draw(41,1.f);assert(detail_trace::scissor_depth==0);++draws;};
        Access::season_focus(layout_page);
        for(int season=0;season<12;++season){
            if(season)layout_page.input(NUVIO_BTN_RIGHT);
            render();assert(Access::season(layout_page)==season);
            // Exactly one focused season control. Its whole boundary is inside
            // the logical content viewport, even with long labels and 120% text.
            assert(detail_trace::focused_controls.size()==1);
            const auto focus=detail_trace::focused_controls[0];
            assert(focus.x>=ui::kPad+9&&focus.x+focus.w<=gfx::W-ui::kPad-9);
            assert(focus.y>0&&focus.y+focus.h<gfx::H);
        }
        assert(layout_page.input(NUVIO_BTN_CROSS).kind==ui::Action::None);
        assert(Access::zone(layout_page)==2); // season activation opens its row, never plays immediately
        const ui::detail_layout::Episodes geometry(gfx::W-2*ui::kPad,scale/100.f,ui::theme_card_scale());
        for(int episode=0;episode<5;++episode){
            if(episode)layout_page.input(NUVIO_BTN_RIGHT);render();
            assert(Access::episode(layout_page)==episode);
            assert(Access::first(layout_page)<=episode&&episode<Access::first(layout_page)+geometry.capacity);
            assert(detail_trace::focused_controls.empty()&&detail_trace::focused_images.size()==1);
            assert(Access::season_top(layout_page)-Access::page(layout_page)>=119.f);
            auto play=layout_page.input(NUVIO_BTN_CROSS);
            assert(play.kind==ui::Action::Play&&play.item.id=="season-11-"+std::to_string(episode+1));
        }
        // Refreshing data must retain the selected episode ID, not its old index.
        auto changed=fixture;jf::Item inserted=fixture.all_episodes.back();inserted.index=0;inserted.id="inserted";
        changed.all_episodes.insert(changed.all_episodes.begin(),inserted);
        Access::refresh_selection(layout_page,changed);
        assert(Access::episodes(layout_page)[Access::episode(layout_page)].id=="season-11-5");
        // Up returns to the same selected season; Left changes seasons without playing.
        layout_page.input(NUVIO_BTN_UP);assert(Access::zone(layout_page)==1);
        assert(layout_page.input(NUVIO_BTN_LEFT).kind==ui::Action::None&&Access::season(layout_page)==10);
        assert(Access::episodes(layout_page).front().season_id=="season-10");
        auto empty=fixture;empty.all_episodes.clear();Access::refresh_selection(layout_page,empty);render();
        bool empty_hint=false;for(const auto &t:detail_trace::texts)if(t.value=="No episodes available for this season.")empty_hint=true;
        assert(empty_hint&&layout_page.input(NUVIO_BTN_CROSS).kind==ui::Action::None);
    }
    detail_test_appearance={};assert(mutation_count==200);
    std::cout<<"PASS: actual seasons/episode viewport, focus ownership, all text/card sizes, long season labels, selection retention, empty seasons ("<<draws<<" renders)\n";
    std::cout<<"PASS: actual detail partial-failure/empty-list loading, available+upcoming groups, 200 watched/favorite toggles, destroyed pages, refresh coalescing, non-mutating spoiler reveal and full manual synopsis\n";
}
