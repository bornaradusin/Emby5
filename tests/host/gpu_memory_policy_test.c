/* Real production planner and allocator; a bounded fake physical arena, no GPU. */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "../../app/engine/media/src/evo_agc_runtime.c"
struct allocation {int64_t offset;size_t bytes;int released,mapped;} records[2048];
static int count,released,attempts;static size_t used,peak,limit=(size_t)-1;
int evo_hw_is_ps5_pro(void){return 0;}
void evo_boot_log(const char *fmt,...){(void)fmt;}
int32_t sceKernelAllocateDirectMemory(int64_t a,int64_t b,size_t n,size_t align,int type,int64_t *off){
 (void)a;(void)b;(void)type;assert(n&&n%align==0);++attempts;
 if(n>limit||used>limit-n)return -12;
 assert(count<2048);*off=0x1000000ll+(int64_t)count*0x20000000ll;
 records[count++]=(struct allocation){*off,n,0,0};used+=n;if(used>peak)peak=used;return 0;}
static struct allocation *record(int64_t off){for(int i=0;i<count;++i)if(records[i].offset==off)return records+i;assert(0);return 0;}
int32_t sceKernelMapDirectMemory(void **p,size_t n,int prot,int flags,int64_t off,size_t align){
 (void)prot;(void)flags;(void)align;struct allocation *r=record(off);assert(!r->released&&r->bytes==n);r->mapped=1;*p=(void *)(uintptr_t)off;return 0;}
int32_t sceKernelMunmap(void *p,size_t n){struct allocation *r=record((int64_t)(uintptr_t)p);assert(r->mapped&&!r->released&&r->bytes==n);r->mapped=0;return 0;}
int32_t sceKernelReleaseDirectMemory(int64_t off,size_t n){struct allocation *r=record(off);assert(!r->mapped&&!r->released&&r->bytes==n);r->released=1;used-=n;++released;return 0;}
static void init(int mode,int net){
 assert(count==released&&used==0);memset(&g_agc_dev,0,sizeof g_agc_dev);g_agc_dev.width=3840;g_agc_dev.height=2160;
 for(int i=0;i<3;++i)g_agc_dev.up.mem_offset[i]=-1;
 for(size_t i=0;i<sizeof(g_agc_dev.pipelines)/sizeof(g_agc_dev.pipelines[0]);++i)g_agc_dev.pipelines[i].valid=1;
 g_agc_dev.up.requested=mode;g_agc_dev.up.net_pref=net;evo_agc_upscale_begin_session();}
static size_t bytes(void){return g_agc_dev.up.mem_bytes[0]+g_agc_dev.up.mem_bytes[1]+g_agc_dev.up.mem_bytes[2];}
int main(void){
 agc_up_plan_t p;limit=48u*1024*1024;
 init(EVO_AGC_UPSCALE_AUTO,EVO_AGC_UPNET_LARGE);
 assert(agc_upscale_plan(1920,1080,0,1,1,&p));assert(p.mode==EVO_AGC_UPSCALE_AI&&p.net==0);
 assert(strstr(evo_agc_upscale_label(),"memory fallback")&&g_agc_dev.up.requested==EVO_AGC_UPSCALE_AUTO);
 assert(bytes()==up_surface_bytes(1920,1080,4)+2*up_surface_bytes(1920,1080,8));
 int before=attempts;
 for(int i=0;i<150;++i){++g_agc_dev.up.frame_serial;assert(agc_upscale_plan(1920,1080,0,1,1,&p)&&p.net==0);}
 assert(attempts==before); // no per-frame allocation storm
 evo_agc_upscale_end_session();assert(!used);
 // At 1200-line input FSR's actual output footprint is smaller than Standard AI.
 init(EVO_AGC_UPSCALE_AUTO,EVO_AGC_UPNET_STANDARD);limit=42u*1024*1024;
 assert(agc_upscale_plan(1920,1200,0,1,1,&p));assert(p.mode==EVO_AGC_UPSCALE_SHARP);
 assert(strstr(evo_agc_upscale_label(),"memory fallback"));evo_agc_upscale_end_session();
 // Explicit Anime4K must not silently turn into FSR after all AI sizes fail.
 init(EVO_AGC_UPSCALE_AI,EVO_AGC_UPNET_STANDARD);
 assert(!agc_upscale_plan(1920,1200,0,1,1,&p));assert(!used);evo_agc_upscale_end_session();
 // Preference remains and recovers without restart when memory pressure disappears.
 init(EVO_AGC_UPSCALE_AUTO,EVO_AGC_UPNET_LARGE);limit=48u*1024*1024;
 assert(agc_upscale_plan(1920,1080,0,1,1,&p)&&p.net==0);
 limit=256u*1024*1024;g_agc_dev.up.frame_serial+=301;
 assert(agc_upscale_plan(1920,1080,0,1,1,&p)&&p.net==1);
 assert(!strstr(evo_agc_upscale_label(),"memory fallback"));evo_agc_upscale_end_session();
 // A small stream is charged its own surfaces, not a worst-case 4K floor.
 init(EVO_AGC_UPSCALE_AUTO,EVO_AGC_UPNET_STANDARD);limit=256u*1024*1024;
 assert(agc_upscale_plan(640,360,0,1,1,&p));size_t small=bytes();assert(small<16u*1024*1024);
 evo_agc_upscale_end_session();
 for(int i=0;i<200;++i){
   evo_agc_upscale_begin_session();const unsigned w=i%3==0?640:i%3==1?1280:1920,h=w*9/16;
   assert(agc_upscale_plan(w,h,0,1,1,&p));assert(bytes()<=48u*1024*1024);
   assert(strstr(evo_agc_upscale_memory_status(),"Reserved"));
   evo_agc_upscale_end_session();assert(!used&&count==released);
 }
 // Size, HDR and missing pipelines remain safety gates, not allocation retries.
 init(EVO_AGC_UPSCALE_AUTO,EVO_AGC_UPNET_STANDARD);before=attempts;
 assert(!agc_upscale_plan(1920,1080,1,1,1,&p));assert(!agc_upscale_plan(3840,2160,0,1,1,&p));
 assert(attempts==before);evo_agc_upscale_end_session();
 assert(!used&&count==released);
 printf("PASS: real planner: M->S and Auto->FSR memory fallback, bounded retries, explicit AI preserved, recovery, small-source footprint, 200 playback sessions, HDR/size gates (%d allocs/releases)\n",count);
}
