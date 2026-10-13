/* Runs the production jf_http_avio.cpp against a fake FFmpeg protocol.
 * Tests deadline/cleanup/error handling, not a real TLS or HTTP implementation. */
#include "jf/jf_http.h"
extern "C" {
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
}
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <thread>
struct AVDictionary {std::map<std::string,std::string> v;};
struct AVIOContext {AVIOInterruptCB interrupt;size_t at=0;};
static int mode=0,closed=0,interrupts=0;
static std::map<std::string,std::string> opts;
extern "C" {
int av_dict_set(AVDictionary **d,const char *k,const char *v,int) {if(!*d)*d=new AVDictionary;(*d)->v[k]=v;return 0;}
int av_dict_set_int(AVDictionary **d,const char *k,int64_t v,int flags) {return av_dict_set(d,k,std::to_string(v).c_str(),flags);}
void av_dict_free(AVDictionary **d) {delete *d;*d=nullptr;}
int avio_open2(AVIOContext **io,const char *,int flags,const AVIOInterruptCB *cb,AVDictionary **d) {
    assert(cb && cb->callback && flags==AVIO_FLAG_READ);opts=(*d)->v;
    if(mode==1)return AVERROR_HTTP_FORBIDDEN;
    *io=new AVIOContext{*cb};return 0;
}
int64_t avio_size(AVIOContext *) { return mode==6 ? -1 : mode==3 ? 40 : 0; }
int avio_closep(AVIOContext **io) {delete *io;*io=nullptr;++closed;return 0;}
int avio_read_partial(AVIOContext *io,unsigned char *p,int n) {
    if(mode==0)return AVERROR_EOF; // successful 204/empty body
    if(mode==4){std::this_thread::sleep_for(std::chrono::milliseconds(120));
        if(io->interrupt.callback(io->interrupt.opaque)){++interrupts;return AVERROR_EXIT;}
        *p='x';return 1;}
    if(mode==5){std::memset(p,'x',n);return n;} // unbounded body
    if(io->at)return (mode==3 || mode==6) ? -5 : AVERROR_EOF;
    const char *body="{\"Played\":true}";const int size=(int)std::strlen(body);assert(n>=size);
    std::memcpy(p,body,size);io->at+=size;return size;
}
int av_opt_get(void *,const char *,int,uint8_t **out) {
    const char *v="session=value; Path=/\nother=value2";
    *out=(uint8_t*)std::malloc(std::strlen(v)+1);std::strcpy((char*)*out,v);return 0;
}
void av_free(void *v){std::free(v);}
int av_strerror(int value,char *out,size_t size){std::snprintf(out,size,"Mock FFmpeg error %d",value);return 0;}
}
int main(){
    mode=0;auto r=jf::http_request("DELETE","https://example.test/PlayedItems/i",{},"",2);
    assert(r.ok()&&r.body.empty()&&closed==1&&r.cookies.size()==2);
    assert(opts["method"]=="DELETE"&&opts["reconnect"]=="0");
    mode=1;r=jf::http_request("GET","https://example.test/i",{},"",2);
    assert(r.status==403&&!r.ok()&&closed==1);
    mode=2;r=jf::http_request("POST","https://example.test/i",{"Authorization: test"},"{}",2);
    assert(r.ok()&&r.body=="{\"Played\":true}"&&closed==2);
    assert(opts["post_data"]=="7b7d"&&opts["headers"].find("Authorization: test\r\n")!=std::string::npos);
    mode=3;r=jf::http_request("GET","https://example.test/i",{},"",2);
    assert(!r.ok()&&r.body.empty()&&closed==3); // truncated JSON not a success
    mode=4;auto start=std::chrono::steady_clock::now();r=jf::http_request("GET","https://example.test/i",{},"",1);
    assert(!r.ok()&&r.body.empty()&&closed==4&&interrupts>0);
    assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(3));
    mode=5;r=jf::http_request("GET","https://example.test/i",{},"",2);
    assert(!r.ok()&&r.body.empty()&&closed==5&&r.error.find("16 MB")!=std::string::npos);
    mode=6;r=jf::http_request("GET","https://example.test/i",{},"",2);
    assert(r.ok()&&r.body=="{\"Played\":true}"&&closed==6); // normal HTTP close without Content-Length
    std::cout<<"PASS: production AVIO wrapper accepts empty success, rejects HTTP/truncated/oversized responses, interrupts slow drips and closes every open connection\n";
}
