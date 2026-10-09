/* Emby5 independent updater; no third-party updater source reused. */
#include "app/update_download.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <unistd.h>
#include <openssl/sha.h>
extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
#include <libavutil/dict.h>
}
namespace update_download {
static bool hex_valid(const std::string &s) {
    if(s.size()!=64) return false;
    for(char ch:s) if(!std::isxdigit(static_cast<unsigned char>(ch))) return false;
    return true;
}
Result fetch_verified(const std::string &url,const std::string &expected,const std::string &dest) {
    Result r;
    /* Never follow arbitrary download URLs from JSON metadata. */
    if(url.rfind("https://github.com/bornaradusin/Emby5/releases/download/",0)!=0 || !hex_valid(expected)) {
        r.error="Untrusted update URL or missing SHA-256"; return r;
    }
    const std::string temp=dest+".part";
    remove(temp.c_str());
    AVDictionary *opts=nullptr;
    av_dict_set_int(&opts,"timeout",20000000,0);
    av_dict_set_int(&opts,"rw_timeout",20000000,0);
    av_dict_set(&opts,"user_agent","Emby5-Updater/1",0);
    AVIOContext *input=nullptr;
    const int opened=avio_open2(&input,url.c_str(),AVIO_FLAG_READ,nullptr,&opts);
    av_dict_free(&opts);
    if(opened<0) { r.error="HTTPS connection failed"; return r; }
    FILE *output=fopen(temp.c_str(),"wb");
    if(!output) { avio_closep(&input); r.error="Cannot create update download"; return r; }
    SHA256_CTX sha;
    SHA256_Init(&sha);
    constexpr uint64_t kLimit=512ull*1024*1024;
    unsigned char buffer[65536];
    int count=0;
    bool ok=true;
    while((count=avio_read(input,buffer,sizeof(buffer)))>0) {
        if(r.bytes + static_cast<uint64_t>(count)>kLimit || fwrite(buffer,1,count,output)!=static_cast<size_t>(count)) {
            r.error="Update is too large or storage is full"; ok=false; break;
        }
        SHA256_Update(&sha,buffer,count);
        r.bytes+=count;
    }
    /* FFmpeg uses AVERROR_EOF for clean completion. */
    if(count<0 && count!=AVERROR_EOF) { r.error="Download interrupted"; ok=false; }
    avio_closep(&input);
    const bool flushed=fflush(output)==0;
    const bool durable=flushed && (fsync(fileno(output))==0 || errno==EINVAL || errno==ENOSYS || errno==ENOTSUP || errno==EOPNOTSUPP);
    if(!durable) { r.error="Download could not be saved"; ok=false; }
    if(fclose(output)!=0) { r.error="Download write failed"; ok=false; }
    unsigned char hash[SHA256_DIGEST_LENGTH]; SHA256_Final(hash,&sha);
    static const char d[]="0123456789abcdef";
    for(unsigned char b:hash){r.sha256.push_back(d[b>>4]);r.sha256.push_back(d[b&15]);}
    std::string wanted=expected;
    for(char &c:wanted)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(ok && r.sha256!=wanted) {r.error="Update SHA-256 mismatch";ok=false;}
    if(ok && r.bytes<1024) {r.error="Update ZIP is incomplete";ok=false;}
    if(!ok) {remove(temp.c_str());return r;}
    if(rename(temp.c_str(),dest.c_str())!=0){remove(temp.c_str());r.error="Cannot stage verified update";return r;}
    r.ok=true;return r;
}
}
