/* Emby5 - SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "ui/home.h"
#include <algorithm>
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
inline void order(ui::HomeModel &model,const std::vector<std::string> &sections,const std::vector<jf::Item> &views){
    using R=ui::HomeRow;
    auto rank=[&](R::Kind k){
        if(sections.empty())return (int)k;
        for(size_t i=0;i<sections.size();++i){const auto &s=sections[i];
            if((s=="resume"&&k==R::Resume)||(s=="nextup"&&k==R::NextUp)||(s=="latestmedia"&&k==R::Latest)||
               ((s=="smalllibrarytiles"||s=="librarybuttons")&&k==R::Libraries))return (int)i;
        }
        return (int)sections.size()+(int)k;
    };
    if(!sections.empty())model.rows.erase(std::remove_if(model.rows.begin(),model.rows.end(),[&](const R &r){
        return (r.kind==R::Resume||r.kind==R::NextUp||r.kind==R::Latest)&&rank(r.kind)>=(int)sections.size();}),model.rows.end());
    auto view_rank=[&](const std::string &id){for(size_t i=0;i<views.size();++i)if(views[i].id==id)return i;return views.size();};
    std::stable_sort(model.rows.begin(),model.rows.end(),[&](const R&a,const R&b){
        if(rank(a.kind)!=rank(b.kind))return rank(a.kind)<rank(b.kind);
        return a.kind==R::Latest&&view_rank(a.parent)<view_rank(b.parent);
    });
}
}
