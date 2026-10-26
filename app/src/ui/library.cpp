/*
 * Emby5 — Emby for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/library.h"
#include "ui/library_layout.h"
#include "app/user_data_update.h"
#include "app/i18n.h"
#include "app/spawn.h"
#include "app/io_worker.h"

#include "gfx/art.h"
#include "nuvio_input.h"

#include <algorithm>
#include <set>
#include <cctype>
#include <cstdio>
#include <thread>

namespace ui {
namespace {


constexpr float kGridTop = 300;
constexpr int kPage = 60;

struct Sort {
    const char *label, *by;
    bool desc;
};
const Sort kSorts[] = {
    {N_("Nylig lagt til"), "DateCreated,SortName", true},
    {N_("A\xE2\x80\x93\xC3\x85"), "SortName", false},
    {N_("Utgivelses\xC3\xA5r"), "PremiereDate,SortName", true},
    {N_("Vurdering"), "CommunityRating,SortName", true},
};
constexpr int kNumSorts = 4;
constexpr int kSortByName = 1;
/* The decades the filter offers (index 0: all). */
const int kDecades[] = {0, 2020, 2010, 2000, 1990, 1980, 1970, 1960, 1950};
constexpr int kNumDecades = 9;
enum FilterRow { FUnplayed, FFavorites, FGenre, FDecade, FReset, FRowCount };

} // namespace

Library::Library(jf::Client &client, std::string title, std::string types, std::string view_id, bool pushed,
                 std::string filter, int collection)
    : m_client(client), m_title(std::move(title)), m_pushed(pushed)
{
    m_sources.push_back({m_title, std::move(view_id), std::move(types), std::move(filter)});
    m_collection=collection;m_scope=browse_state::scope(client);
    load_preferences();
    m_nav.snap(1.f);
}

Library::~Library()
{
    std::lock_guard<std::mutex> g(m_data->lock);
    m_data->alive = false;
    ++m_data->generation;
    for(auto &entry:m_source_cache)if(entry.second.data!=m_data){
        std::lock_guard<std::mutex> other(entry.second.data->lock);
        entry.second.data->alive=false;++entry.second.data->generation;
    }
}

std::string Library::preference_key() const {
    const auto &s=source();return s.view+"\n"+s.types+"\n"+s.filter+"\n"+std::to_string(m_collection);
}
void Library::load_preferences(){
    if(m_collection>=0)return;
    const auto p=browse_state::library(m_scope,preference_key());
    m_sort=std::clamp(p.sort,0,kNumSorts-1);m_filters.unplayed=p.unplayed;
    m_filters.favorites=p.favorites;m_filters.decade=std::clamp(p.decade,0,kNumDecades-1);
    m_saved_genre=p.genre;
}
void Library::save_preferences(){
    if(m_collection>=0)return;
    browse_state::LibraryPrefs p;p.sort=m_sort;p.unplayed=m_filters.unplayed;
    p.favorites=m_filters.favorites;p.decade=m_filters.decade;
    {std::lock_guard<std::mutex> g(m_data->lock);
        if(m_filters.genre>0&&m_filters.genre<=(int)m_data->genres.size())m_saved_genre=m_data->genres[m_filters.genre-1];}
    p.genre=m_saved_genre;browse_state::save_library(m_scope,preference_key(),p);
}
void Library::sync_selection(){
    std::lock_guard<std::mutex> g(m_data->lock);
    if(!m_data->published)m_data->publish_items();
    if(m_snapshot!=m_data->published){
        m_snapshot=m_data->published;
        if(!m_anchor.empty())for(size_t i=0;i<m_snapshot->size();++i)
            if((*m_snapshot)[i].id==m_anchor){m_index=(int)i;break;}
        m_index=std::clamp(m_index,0,std::max(0,(int)m_snapshot->size()-1));
    }
    m_anchor=m_index<(int)m_snapshot->size()?(*m_snapshot)[m_index].id:std::string();
}

bool Library::needs_redraw() const
{
    std::lock_guard<std::mutex> g(m_data->lock);
    return m_data->revision != m_seen_revision ||
           (m_art_retry_at > 0 && music_art_now() >= m_art_retry_at);
}

void Library::request_visible_artwork(const std::vector<jf::Item> &visible)
{
    const auto d=m_data;
    m_art_retry_at=0;
    for(const auto &item:visible) {
        if(item.type!="MusicAlbum") continue;
        MusicArtwork cached;
        unsigned generation;
        uint64_t job;
        bool primary_failed;
        {
            std::lock_guard<std::mutex> g(d->lock);
            if(!d->alive || d->artwork_loading) return;
            auto &stored=d->artwork[item.id];
            if(!stored.accepts(item)) stored=MusicArtwork{};
            cached=stored;
            primary_failed=art::failed(music_image_url(m_client,cached.preferred(item)));
            if(!cached.needs_fetch(item,primary_failed)) {
                if(cached.attempts<3 && cached.retry_at>music_art_now() &&
                   (!m_art_retry_at || cached.retry_at<m_art_retry_at)) m_art_retry_at=cached.retry_at;
                continue;
            }
            d->artwork_loading=true;
            job=++d->artwork_job;
            generation=d->generation;
        }
        jf::Client *client=&m_client;
        auto current=[d,generation] {
            std::lock_guard<std::mutex> g(d->lock);
            return d->alive && generation==d->generation;
        };
        auto failure=[d,generation,job,id=item.id] {
            std::lock_guard<std::mutex> g(d->lock);
            if(job!=d->artwork_job) return;
            d->artwork_loading=false;
            if(d->alive && generation==d->generation) {
                auto &art=d->artwork[id]; ++art.attempts;
                art.retry_at=music_art_now()+5.0*art.attempts;
            }
            ++d->revision;
        };
        if(!emby5::spawn_io([d,client,generation,job,item,cached,primary_failed,current] {
            auto result=fetch_music_artwork(*client,item,cached,primary_failed,current);
            std::lock_guard<std::mutex> g(d->lock);
            if(job!=d->artwork_job) return;
            d->artwork_loading=false;
            if(d->alive && generation==d->generation) d->artwork[item.id]=std::move(result);
            ++d->revision; // wake the idle grid; no tab switch or button is needed
        },failure)) failure();
        return; // one metadata worker; visible/focused albums have priority
    }
}

void Library::set_sources(std::vector<Source> sources)
{
    if(sources.empty())return;
    const Source was=source();bool same=sources.size()==m_sources.size();
    for(size_t i=0;same&&i<sources.size();++i)same=sources[i].view==m_sources[i].view&&sources[i].types==m_sources[i].types&&sources[i].filter==m_sources[i].filter&&sources[i].label==m_sources[i].label;
    if(same)return;
    m_sources=std::move(sources);m_source=0;m_source_at=-1;
    for(size_t i=0;i<m_sources.size();++i)if(m_sources[i].view==was.view&&m_sources[i].types==was.types&&m_sources[i].filter==was.filter){m_source=(int)i;break;}
    m_pill=m_sources.size()>1?m_source:0;
    if(source().view==was.view&&source().types==was.types&&source().filter==was.filter)return;
    bool used;{std::lock_guard<std::mutex> g(m_data->lock);used=m_data->loading||m_data->total>=0;m_data->alive=false;++m_data->generation;}
    for(auto &v:m_source_cache)if(v.second.data!=m_data){std::lock_guard<std::mutex> g(v.second.data->lock);v.second.data->alive=false;++v.second.data->generation;}
    m_source_cache.clear();m_data=std::make_shared<Data>();m_snapshot.reset();m_anchor.clear();m_index=0;m_scroll.snap(0);
    m_filters={};m_sort=0;m_saved_genre.clear();load_preferences();
    if(used)load_more();
}

bool Library::square() const
{
    const std::string &t = source().types;
    return t == "MusicAlbum" || t == "MusicArtist" || t == "Playlist";
}

int Library::pill_count() const { return (m_sources.size() > 1 ? (int)m_sources.size() : 0) + 1; }   /* + the sort & filter button */

bool Library::by_name() const { return m_collection<0 && m_sort == kSortByName && source().types != "MusicArtist"; }

std::string Library::filter_query() const
{
    if(m_collection>=0)return {};
    std::string q, f;
    if (m_filters.unplayed)
        f = "IsUnplayed";
    if (m_filters.favorites)
        f += std::string(f.empty() ? "" : ",") + "IsFavorite";
    if (!f.empty())
        q += "&Filters=" + f;
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        const std::string genre=m_filters.genre>0&&m_filters.genre<=(int)m_data->genres.size()
            ?m_data->genres[m_filters.genre-1]:m_saved_genre;
        if(!genre.empty())q += "&Genres=" + jf::Client::escape(genre);
    }
    if (m_filters.decade > 0 && m_filters.decade < kNumDecades) {
        q += "&Years=";
        for (int y = 0; y < 10; y++)
            q += std::to_string(kDecades[m_filters.decade] + y) + (y < 9 ? "," : "");
    }
    return q;
}

int Library::active_filters() const
{
    return (int)m_filters.unplayed + (int)m_filters.favorites + (m_filters.genre > 0 || !m_saved_genre.empty()) + (m_filters.decade > 0);
}

std::string Library::types_for(const std::string &collection_type)
{
    if (collection_type == "movies") return "Movie";
    if (collection_type == "tvshows") return "Series";
    if (collection_type == "homevideos") return "Video";
    if (collection_type == "musicvideos") return "MusicVideo";
    if (collection_type == "boxsets") return "BoxSet";
    if (collection_type == "music") return "MusicAlbum";
    if (collection_type == "playlists") return "Playlist";
    return "Movie,Series,Video";   /* mixed */
}

void Library::reconcile_user_data(const jf::Item &fresh)
{
    std::set<Data*> seen;
    auto reconcile=[&](const std::shared_ptr<Data> &data){if(!seen.insert(data.get()).second)return;
        std::lock_guard<std::mutex> g(data->lock);
        for(auto &it:data->items)emby5::reconcile_user_data(it,fresh);
        data->publish_items();++data->revision;
    };
    reconcile(m_data);for(auto &entry:m_source_cache)reconcile(entry.second.data);
}

void Library::activate()
{
    m_nav.to(1.f);
    bool need;
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        need = m_data->total < 0 && !m_data->loading;
        for(auto &entry:m_data->artwork) {
            auto &art=entry.second;
            if(art.retry_at>0 && art.retry_at<=music_art_now()) {art.attempts=0;art.retry_at=0;}
        }
        ++m_data->revision;
    }
    if (need)
        load_more();
}

void Library::refresh_user_data()
{
    for(auto &entry:m_source_cache)if(entry.second.data!=m_data){std::lock_guard<std::mutex> g(entry.second.data->lock);entry.second.data->stale=true;}
    auto d = m_data;
    unsigned generation;
    int limit;
    {
        std::lock_guard<std::mutex> g(d->lock);
        if (d->total < 0 && !d->loading) return; // unopened tab stays lazy
        generation = ++d->generation;
        ++d->revision;
        limit = std::max(kPage, (int)d->items.size());
        d->loading = true;d->stale=false;
    }
    jf::Client *c = &m_client;
    const Source src = source();
    const Sort sort = kSorts[m_sort];
    const std::string filter = filter_query();
    const int collection=m_collection;
    if (!emby5::spawn_io([d, c, src, sort, filter, generation, limit,collection] {
        jf::Page p;
        if(collection>=0){
            p.ok=true;
            for(int start=0;start<limit;){
                auto part=c->collection_page((jf::Client::Collection)collection,src.view,start,std::min(120,limit-start));
                if(!part.ok){p.ok=false;break;}
                p.total=part.total;p.items.insert(p.items.end(),part.items.begin(),part.items.end());
                start+=(int)part.items.size();if(part.items.empty()||start>=part.total)break;
                {std::lock_guard<std::mutex> g(d->lock);if(!d->alive||generation!=d->generation)return;}
            }
        }else p=src.types=="MusicArtist"?c->album_artists(src.view,sort.by,sort.desc,0,limit)
             :c->library(src.view,src.types,sort.by,sort.desc,0,limit,src.filter+filter);
        std::lock_guard<std::mutex> g(d->lock);
        if (!d->alive || generation != d->generation) return;
        if (p.ok) { d->items = p.items; d->total = std::max(p.total,(int)d->items.size());d->publish_items(); }
        d->failed=!p.ok;
        d->loading = false;
        ++d->revision;
    }, [d, generation] {
        std::lock_guard<std::mutex> g(d->lock);
        if (generation == d->generation) { d->loading = false; ++d->revision; }
    })) {
        std::lock_guard<std::mutex> g(d->lock);
        if (generation == d->generation) { d->loading = false; ++d->revision; }
    }
}

void Library::reload()
{
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        m_data->items.clear();m_data->publish_items();m_data->failed=false;m_data->retry_at=0;
        m_data->total = -1;
        m_data->loading = false;
        m_data->generation++;
        ++m_data->revision;
    }
    m_index = 0;m_anchor.clear();m_snapshot.reset();
    m_scroll.snap(0);
    save_preferences();load_more();
}

void Library::load_more()
{
    std::shared_ptr<Data> d = m_data;
    int start;
    unsigned gen;
    {
        std::lock_guard<std::mutex> g(d->lock);
        if (d->loading || (d->failed && music_art_now()<d->retry_at) || (d->total >= 0 && (int)d->items.size() >= d->total))
            return;
        d->loading = true;
        start = (int)d->items.size();
        gen = d->generation;
    }
    const Sort s = kSorts[m_sort];
    jf::Client *c = &m_client;
    const Source src = source();
    const std::string fq = filter_query();
    auto failed=[d,gen] {
        std::lock_guard<std::mutex> g(d->lock);
        if(d->alive && gen==d->generation) {d->loading=false;d->failed=true;d->retry_at=music_art_now()+5; ++d->revision;}
    };
    const int collection=m_collection;
    if(!emby5::spawn_io([d, c, src, s, start, gen, fq,collection] {
        const auto page = collection>=0?c->collection_page((jf::Client::Collection)collection,src.view,start,kPage):src.types == "MusicArtist" ? c->album_artists(src.view, s.by, s.desc, start, kPage)
            : c->library(src.view, src.types, s.by, s.desc, start, kPage, src.filter + fq);
        std::lock_guard<std::mutex> g(d->lock);
        if(!d->alive || gen!=d->generation) return;
        if(page.ok) {
            std::set<std::string> ids;for(const auto &v:d->items)ids.insert(v.id);
            const size_t before=d->items.size();for(const auto &v:page.items)if(ids.insert(v.id).second)d->items.push_back(v);
            d->total=d->items.size()==before ? (int)d->items.size() : std::max(page.total,(int)d->items.size());
            d->publish_items();
        }
        d->failed=!page.ok;d->retry_at=page.ok?0:music_art_now()+5;
        d->loading=false;
        ++d->revision;
    },failed)) failed();
}

Action Library::search(bool)
{
    Action a;
    if (modal() || m_sources.empty() || m_collection >= 0) return a;
    const auto &src = source();
    a.kind = Action::Search;
    a.search_context = {m_sources.size() > 1 ? src.label : m_title,
                        src.types, src.view, src.filter, false};
    return a;
}

Action Library::input(uint32_t p)
{
    sync_selection();
    struct Remember{Library *self;~Remember(){
        if(self->m_snapshot&&self->m_index>=0&&self->m_index<(int)self->m_snapshot->size())self->m_anchor=(*self->m_snapshot)[self->m_index].id;
        self->sync_selection();}} remember{this};
    Action a;
    if ((p & NUVIO_BTN_TRIANGLE) && !modal()) return search();
    int count;
    bool more;   /* pages still to come: the last row loaded is not the end */
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        count = (int)m_data->items.size();
        more = m_data->total < 0 ? m_data->loading : count < m_data->total;
    }
    if(count==0&&(p&NUVIO_BTN_CROSS)){
        bool retry=false;{std::lock_guard<std::mutex> g(m_data->lock);retry=m_data->failed&&!m_data->loading;if(retry){m_data->retry_at=0;m_data->total=-1;}}
        if(retry){load_more();return a;}
    }
    if (m_filter_open) {
        filter_input(p);
        return a;
    }
    if (m_menu.active()) {
        m_menu.input(p, &a);
        return a;
    }
    if (p & NUVIO_BTN_OPTIONS) {
        std::lock_guard<std::mutex> g(m_data->lock);
        if (m_index >= 0 && m_index < (int)m_data->items.size())
            m_menu.open(m_data->items[m_index],m_collection==jf::Client::ResumeCollection);
        return a;
    }
    /* The pill row: the sources (when there is a choice), then the sorts. */
    const int ns = m_sources.size() > 1 ? (int)m_sources.size() : 0;
    if ((p & NUVIO_BTN_SQUARE) && m_collection<0 && !m_menu.active()) {   /* □: sort & filter, from anywhere here */
        open_sheet();
        return a;
    }
    if (m_in_pills) {
        if (p & (NUVIO_BTN_LEFT | NUVIO_BTN_RIGHT)) {
            m_pill = (p & NUVIO_BTN_LEFT) ? std::max(0, m_pill - 1) : std::min(pill_count() - 1, m_pill + 1);
            m_source_at = m_pill < ns && m_pill != m_source ? m_now + 0.35 : -1;   /* the library follows the focus */
        }
        else if (p & NUVIO_BTN_CROSS) {
            if (m_pill == ns) {   /* the sort & filter button: the sheet, on the current sort */
                open_sheet();
            } else if (m_pill < ns) {
                switch_source(m_pill);
            }
        } else if (p & NUVIO_BTN_DOWN) {
            if (m_source_at >= 0)
                switch_source(m_pill);   /* going down: the library under the focus, now */
            if (count > 0 || m_source_at < 0) {
                m_in_pills = false;
                m_index = std::clamp(m_index,0,std::max(0,count-1));
            }
        } else if (p & NUVIO_BTN_CIRCLE) {
            a.kind = m_pushed ? Action::Back : Action::ToNav;
        } else if ((p & NUVIO_BTN_UP) && !m_pushed) {
            a.kind = Action::ToNav;
        }
        return a;
    }
    const LibraryGeometry layout(gfx::W,kPad);
    const int kCols = layout.cols;
    const int row = m_index / kCols, col = m_index % kCols;
    if ((p & (NUVIO_BTN_L2 | NUVIO_BTN_R2)) && by_name()) {   /* A-Å: to the previous / next letter */
        jump_letter((p & NUVIO_BTN_R2) ? 1 : -1);
        return a;
    }
    if (p & NUVIO_BTN_RIGHT) {
        if (col + 1 < kCols && m_index + 1 < count)
            m_index++;
        else
            m_bump = true;
    } else if (p & NUVIO_BTN_LEFT) {
        if (col > 0)
            m_index--;
        else
            m_bump = true;
    } else if (p & NUVIO_BTN_DOWN) {
        if (m_index + kCols < count)
            m_index += kCols;
        else if (row + 1 <= (count - 1) / kCols)
            m_index = count - 1;   /* last, partial row */
        else if (!more)
            m_bump = true;         /* the library's end (not while its next page comes) */
    } else if (p & NUVIO_BTN_UP) {
        if (row > 0)
            m_index -= kCols;
        else if(m_pushed&&m_collection>=0){a.kind=Action::Back;}
        else {   /* up to the nearest pills: the left half to the libraries, the right to the sorts */
            m_in_pills = true;
            m_pill = ns > 0 && col < kCols / 2 ? m_source : ns;
        }
    } else if (p & NUVIO_BTN_CIRCLE) {
        if(m_pushed&&m_collection>=0){a.kind=Action::Back;return a;}
        /* Back: to the top of the grid, then to the sort row. */
        if (m_index >= kCols)
            m_index = col;
        else {
            m_in_pills = true;
            m_pill = ns > 0 ? m_source : ns;
        }
    } else if (p & NUVIO_BTN_CROSS) {
        std::lock_guard<std::mutex> g(m_data->lock);
        if (m_index < (int)m_data->items.size()) {
            a.kind = (m_collection==jf::Client::ResumeCollection||m_collection==jf::Client::NextUpCollection)?Action::Play:Action::Open;
            a.item = m_data->items[m_index];
        }
    }
    if (m_index > count - 24)
        load_more();
    return a;
}

void Library::enter_from_top()
{
    const int ns = m_sources.size() > 1 ? (int)m_sources.size() : 0;
    if(m_snapshot&&!m_snapshot->empty()) {m_in_pills=false;return;}
    m_in_pills = m_collection<0;
    m_pill = ns > 0 ? m_source : ns;
    m_source_at = -1;
}

void Library::switch_source(int i)
{
    m_source_at=-1;if(i==m_source||i<0||i>=(int)m_sources.size())return;
    save_preferences();
    m_source_cache[preference_key()]={m_data,m_index,m_sort,m_filters,m_scroll.value,m_anchor,m_saved_genre};
    m_source=i;m_snapshot.reset();
    const auto found=m_source_cache.find(preference_key());
    if(found!=m_source_cache.end()){
        const auto &v=found->second;m_data=v.data;m_index=v.index;m_sort=v.sort;m_filters=v.filters;
        m_scroll.snap(v.scroll);m_anchor=v.anchor;m_saved_genre=v.genre;
    }else{
        m_data=std::make_shared<Data>();m_index=0;m_sort=0;m_filters={};m_anchor.clear();m_saved_genre.clear();m_scroll.snap(0);load_preferences();
    }
    bool stale;{std::lock_guard<std::mutex> g(m_data->lock);stale=m_data->stale;}
    if(stale)refresh_user_data();else load_more();
}

void Library::open_sheet()
{
    if(m_collection>=0)return;
    m_filter_open = true;
    m_filter_row = m_sort;
    m_filter_a.to(1.f);
    std::shared_ptr<Data> d = m_data;
    bool need;
    {
        std::lock_guard<std::mutex> g(d->lock);
        need = !d->genres_loaded;
        d->genres_loaded = true;
    }
    if (need) {
        jf::Client *c = &m_client;
        const Source src = source();
        unsigned generation;
        {std::lock_guard<std::mutex> g(d->lock); generation=d->generation;}
        auto failure=[d,generation] {
            std::lock_guard<std::mutex> g(d->lock);
            if(d->alive && generation==d->generation) {d->genres_loaded=false; ++d->revision;}
        };
        if(!emby5::spawn_io([d, c, src, generation] {
            std::vector<std::string> gs = c->genres_in(src.view, src.types);
            std::lock_guard<std::mutex> g(d->lock);
            if(!d->alive || generation!=d->generation) return;
            d->genres = std::move(gs);
            ++d->revision;
        },failure)) failure();
    }
}

/* The filter sheet: Usett and Favoritter toggle, Sjanger and Tiår step with
 * left/right, Nullstill clears. Every change reloads at once. */
void Library::filter_input(uint32_t p)
{
    if (p & (NUVIO_BTN_CIRCLE | NUVIO_BTN_OPTIONS)) {
        m_filter_open = false;
        m_filter_a.to(0.f);
        return;
    }
    if (p & NUVIO_BTN_UP) {
        m_filter_row = std::max(0, m_filter_row - 1);
        return;
    }
    if (p & NUVIO_BTN_DOWN) {
        m_filter_row = std::min(kNumSorts + (int)FRowCount - 1, m_filter_row + 1);
        return;
    }
    if (m_filter_row < kNumSorts) {   /* Sorter etter: Cross picks */
        if ((p & NUVIO_BTN_CROSS) && m_filter_row != m_sort) {
            m_sort = m_filter_row;
            reload();
        }
        return;
    }
    int ngenres;
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        ngenres = (int)m_data->genres.size();
    }
    const int dir = (p & NUVIO_BTN_LEFT) ? -1 : (p & (NUVIO_BTN_RIGHT | NUVIO_BTN_CROSS)) ? 1 : 0;
    if (!dir)
        return;
    if(!m_saved_genre.empty()&&m_filters.genre==0){std::lock_guard<std::mutex> g(m_data->lock);
        auto it=std::find(m_data->genres.begin(),m_data->genres.end(),m_saved_genre);
        if(it!=m_data->genres.end())m_filters.genre=(int)(it-m_data->genres.begin())+1;}
    const Filters was = m_filters;
    const std::string was_genre=m_saved_genre;
    auto cycle = [dir](int i, int n) { return n > 0 ? ((i + dir) % n + n) % n : 0; };
    switch (m_filter_row - kNumSorts) {
    case FUnplayed: m_filters.unplayed = !m_filters.unplayed; break;
    case FFavorites: m_filters.favorites = !m_filters.favorites; break;
    case FGenre: m_filters.genre = cycle(m_filters.genre, ngenres + 1);m_saved_genre.clear(); break;
    case FDecade: m_filters.decade = cycle(m_filters.decade, kNumDecades); break;
    case FReset:
        if (p & NUVIO_BTN_CROSS)
            {m_filters = Filters();m_saved_genre.clear();}
        break;
    }
    if (m_filters.unplayed != was.unplayed || m_filters.favorites != was.favorites || m_filters.genre != was.genre ||
        m_filters.decade != was.decade || was_genre!=m_saved_genre)
        reload();
}

void Library::draw_filters(float dt)
{
    if (m_filter_a.step(dt, 14.f))
        m_animating = true;
    const float a = m_filter_a.value;
    if (a <= 0.01f)
        return;
    std::vector<std::string> genres;
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        genres = m_data->genres;
    }
    /* One sheet: Sorter etter (pick one, a check on it), then Filter. */
    gfx::fill({0, 0, gfx::W, gfx::H}, alpha(0x99000000u, a));
    const float w = 760, row_h = 62, gap = 4, head = 56;
    const int rows = kNumSorts + FRowCount;
    const float h = 40 + 2 * head + rows * (row_h + gap) + 90;
    const gfx::Rect r{(gfx::W - w) / 2, (gfx::H - h) / 2 + 24 * (1.f - a), w, h};
    glass_panel(r, 28, a);
    auto row_y = [&](int i) { return r.y + 40 + head + i * (row_h + gap) + (i >= kNumSorts ? head : 0); };
    gfx::text(r.x + 60, r.y + 40 + head - 16, T("Sorter etter"), {gfx::Bold, 22}, alpha(kText3, a));
    gfx::text(r.x + 60, row_y(kNumSorts) - 16, T("Filter"), {gfx::Bold, 22}, alpha(kText3, a));
    m_filter_drop.to({r.x + 30, row_y(m_filter_row), w - 60, row_h}, m_filter_row, r.x, r.y);
    m_filter_drop.draw(dt, a, &m_animating, 16);
    for (int i = 0; i < rows; i++) {
        const bool focus = i == m_filter_row;
        const float cy = row_y(i) + row_h / 2 + 9;
        std::string label, value;
        bool check = false;
        if (i < kNumSorts) {
            label = T(kSorts[i].label);
            check = i == m_sort;
        } else {
            switch (i - kNumSorts) {
            case FUnplayed: label = T("Bare usette"); value = m_filters.unplayed ? T("På") : T("Av"); break;
            case FFavorites: label = T("Bare favoritter"); value = m_filters.favorites ? T("På") : T("Av"); break;
            case FGenre:
                label = T("Sjanger");
                value = m_filters.genre > 0 && m_filters.genre <= (int)genres.size() ? genres[m_filters.genre - 1]
                                                                                      : m_saved_genre.empty()?std::string(T("Alle")):m_saved_genre;
                break;
            case FDecade: {
                label = T("Tiår");
                char b[64];
                std::snprintf(b, sizeof b, T("%d-tallet"), kDecades[m_filters.decade]);
                value = m_filters.decade > 0 ? std::string(b) : std::string(T("Alle"));
                break;
            }
            case FReset: label = T("Nullstill filtre"); break;
            }
        }
        gfx::text(r.x + 60, cy, label, {focus || check ? gfx::Bold : gfx::SemiBold, 25},
                  alpha(focus || check ? kText : kText2, a));
        if (check)
            draw_check(r.x + w - 76, cy - 9, 22, alpha(kText, a));
        if (!value.empty()) {
            const int fr = i - kNumSorts;
            const bool on = value != T("Av") && value != T("Alle");
            gfx::text(r.x + w - 60 - (focus && fr >= FGenre ? 30 : 0), cy, value, {gfx::Medium, 23, 330},
                      alpha(on ? kText : kText3, a), 2);
            if (focus && (fr == FGenre || fr == FDecade))
                gfx::text(r.x + w - 56, cy, "\xE2\x80\xBA", {gfx::Bold, 30}, alpha(kText2, a), 2);
        }
    }
    draw_pad_hints(r.x + 60, r.y + r.h - 42, {{PadButton::Cross, T("Velg")}, {PadButton::Circle, T("Ferdig")}}, 0, 26,
                   a);
}

/* A-Å (sorted by name): L1/R1 to the start of the previous / next letter. Emby
 * counts what sorts before a letter (NameLessThan, case-blind); that count is the
 * first title of the letter. The pages up to it load in one request. */
void Library::jump_letter(int dir)
{
    std::shared_ptr<Data> d = m_data;
    std::string name;
    int total;
    {
        std::lock_guard<std::mutex> g(d->lock);
        if (m_index >= (int)d->items.size() || d->loading)
            return;
        name = d->items[m_index].name;
        total = d->total;
    }
    size_t k = 0;
    while (k < name.size() && !std::isalnum((unsigned char)name[k]))
        k++;   /* past quotes and the like, as SortName does */
    const char c0 = k < name.size() ? (char)std::toupper((unsigned char)name[k]) : '#';
    const int cur = c0 >= 'A' && c0 <= 'Z' ? c0 - 'A' : -1;   /* -1: a digit or other, before A */
    const int at = m_index;
    jf::Client *c = &m_client;
    const Source src = source();
    const Sort s = kSorts[m_sort];
    const std::string fq = src.filter + filter_query();
    unsigned gen;
    {
        std::lock_guard<std::mutex> g(d->lock);
        gen = d->generation;
        d->loading = true;
    }
    auto failed=[d,gen] {
        std::lock_guard<std::mutex> g(d->lock);
        if(d->alive && gen==d->generation) {d->loading=false;d->failed=true;d->retry_at=music_art_now()+5; ++d->revision;}
    };
    if(!emby5::spawn_io([d, c, src, s, fq, dir, cur, at, total, gen] {
        auto letter = [](int i) { return std::string(1, (char)('A' + i)); };
        int target = -1;
        std::string shown;
        if (dir > 0) {
            for (int i = cur + 1; i < 26 && target < 0; i++) {
                const int n = c->count_before(src.view, src.types, fq, letter(i));
                if (n < 0)
                    break;
                if (n > at && n < total)
                    target = n, shown = letter(i);
            }
        } else {
            const int here = cur >= 0 ? c->count_before(src.view, src.types, fq, letter(cur)) : 0;
            if (here >= 0 && here < at)
                target = here, shown = cur >= 0 ? letter(cur) : "#";
            for (int i = cur - 1; i >= 0 && target < 0; i--) {
                const int n = c->count_before(src.view, src.types, fq, letter(i));
                if (n < 0)
                    break;
                if (n < at)
                    target = n, shown = letter(i);
            }
            if (target < 0 && at > 0)
                target = 0, shown = "#";
        }
        int have;
        {
            std::lock_guard<std::mutex> g(d->lock);
            have = (int)d->items.size();
        }
        jf::Page page;
        if (target >= 0 && have < target + kPage)
            page = c->library(src.view, src.types, s.by, s.desc, have, target + kPage - have, fq);
        std::lock_guard<std::mutex> g(d->lock);
        if (!d->alive || gen != d->generation) return;
        d->loading = false;
        ++d->revision;
        if(page.ok) d->items.insert(d->items.end(), page.items.begin(), page.items.end());
        if (target >= 0 && target < (int)d->items.size()) {
            d->publish_items();
            d->jump_to = target;
            d->jump_letter = shown;
        }
    },failed)) failed();
}

void Library::draw(double now, float dt)
{
    m_animating = false;
    if(m_source_at>=0&&now>=m_source_at&&m_in_pills)switch_source(m_pill);
    sync_selection();
    const auto snapshot=m_snapshot;
    const auto &items=*snapshot;
    std::unordered_map<std::string,MusicArtwork> artwork;
    int total;
    bool loading, failed;
    m_now = now;
    if (m_source_at >= 0) {   /* a library pill rested on long enough: show that library */
        m_animating = true;
        if (now >= m_source_at && m_in_pills)
            switch_source(m_pill);
    }
    {
        std::lock_guard<std::mutex> g(m_data->lock);
        if (m_data->jump_to >= 0) {   /* an A-Å jump landed */
            m_index = m_data->jump_to;
            m_in_pills = false;
            m_letter = m_data->jump_letter;
            m_letter_at = now;
            m_data->jump_to = -1;
        }

        m_seen_revision = m_data->revision;
        total = m_data->total;
        loading = m_data->loading;failed=m_data->failed;
    }
    if (loading)
        m_animating = true;
    m_index = std::min(m_index, std::max(0, (int)items.size() - 1));
    const jf::Item *focused = (!m_in_pills && m_index < (int)items.size()) ? &items[m_index] : nullptr;
    if (focused)
        m_ambient.set(focused->backdrop_blurhash.empty() ? focused->primary_blurhash : focused->backdrop_blurhash, now);
    else if (!items.empty())
        m_ambient.set(items[0].backdrop_blurhash.empty() ? items[0].primary_blurhash : items[0].backdrop_blurhash, now);
    m_ambient.draw(dt, 0.62f, &m_animating);

    /* Grid scroll. */
    const LibraryGeometry layout(gfx::W,kPad);
    const int kCols = layout.cols;
    const float kPosterW=layout.width, kPosterH=layout.height, kColGap=layout.gap, kRowPitch=layout.pitch;
    const int row = m_in_pills ? 0 : m_index / kCols;
    const bool sq = square();
    const float pitch = sq ? kPosterW+120.f : kRowPitch, tile_h = sq ? kPosterW : kPosterH;
    /* The focused row comes up near the top (whole, with the next row peeking below);
     * the first row stays under the header. */
    m_scroll.to(row > 0 ? (float)row * pitch - 60.f : 0.f);
    if (m_scroll.step(dt, 11.f))
        m_animating = true;
    m_nav.to(m_scroll.target < 1.f ? 1.f : 0.f);
    if (m_nav.step(dt, 10.f))
        m_animating = true;

    /* Header: title, count and the sort pills (fade with the nav). */
    const float ha = m_nav.value;
    if (ha > 0.01f) {
        const float hy = 220 - (1.f - ha) * 40;
        const int ns = m_sources.size() > 1 ? (int)m_sources.size() : 0;
        float tw = 0;
        if (ns == 0) {
            tw = gfx::text(kPad, hy, m_title, {gfx::Bold, 64}, alpha(kText, ha));
        } else {   /* the sources as a glass bar where the title would be, the drop on the picked one */
            std::vector<std::string> labels;
            for (int i = 0; i < ns; i++)
                labels.push_back(m_sources[i].label);
            const bool here = m_focused && m_in_pills && m_pill < ns;
            /* The pane reaches 6 px past the pills on each side, so they start on the title's column;
             * tw: the pills' own width, as the title's. */
            tw = pill_bar(kPad - 6, hy - 50, labels, here ? m_pill : m_source, here, m_src_drop, dt, ha, &m_animating,
                          0, hy) - 2 * 6;
        }
        if (total >= 0) {
            char cnt[32];
            std::snprintf(cnt, sizeof cnt, T("%d titler"), total);
            gfx::text(kPad + tw + 20, hy, cnt, {gfx::Medium, 24}, alpha(kText3, ha));
        }
        /* Sorting and filters behind one round glass button on the right (an icon:
         * tapering lines), what is in force written small beside it. */
        if(m_collection<0){
        const int nf = active_filters();
        const float bd = 66, bx = gfx::W - kPad - bd, by = hy - 44;
        const gfx::Rect btn{bx, by, bd, bd};
        glass_panel(btn, bd / 2, ha, false);
        const bool here = m_focused && m_in_pills && m_pill == ns;
        if (here)
            m_sort_drop.to(btn, 0, 0, hy);
        else
            m_sort_drop.hide();
        m_sort_drop.draw(dt, ha, &m_animating);
        const float icx = bx + bd / 2, icy = by + bd / 2;
        for (int k = 0; k < 3; k++) {   /* three bars, each shorter: sort & filter */
            const float lw = 30 - k * 9;
            gfx::fill({icx - lw / 2, icy - 10 + k * 9, lw, 3.5f}, alpha(kText, ha), 1.75f);
        }
        if (nf > 0) {   /* how many filters are on */
            const gfx::Rect badge{bx + bd - 22, by - 4, 26, 26};
            gfx::fill(badge, alpha(0xff0a84ffu, ha), 13);
            gfx::text(badge.x + 13, badge.y + 19, std::to_string(nf), {gfx::Bold, 17}, alpha(kText, ha), 1);
        }
        std::string state = T(kSorts[m_sort].label);
        if (nf > 0)
            state += std::string("  \xC2\xB7  ") + (nf == 1 ? T("1 filter") : std::to_string(nf) + T(" filtre"));
        gfx::text(bx - 18, hy, state, {gfx::Medium, 22, 520}, alpha(here ? kText2 : kText3, ha), 2);
        const float sw = gfx::text_width(state, {gfx::Medium, 22, 520});
        if (by_name() && !m_in_pills) {   /* A-Å: the letter jump, shown where it works */
            draw_pad_hints(bx - 18 - sw, hy + 44, {{PadButton::L2, ""}, {PadButton::R2, T("Hopp til bokstav")}}, 0, 22, ha);
        }
        }
    }

    /* The grid, clipped below the header. */
    const float top = kGridTop - m_scroll.value;
    gfx::push_scissor({0, (kGridTop - 60) * ha, gfx::W, gfx::H});
    const int first=std::max(0,(int)std::floor((-tile_h-60-top)/pitch))*kCols;
    const int last=std::min((int)items.size(),std::max(0,(int)std::ceil((gfx::H+20-top)/pitch)+1)*kCols);
    std::vector<jf::Item> display;
    if(first<last)display.assign(items.begin()+first,items.begin()+last);
    std::vector<jf::Item> visible_albums;
    {std::lock_guard<std::mutex> g(m_data->lock);
        for(const auto &item:display)if(item.type=="MusicAlbum"){
            auto i=m_data->artwork.find(item.id);if(i!=m_data->artwork.end())artwork.emplace(i->first,i->second);}}
    for(size_t j=0;j<display.size();++j){
        auto &item=display[j];if(item.type!="MusicAlbum")continue;
        if((int)j+first==m_index)visible_albums.insert(visible_albums.begin(),item);else visible_albums.push_back(item);
        const auto found=artwork.find(item.id);
        item=music_display_item(m_client,item,found!=artwork.end()?found->second:MusicArtwork{},art::failed);
    }
    request_visible_artwork(visible_albums);
    int focus_i = -1;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = first; i < last; i++) {
            const int r = i / kCols, c = i % kCols;
            const float y = top + r * pitch;
            if (y > gfx::H + 20 || y + tile_h + 60 < 0)
                continue;
            const bool f = m_focused && !m_in_pills && i == m_index;
            if (f)
                focus_i = i;
            if ((pass == 0) == f)
                continue;   /* focused poster last, over its neighbours */
            const float lift = m_lifts.step(display[(size_t)(i-first)].id, f, dt, &m_animating);
            const gfx::Rect tile{kPad + c * (kPosterW + kColGap), y, kPosterW, tile_h};
            draw_poster(m_client, display[(size_t)(i-first)], tile, lift, 1.f);
            if (f) {
                const float k = 1.f + 0.1f * theme_poster_lift(lift);
                m_card = {{tile.x - tile.w * (k - 1) / 2, tile.y - tile.h * (k - 1) / 2, tile.w * k, tile.h * k},
                          poster_url(m_client, display[(size_t)(i-first)], 480), display[(size_t)(i-first)].primary_blurhash, 14 * k};
                m_has_card = true;
            }
        }
    }
    gfx::pop_scissor();
    (void)focus_i;

    if (items.empty())
        gfx::text(gfx::W / 2, 560, failed?T("Could not load this list. Press X to retry."):loading || total < 0 ? T("Henter \xE2\x80\xA6")
                  : active_filters() > 0 ? T("Ingen titler passer filteret") : T("Ingenting her ennå"),
                  {gfx::Medium, 30}, kText2, 1);
    m_menu.draw(dt, &m_animating);
    if (art::animating())
        m_animating = true;

    /* After an A-Å jump: the letter, large on glass, for a moment. */
    const double since = now - m_letter_at;
    if (since < 1.1 && !m_letter.empty()) {
        const float la = since < 0.15 ? (float)(since / 0.15) : since > 0.8 ? (float)((1.1 - since) / 0.3) : 1.f;
        const gfx::Rect lr{gfx::W / 2 - 110, gfx::H / 2 - 110, 220, 220};
        glass_panel(lr, 44, la);
        gfx::text(gfx::W / 2, gfx::H / 2 + 46, m_letter, {gfx::Bold, 130}, alpha(kText, la), 1);
        m_animating = true;
    }
    draw_filters(dt);
}

} // namespace ui
