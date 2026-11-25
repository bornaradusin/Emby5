/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/home.h"
#include <algorithm>
#include <cctype>
namespace home_sections {
inline std::string key(const ui::HomeRow &row){return std::to_string((int)row.kind)+"/"+(row.parent.empty()?row.title:row.parent);}
inline void merge(ui::HomeModel &model,ui::HomeRow row,bool ok){
    const std::string id=key(row);
    auto &failed=model.failed_sections;
    failed.erase(std::remove(failed.begin(),failed.end(),id),failed.end());
    if(!ok){failed.push_back(id);return;}
    auto &rows=model.rows;
    auto found=std::find_if(rows.begin(),rows.end(),[&](const ui::HomeRow &v){return key(v)==id;});
    if(row.items.empty()){if(found!=rows.end())rows.erase(found);return;}
    if(found!=rows.end())*found=std::move(row);else rows.push_back(std::move(row));
}
/* Which of Emby5's rows a server's home section shows. Besides the standard names, a section
 * by another name that shows what's being watched ("continue", "keepwatching", a plugin's
 * own) is Continue Watching, and with no separate Next Up in the layout it brings Next Up with
 * it, as a merged "keep watching" row does: before, any such name left both rows out (#11).
 * Music and audiobook resume sections are not Continue Watching. */
inline bool section_shows(const std::string &section,ui::HomeRow::Kind k,bool layout_has_nextup){
    using R=ui::HomeRow;
    std::string s=section;
    for(char &c:s)c=(char)std::tolower((unsigned char)c);
    const auto has=[&](const char *w){return s.find(w)!=std::string::npos;};
    const bool audio=s=="resumeaudio"||s=="resumebook";
    const bool resume=!audio&&(has("resume")||has("continue")||has("keepwatching"));
    switch(k){
    case R::Resume:return resume;
    case R::NextUp:return has("nextup")||(resume&&s!="resume"&&!layout_has_nextup);
    case R::Latest:return s=="latestmedia";
    case R::Libraries:return s=="smalllibrarytiles"||s=="librarybuttons";
    default:return false;
    }
}
inline void order(ui::HomeModel &model,const std::vector<std::string> &sections,const std::vector<jf::Item> &views){
    using R=ui::HomeRow;
    const bool layout_has_nextup=std::any_of(sections.begin(),sections.end(),[](const std::string &s){
        std::string l=s;for(char &c:l)c=(char)std::tolower((unsigned char)c);return l.find("nextup")!=std::string::npos;});
    auto rank=[&](R::Kind k){
        if(sections.empty())return (int)k;
        for(size_t i=0;i<sections.size();++i)
            if(section_shows(sections[i],k,layout_has_nextup))return (int)i;
        return (int)sections.size()+(int)k;
    };
    if(!sections.empty())model.rows.erase(std::remove_if(model.rows.begin(),model.rows.end(),[&](const R &r){
        return (r.kind==R::Resume||r.kind==R::NextUp||r.kind==R::Latest)&&rank(r.kind)>=(int)sections.size();}),model.rows.end());
    auto view_rank=[&](const std::string &id){for(size_t i=0;i<views.size();++i)if(views[i].id==id)return i;return views.size();};
    std::stable_sort(model.rows.begin(),model.rows.end(),[&](const R&a,const R&b){
        if(rank(a.kind)!=rank(b.kind))return rank(a.kind)<rank(b.kind);
        /* one merged section: Continue Watching, then Next Up, whichever arrived first */
        if(a.kind!=b.kind&&(a.kind==R::Resume||a.kind==R::NextUp)&&(b.kind==R::Resume||b.kind==R::NextUp))
            return a.kind==R::Resume;
        return a.kind==R::Latest&&view_rank(a.parent)<view_rank(b.parent);
    });
}
}
