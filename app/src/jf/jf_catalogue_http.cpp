/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "jf_catalogue_http.h"
#include "app/i18n.h"
#include "json_array_stream.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
extern "C" {
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
}

namespace jf {
namespace {
using Clock=std::chrono::steady_clock;
struct Deadline {
    Clock::time_point last=Clock::now(),started=last;
    std::chrono::seconds idle;
    const std::function<bool()> &cancel;
    bool timed_out=false,aborted=false;
    static int check(void *p) {
        auto &d=*static_cast<Deadline*>(p);
        if(d.cancel&&d.cancel()){d.aborted=true;return 1;}
        const auto now=Clock::now();
        if(now-d.last>=d.idle || now-d.started>=std::chrono::minutes(30)) {
            d.timed_out=true;return 1;
        }
        return 0;
    }
};
int http_status(int rc) {
    switch(rc) {
    case AVERROR_HTTP_BAD_REQUEST:return 400;
    case AVERROR_HTTP_UNAUTHORIZED:return 401;
    case AVERROR_HTTP_FORBIDDEN:return 403;
    case AVERROR_HTTP_NOT_FOUND:return 404;
    case AVERROR_HTTP_OTHER_4XX:return 499;
    case AVERROR_HTTP_SERVER_ERROR:return 500;
    default:return 0;
    }
}
}
namespace {
/* One GET read as it arrives: open (with the catalogue's deadline and error
 * texts), then chunk() for each piece until the end, a refusal or max_bytes. */
CatalogueHttpResult transfer(const std::string &url,const char *headers,
    const std::function<bool(const unsigned char *,size_t,std::string *)> &chunk,
    const std::function<bool(std::string *)> &finish,
    const std::function<bool()> &cancelled,
    const std::function<void(uint64_t)> &activity,int idle_seconds,uint64_t max_bytes,size_t *items) {
    CatalogueHttpResult result;
    if(url.rfind("http://",0)!=0&&url.rfind("https://",0)!=0) {
        result.error=N_("Invalid catalogue address");return result;
    }
    Deadline deadline{Clock::now(),Clock::now(),std::chrono::seconds(std::max(1,idle_seconds)),cancelled};
    AVIOInterruptCB interrupt{Deadline::check,&deadline};
    if(Deadline::check(&deadline)){result.cancelled=true;result.error=N_("Catalogue request cancelled");return result;}
    AVDictionary *options=nullptr;
    av_dict_set(&options,"method","GET",0);
    // JSON is not a seekable media file. No byte-range probe or reconnect replay.
    av_dict_set(&options,"seekable","0",0);
    av_dict_set(&options,"reconnect","0",0);
    av_dict_set(&options,"multiple_requests","0",0);
    av_dict_set(&options,"headers",headers,0);
    av_dict_set_int(&options,"rw_timeout",(int64_t)std::max(1,idle_seconds)*1000000,0);
    av_dict_set_int(&options,"timeout",(int64_t)std::max(1,idle_seconds)*1000000,0);
    AVIOContext *io=nullptr;
    const int opened=avio_open2(&io,url.c_str(),AVIO_FLAG_READ,&interrupt,&options);
    av_dict_free(&options);
    struct Close {AVIOContext **p;~Close(){if(*p)avio_closep(p);}} close{&io};
    if(opened<0) {
        result.status=http_status(opened);result.cancelled=deadline.aborted;
        result.transport_error=!result.status&&!result.cancelled;
        result.error=result.cancelled?N_("Catalogue request cancelled"):deadline.timed_out?
            N_("Catalogue connection made no progress"):result.status?"HTTP "+std::to_string(result.status):
            "Catalogue connection failed ("+std::to_string(opened)+")";
        return result;
    }
    result.status=200;deadline.last=Clock::now();
    const int64_t declared=avio_size(io);
    unsigned char buffer[16384];int n=0;
    while(true) {
        if(Deadline::check(&deadline)){n=AVERROR_EXIT;break;}
        n=avio_read_partial(io,buffer,sizeof buffer);
        if(n<=0)break;
        deadline.last=Clock::now();result.bytes+=(uint64_t)n;
        if(result.bytes>max_bytes){result.error=N_("Catalogue exceeds streaming safety limit");break;}
        if(activity)activity(result.bytes);
        if(!chunk(buffer,(size_t)n,&result.error))break;
        // Parsing/disk publication is local work, not a stalled network read.
        deadline.last=Clock::now();
    }
    if(items)result.items=*items;
    result.cancelled=deadline.aborted;
    if(result.error.empty()) {
        if(result.cancelled)result.error=N_("Catalogue request cancelled");
        else if(deadline.timed_out)result.error=N_("Catalogue transfer stopped making progress");
        else if(n<0&&n!=AVERROR_EOF && !(n==AVERROR(EIO)&&declared<0)) {
            result.error="Catalogue read failed ("+std::to_string(n)+")";result.transport_error=true;
        } else if(!finish(&result.error)){}
        else result.complete=true;
    }
    if(items)result.items=*items;
    if(deadline.timed_out)result.transport_error=true;
    return result;
}
}
CatalogueHttpResult http_catalogue_array(const std::string &url,
    const std::function<bool(const std::string &)> &item,
    const std::function<bool()> &cancelled,
    const std::function<void(uint64_t)> &activity,int idle_seconds) {
    JsonArrayStream parser(item);
    size_t items=0;
    return transfer(url,"Accept: application/json\r\nConnection: close\r\n",
        [&](const unsigned char *data,size_t n,std::string *error) {
            const bool ok=parser.feed(reinterpret_cast<const char*>(data),n);
            items=parser.items();
            if(!ok)*error=parser.error();
            return ok;
        },
        [&](std::string *error) {
            const bool ok=parser.finish();
            items=parser.items();
            if(!ok)*error=parser.error();
            return ok;
        },cancelled,activity,idle_seconds,512ull<<20,&items);
}
CatalogueHttpResult http_stream(const std::string &url,
    const std::function<bool(const char *, size_t)> &bytes,
    const std::function<bool()> &cancelled,
    const std::function<void(uint64_t)> &activity,int idle_seconds,uint64_t max_bytes) {
    return transfer(url,"Accept: application/xml, text/xml, */*\r\nConnection: close\r\n",
        [&](const unsigned char *data,size_t n,std::string *error) {
            if(bytes(reinterpret_cast<const char*>(data),n))return true;
            *error="stopped by the reader";   /* the caller knows why, and says so */
            return false;
        },
        [](std::string *){return true;},cancelled,activity,idle_seconds,max_bytes,nullptr);
}
}
