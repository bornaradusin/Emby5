/* Catalogue lookups for browsing. SPDX-License-Identifier: GPL-3.0-or-later */
std::string provider_identity(){
    iptv_xtream::Credentials c;iptv_xtream::load_credentials(&c);
    return root(c.server)+"\n"+c.username+"\n"+iptv_m3u::playlist_url();
}
bool find_item(const std::string &source,const std::string &id,bool series,Item *out){
    if(!out)return false;
    iptv_xtream::Credentials creds;const bool has=iptv_xtream::load_credentials(&creds);
    std::shared_ptr<const Index> index;FILE *raw=nullptr;uint64_t offset=0;
    {std::lock_guard<std::mutex> g(mu);
        if(source=="M3U"){for(const auto &v:m3u_movies)if(emby5::playlist_item_key(v.url)==id){*out=v;out->id=id;return true;}return false;}
        if(source!="Xtream"||!has)return false;
        index=visible_index(series);const auto it=index->by_id.find(id);if(it==index->by_id.end())return false;
        offset=it->second.offset;raw=it->second.file?std::fopen(it->second.file->path.c_str(),"rb"):nullptr;}
    std::unique_ptr<FILE,decltype(&std::fclose)> f(raw,&std::fclose);if(!f)return false;
    if(std::fseek(f.get(),(long)offset,SEEK_SET)!=0||!read_item(f.get(),*out))return false;
    if(!series)out->url=movie_url(creds,out->id,out->extension);
    return true;
}
int locate_item(bool series,const std::string &group,const std::string &source,const std::string &id,const std::string &search){
    // Reconciliation uses metadata refs, not a full deserialised catalogue.
    std::lock_guard<std::mutex> g(mu);
    const auto active=visible_index(series);const auto &index=*active;const auto needle=lower(search);int position=0;
    const auto entry=index.by_id.find(id);const auto refs=index.by_group.find(group);
    if(refs!=index.by_group.end())for(const auto &ref:refs->second){
        if(!needle.empty()&&ref.name.find(needle)==std::string::npos)continue;
        if(source=="Xtream"&&entry!=index.by_id.end()&&entry->second.offset==ref.offset&&entry->second.file==ref.file)return position;++position;
    }
    if(!series)for(const auto &v:m3u_movies){if(v.group!=group||(!needle.empty()&&lower(v.name).find(needle)==std::string::npos))continue;
        if(source==v.source&&emby5::playlist_item_key(v.url)==id)return position;++position;}
    return -1;
}
// Build a view from the original catalogue and stream resolver. A movie's
// optional metadata must not become a prerequisite for opening its stream.
Detail details(const Item &item) {
    Detail out;
    out.item = item;
    const auto before = provider_identity();
    if (!find_item(item.source, item.id, item.series, &out.item)) {
        out.error = N_("This title is no longer available. Refresh the catalogue.");
        return out;
    }
    if (item.series) {
        out.episodes = episodes(out.item, &out.error);
        out.ok = out.error.empty();
    } else {
        out.ok = !out.item.url.empty();
        if (!out.ok) out.error = N_("This title has no playable stream.");
    }
    if (before != provider_identity()) {
        out = {};
        out.error = N_("The provider changed. Reopen this title.");
    }
    return out;
}
