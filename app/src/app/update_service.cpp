/* Independent Emby5 updater. GitHub asset SHA-256 is required before staging. */
#include "app/update_service.h"
#include "app/update_download.h"
#include "app/update_archive.h"
#include "app/settings.h"
#include "jf/jf_http.h"
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <atomic>
#include <cerrno>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
extern "C" {
#include "cJSON.h"
}
#ifndef EMBY5_VERSION
#define EMBY5_VERSION "0.0.1"
#endif
namespace update_service {
namespace {
const char *kRoot="/data/homebrew/PPSA99515";
const char *kStage="/data/homebrew/PPSA99515/.emby-update-stage";
std::mutex lock;
Snapshot state;
std::string asset_url,asset_sha;
std::atomic<bool> exit_after_handoff(false);
std::string pending_notification;
long version(const std::string &v){
    int a=0,b=0,c=0;
    const char *p=v.c_str();if(*p=='v'||*p=='V')++p;
    if(std::sscanf(p,"%d.%d.%d",&a,&b,&c)!=3 || a<0 || b<0 || c<0 || b>999 || c>999)return -1;
    return a*1000000L+b*1000L+c;
}
std::string field(const cJSON *j,const char *key){
    const cJSON *v=j?cJSON_GetObjectItemCaseSensitive(j,key):nullptr;
    return cJSON_IsString(v)&&v->valuestring?v->valuestring:"";
}
void report(const std::string &msg){std::lock_guard<std::mutex> g(lock);state.message=msg;}
bool create_stage(){
    if(mkdir(kRoot,0755)!=0 && errno!=EEXIST)return false;
    struct stat st{};
    if(lstat(kRoot,&st)!=0 || !S_ISDIR(st.st_mode))return false;
    if(mkdir(kStage,0755)!=0 && errno!=EEXIST)return false;
    return lstat(kStage,&st)==0 && S_ISDIR(st.st_mode);
}
bool read_ack(int fd){
    char buf[8]{};
    const int count=recv(fd,buf,sizeof(buf)-1,0);
    return count>=5 && std::memcmp(buf,"READY",5)==0;
}
bool send_helper(std::string *error){
    FILE *elf=std::fopen((std::string(kRoot)+"/update-helper.elf").c_str(),"rb");
    if(!elf){*error="Updater helper not installed";return false;}
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0){fclose(elf);*error="Cannot open PS5 payload loader connection";return false;}
    timeval timeout{};timeout.tv_sec=15;
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(9021);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(connect(sock,(sockaddr*)&addr,sizeof(addr))!=0){
        fclose(elf);close(sock);*error="PS5 ELF loader on port 9021 is not available";return false;
    }
    unsigned char buffer[65536];size_t n;
    bool ok=true;
    while((n=fread(buffer,1,sizeof(buffer),elf))>0){
        size_t sent=0;
        while(sent<n){const ssize_t k=send(sock,buffer+sent,n-sent,0);if(k<=0){ok=false;break;}sent+=(size_t)k;}
        if(!ok)break;
    }
    if(ferror(elf))ok=false;
    fclose(elf);
    if(ok)ok=read_ack(sock);
    close(sock);
    if(!ok)*error="Updater helper could not start or acknowledge";
    return ok;
}
}
void check(){
    {
        std::lock_guard<std::mutex> g(lock);
        if(state.checking || state.downloading || state.installing)return;
        state.checking=true;state.message="Checking GitHub Releases...";
    }
    std::thread([]{
        const jf::HttpResponse response=jf::http_request("GET","https://api.github.com/repos/bornaradusin/Emby5/releases/latest",
            {"Accept: application/vnd.github+json","User-Agent: Emby5/" EMBY5_VERSION},"",12);
        Snapshot next;
        std::string url,digest;
        if(!response.ok())next.message="GitHub update check failed";
        else{
            cJSON *root=cJSON_Parse(response.body.c_str());
            next.latest=field(root,"tag_name");
            next.notes=field(root,"body").substr(0,4096);
            const long newest=version(next.latest),installed=version(EMBY5_VERSION);
            if(newest<0 || installed<0)next.message="Release information unavailable";
            else if(newest>installed){
                next.available=true;
                const std::string wanted="Emby5-"+next.latest+".zip";
                const cJSON *assets=cJSON_GetObjectItemCaseSensitive(root,"assets");
                if(cJSON_IsArray(assets)){
                    const int size=cJSON_GetArraySize(assets);
                    for(int i=0;i<size;++i){
                        const cJSON *a=cJSON_GetArrayItem(assets,i);
                        if(field(a,"name")==wanted){
                            url=field(a,"browser_download_url");
                            digest=field(a,"digest");
                            break;
                        }
                    }
                }
                if(digest.compare(0,7,"sha256:")==0)digest.erase(0,7);
                if(url.empty() || digest.size()!=64){
                    next.message="New version found, but verified ZIP metadata is missing";
                }else next.message="Emby5 "+next.latest+" available";
            }else next.message="Up to date ("+std::string(EMBY5_VERSION)+")";
            if(root)cJSON_Delete(root);
        }
        {
            std::lock_guard<std::mutex> g(lock);
            asset_url=url;asset_sha=digest;
            state=std::move(next);
            if(state.available)pending_notification="Emby5 "+state.latest+" available - Settings > System";
        }
        if(settings::get().local.auto_download_updates && !url.empty() && !digest.empty())download();
    }).detach();
}
static std::atomic<bool> auto_install_requested{false};
void download_and_install(){
    const Snapshot u=snapshot();
    if(u.installing || u.downloading || u.checking ||
       (!u.available && !u.staged))return;
    if(u.staged){install();return;}
    auto_install_requested.store(true);
    download();
}
void download(){
    std::string url,sha;
    {
        std::lock_guard<std::mutex> g(lock);
        if(state.checking || state.downloading || state.installing || !state.available || asset_url.empty() || asset_sha.empty())return;
        state.downloading=true;state.staged=false;state.message="Downloading and verifying update...";
        url=asset_url;sha=asset_sha;
    }
    std::thread([url,sha]{
        std::string error;
        bool okay=create_stage();
        if(!okay)error="Cannot create update staging folder";
        if(okay){
            auto d=update_download::fetch_verified(url,sha,std::string(kStage)+"/release.zip");
            okay=d.ok;
            if(!okay)error=d.error;
        }
        if(okay){
            report("Checking update ZIP files...");
            okay=update_archive::unpack(std::string(kStage)+"/release.zip",std::string(kStage)+"/unpacked",&error);
        }
        {
            std::lock_guard<std::mutex> g(lock);
            state.downloading=false;state.staged=okay;
            state.message=okay?"Update verified and ready. Select Install update.":"Update rejected: "+error;
        }
        if(okay && auto_install_requested.exchange(false))install();
        if(!okay)auto_install_requested.store(false);
    }).detach();
}
void install(){
    {
        std::lock_guard<std::mutex> g(lock);
        if(!state.staged || state.downloading || state.installing)return;
        state.installing=true;state.message="Starting independent PS5 installer...";
    }
    std::thread([]{
        if(!create_stage()){report("Cannot access update staging folder");std::lock_guard<std::mutex> g(lock);state.installing=false;return;}
        const std::string parent=std::string(kStage)+"/parent.pid";
        FILE *f=fopen(parent.c_str(),"wb");
        if(!f){report("Cannot prepare updater handoff");std::lock_guard<std::mutex> g(lock);state.installing=false;return;}
        fprintf(f,"%ld\n",(long)getpid());
        const bool flushed=fflush(f)==0;
        const bool saved=flushed && (fsync(fileno(f))==0 || errno==EINVAL || errno==ENOSYS || errno==ENOTSUP || errno==EOPNOTSUPP);
        fclose(f);
        std::string error;
        if(!saved || !send_helper(&error)){
            report("Installer not started: "+(saved?error:std::string("cannot save handoff")));
            std::lock_guard<std::mutex> g(lock);state.installing=false;return;
        }
        report("Installer ready; closing Emby5...");
        exit_after_handoff.store(true);
    }).detach();
}
void later(){std::lock_guard<std::mutex> g(lock);pending_notification.clear();state.message="Update reminder dismissed for this session";}
std::string take_notification(){std::lock_guard<std::mutex> g(lock);std::string result=std::move(pending_notification);pending_notification.clear();return result;}
Snapshot snapshot(){std::lock_guard<std::mutex> g(lock);return state;}
bool should_exit(){return exit_after_handoff.exchange(false);}
}
