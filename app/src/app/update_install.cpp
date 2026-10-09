/* Emby5 1.2.2 - independent safe replacement with rollback.
 * No third-party updater implementation reused. */
#include "app/update_install.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace update_install {
namespace {
constexpr const char *kRoot="/data/homebrew/PPSA99515";
constexpr const char *kTxn="/data/homebrew/PPSA99515/.emby-update-backup";
constexpr const char *kProgress="/data/homebrew/PPSA99515/.emby-update-backup/transaction";
const std::string kStage="/data/homebrew/PPSA99515/.emby-update-stage/unpacked/PPSA99515";
struct Item {std::string path;bool old;};
bool fail(std::string *err,const std::string &msg){if(err)*err=msg;return false;}
bool synced(FILE *file){
    if(fflush(file)!=0)return false;
    if(fsync(fileno(file))==0)return true;
    return errno==EINVAL || errno==ENOSYS || errno==ENOTSUP || errno==EOPNOTSUPP;
}
bool exists(const std::string &path){struct stat st{};return lstat(path.c_str(),&st)==0;}
bool safe_regular(const std::string &path){struct stat st{};return lstat(path.c_str(),&st)==0 && S_ISREG(st.st_mode);}
bool mkparents(const std::string &path){
    size_t at=1;
    while((at=path.find('/',at))!=std::string::npos){
        const std::string part=path.substr(0,at);
        struct stat st{};
        if(lstat(part.c_str(),&st)==0){if(!S_ISDIR(st.st_mode))return false;}
        else if(mkdir(part.c_str(),0755)!=0)return false;
        ++at;
    }return true;
}
bool descend(const std::string &root,const std::string &sub,std::vector<std::string>& paths){
    DIR *dir=opendir((root+sub).c_str());if(!dir)return false;
    struct dirent *entry;
    bool okay=true;
    while((entry=readdir(dir))!=nullptr){
        const char *name=entry->d_name;
        if(!strcmp(name,".") || !strcmp(name,".."))continue;
        const std::string rel=sub+"/"+name;
        struct stat st{};
        if(lstat((root+rel).c_str(),&st)!=0){okay=false;break;}
        if(S_ISDIR(st.st_mode)) {if(!descend(root,rel,paths)){okay=false;break;}}
        else if(S_ISREG(st.st_mode)) paths.push_back(rel.substr(1));
        else {okay=false;break;}
    }
    closedir(dir);return okay;
}
bool keep(const std::string &rel){
    if(rel=="eboot.bin" || rel=="update-helper.elf")return true;
    if(rel.compare(0,8,"sce_sys/")==0 || rel.compare(0,11,"sce_module/")==0 || rel.compare(0,9,"licenses/")==0)return true;
    return false;
}
bool read_transaction(std::vector<Item>& v){
    FILE *f=fopen(kProgress,"rb");if(!f)return false;
    char buf[512];bool valid=true;
    while(fgets(buf,sizeof buf,f)){
        const size_t n=strlen(buf);
        if(!n || buf[n-1]!='\n'){valid=false;break;}
        buf[n-1]=0;
        if(strlen(buf)<3 || buf[1]!=' ' || (buf[0]!='0' && buf[0]!='1') || !keep(buf+2)){valid=false;break;}
        v.push_back({buf+2,buf[0]=='1'});
    }
    if(ferror(f))valid=false;
    fclose(f);return valid;
}
bool append_transaction(const Item& item){
    FILE *f=fopen(kProgress,"ab");if(!f)return false;
    const int count=fprintf(f,"%c %s\n",item.old?'1':'0',item.path.c_str());
    const bool ok=count>0 && synced(f);
    fclose(f);return ok;
}
/* Directory/file removal only within the update-backup folder. */
bool cleanup_tree(const std::string &path){
    struct stat st{};if(lstat(path.c_str(),&st)!=0)return errno==ENOENT;
    if(!S_ISDIR(st.st_mode))return unlink(path.c_str())==0;
    DIR *d=opendir(path.c_str());if(!d)return false;
    struct dirent *e;bool ok=true;
    while((e=readdir(d))!=nullptr){
        if(!strcmp(e->d_name,".") || !strcmp(e->d_name,".."))continue;
        if(!cleanup_tree(path+"/"+e->d_name)){ok=false;break;}
    }
    closedir(d);return ok && rmdir(path.c_str())==0;
}
}
bool recover(std::string *err){
    if(!exists(kTxn))return true;
    if(exists(std::string(kTxn)+"/committed"))return cleanup_tree(kTxn) || fail(err,"Cannot clear previous successful update backup");
    std::vector<Item> applied;
    if(!read_transaction(applied))return fail(err,"Rollback record is corrupted; do not modify installation");
    for(auto it=applied.rbegin();it!=applied.rend();++it){
        const std::string dest=std::string(kRoot)+"/"+it->path;
        const std::string old=std::string(kTxn)+"/old/"+it->path;
        if(it->old){
            if(!safe_regular(old)) {
                if(safe_regular(dest)) continue; /* crash before backing up this file */
                return fail(err,"Previous application file is missing");
            }
            if(exists(dest) && unlink(dest.c_str())!=0)return fail(err,"Cannot remove partially installed file");
            if(rename(old.c_str(),dest.c_str())!=0)return fail(err,"Cannot restore previous version");
        }else if(exists(dest) && unlink(dest.c_str())!=0)return fail(err,"Cannot remove new file during rollback");
    }
    if(!cleanup_tree(kTxn))return fail(err,"Cannot clear rollback state");
    return true;
}
bool apply(const std::string &staging_root,std::string *err){
    /* Deliberately ignore caller-provided arbitrary locations. */
    if(staging_root!=kStage)return fail(err,"Unexpected staging path");
    if(!recover(err))return false;
    char actual[1024];
    if(!realpath(kRoot,actual) || strcmp(actual,kRoot)!=0)return fail(err,"Installation directory is not a normal Emby5 folder");
    if(!safe_regular(std::string(kRoot)+"/eboot.bin") || !safe_regular(kStage+"/eboot.bin"))return fail(err,"App executable is missing");
    if(!safe_regular(kStage+"/sce_sys/param.json"))return fail(err,"Release metadata is missing");
    std::vector<std::string> files;
    if(!descend(kStage,"",files) || files.empty())return fail(err,"Cannot enumerate update staging");
    for(const auto& rel:files)if(!keep(rel))return fail(err,"Unexpected file in staged update");
    /* Move the executable last to keep the old app launchable for as long as possible. */
    std::stable_sort(files.begin(),files.end(),[](const std::string&a,const std::string&b){
        if(a=="eboot.bin")return false;
        if(b=="eboot.bin")return true;
        if(a=="update-helper.elf")return false;
        if(b=="update-helper.elf")return true;
        return a<b;
    });
    if(!mkparents(std::string(kTxn)+"/old/file"))
        return fail(err,"Cannot create rollback folder");
    FILE *transaction=fopen(kProgress,"wb");if(!transaction)return fail(err,"Cannot open rollback record");
    const bool flushed=synced(transaction);
    fclose(transaction);if(!flushed)return fail(err,"Cannot save rollback record");
    for(const auto& rel:files){
        const std::string dest=std::string(kRoot)+"/"+rel;
        const std::string old=std::string(kTxn)+"/old/"+rel;
        const std::string staged=kStage+"/"+rel;
        if(!safe_regular(staged) || !mkparents(dest) || !mkparents(old)) {
            fail(err,"Update staging or destination is invalid");recover(nullptr);return false;
        }
        const bool had_previous=exists(dest);
        if(had_previous && !safe_regular(dest)){
            fail(err,"Refusing to replace non-regular app file");recover(nullptr);return false;
        }
        /* Record each write BEFORE file operations so power-loss recovery knows about it. */
        if(!append_transaction({rel,had_previous})){
            fail(err,"Could not persist rollback record");recover(nullptr);return false;
        }
        if((had_previous && rename(dest.c_str(),old.c_str())!=0) ||
           rename(staged.c_str(),dest.c_str())!=0 || chmod(dest.c_str(),0755)!=0){
            fail(err,"File replacement failed; attempting rollback");recover(nullptr);return false;
        }
    }
    /* A committed marker distinguishes an installed version from an interrupted transaction. */
    const std::string completed=std::string(kTxn)+"/committed";
    const std::string partial=completed+".tmp";
    FILE *done=fopen(partial.c_str(),"wb");
    if(!done || !synced(done)){
        if(done)fclose(done);
        fail(err,"Cannot commit update transaction");recover(nullptr);return false;
    }
    fclose(done);
    if(rename(partial.c_str(),completed.c_str())!=0){
        fail(err,"Cannot finalize update transaction");recover(nullptr);return false;
    }
    return true;
}
}
