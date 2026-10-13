/* Production scratch ownership and lifetime; simulated kernel allocations/GPU fences. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "../../app/engine/media/src/evo_agc_runtime.c"
struct allocation { int64_t offset;size_t bytes;int released,mapped; } allocations[1024];
static int count,released,fail_alloc,fail_map,fail_release,fail_unmap;
static volatile uint32_t fences[EVO_AGC_FRAME_SLOTS][16] __attribute__((aligned(64)));
static struct allocation *find(int64_t off){for(int i=0;i<count;++i)if(allocations[i].offset==off)return &allocations[i];assert(0);return NULL;}
int32_t sceKernelAllocateDirectMemory(int64_t a,int64_t b,size_t n,size_t align,int type,int64_t *off){
 (void)a;(void)b;(void)type;assert(n&&n%align==0);if(fail_alloc)return -12;
 assert(count<1024);*off=0x1000000ll+(int64_t)count*0x10000000ll;allocations[count++]=(struct allocation){*off,n,0,0};return 0;}
int32_t sceKernelMapDirectMemory(void **p,size_t n,int prot,int flags,int64_t off,size_t align){
 (void)prot;(void)flags;(void)align;struct allocation *a=find(off);assert(a->bytes==n&&!a->released);
 if(fail_map)return -23;*p=(void *)(uintptr_t)off;a->mapped=1;return 0;}
int32_t sceKernelMunmap(void *p,size_t n){struct allocation *a=find((int64_t)(uintptr_t)p);assert(a->mapped&&a->bytes==n&&!a->released);if(fail_unmap)return -31;a->mapped=0;return 0;}
int32_t sceKernelReleaseDirectMemory(int64_t off,size_t n){struct allocation *a=find(off);assert(!a->mapped&&!a->released&&a->bytes==n);if(fail_release)return -32;a->released=1;++released;return 0;}
void evo_boot_log(const char *fmt,...){(void)fmt;}
void evo_boot_log_flush(void){}
int sceKernelUsleep(unsigned int us){(void)us;return 0;}
static void init(void){assert(count==released);memset(&g_agc_dev,0,sizeof g_agc_dev);memset((void*)fences,0,sizeof fences);
 for(int i=0;i<3;++i)g_agc_dev.up.mem_offset[i]=-1;
 for(int i=0;i<EVO_AGC_FRAME_SLOTS;++i)g_agc_dev.fences[i]=&fences[i][0];}
static void submit(unsigned slot,uint32_t marker){agc_upscale_note_submit(slot,marker);g_agc_dev.fence_expect[slot]=marker;g_agc_dev.up.frame_blocks=0;}
int main(void){
 init();size_t fsr=0,ai=0;
 for(int i=0;i<60;++i){
  evo_agc_upscale_begin_session();const int mode=i%2?EVO_AGC_UPSCALE_AI:EVO_AGC_UPSCALE_SHARP;
  assert(agc_upscale_alloc(0,i%3?1920:1280,i%3?1080:720,3840,2160,mode)==0);
  const int block=g_agc_dev.up.active_base;if(mode==EVO_AGC_UPSCALE_SHARP){fsr=g_agc_dev.up.mem_bytes[block];assert(!g_agc_dev.up.slot_capacity[block*5+2]);}else ai=g_agc_dev.up.mem_bytes[block];
  up_surface(UP_SLOT_L0,1920,1080,0);up_surface(UP_SLOT_E,3840,2160,0);
  if(i%4==1){assert(agc_upscale_alloc(1,1920,1080,3840,2160,mode)==0);up_surface(UP_SLOT_UL_A,1920,1080,1);}
  const unsigned slot=i%EVO_AGC_FRAME_SLOTS;const uint32_t marker=100+i*10;fences[slot][0]=marker-1;submit(slot,marker);
  int before=released;evo_agc_upscale_end_session();assert(released==before&&g_agc_dev.up.reclaim_mask);
  g_agc_dev.frame_active=1;fences[slot][0]=marker;agc_upscale_maintenance();assert(released==before);
  g_agc_dev.frame_active=0;agc_upscale_maintenance();assert(count==released&&!g_agc_dev.up.reclaim_mask);
 }
 assert(fsr<ai);
 // An unrelated menu submission must not pin scratch indefinitely.
 init();assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_SHARP)==0);up_surface(0,1920,1080,0);submit(0,7);
 fences[0][0]=7;agc_upscale_maintenance();assert(!g_agc_dev.up.block_fence[0][0]);
 fences[0][0]=0;submit(0,11);assert(!g_agc_dev.up.block_fence[0][0]);
 evo_agc_upscale_end_session();assert(count==released);
 // Clearing generic frame-wait bookkeeping does not prove scratch is safe.
 init();assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);up_surface(0,1920,1080,0);submit(0,22);
 g_agc_dev.fence_expect[0]=0;evo_agc_upscale_end_session();assert(count!=released);
 fences[0][0]=21;agc_upscale_maintenance();assert(count!=released);
 fences[0][0]=23;agc_upscale_maintenance();assert(count==released);
 // Wrapped nonzero markers are accepted; CPU reset zero never is.
 init();assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_SHARP)==0);up_surface(0,1920,1080,0);submit(0,UINT32_MAX-2);
 evo_agc_upscale_end_session();assert(count!=released);fences[0][0]=2;agc_upscale_maintenance();assert(count==released);
 // Runtime drain never turns timeout into proved ownership completion.
 init();g_agc_dev.initialized=1;g_agc_dev.fence_expect[0]=80;g_agc_dev.up.block_fence[0][0]=80;
 assert(!agc_wait_gpu_idle(1));assert(g_agc_dev.fence_expect[0]==80&&g_agc_dev.up.block_fence[0][0]==80);
 fences[0][0]=80;assert(agc_wait_gpu_idle(1));assert(!g_agc_dev.fence_expect[0]&&!g_agc_dev.up.block_fence[0][0]);g_agc_dev.initialized=0;
 // Failed map+release keeps a physical ownership record and can recover later.
 init();fail_map=fail_release=1;
 assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_SHARP)<0);
 assert(g_agc_dev.up.mem_bytes[0]>0&&g_agc_dev.up.reclaim_mask==1&&g_agc_dev.up.allocation_failure==5);
 evo_agc_upscale_begin_session();assert(g_agc_dev.up.alloc_state[0]<0);
 fail_release=fail_map=0;g_agc_dev.up.frame_serial+=120;agc_upscale_maintenance();assert(count==released);
 assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_SHARP)==0);
 fail_unmap=1;evo_agc_upscale_end_session();int before=released;assert(g_agc_dev.up.mem_base[0]&&count!=released);
 fail_unmap=0;agc_upscale_maintenance();assert(released==before);g_agc_dev.up.frame_serial+=120;agc_upscale_maintenance();assert(count==released);
 // Failed allocation backs off without consuming any physical block.
 init();fail_alloc=1;before=count;assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_SHARP)<0);assert(count==before);
 fail_alloc=0;g_agc_dev.up.frame_serial+=120;assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_SHARP)==0);evo_agc_upscale_end_session();assert(count==released);
 puts("PASS: production scratch lifecycle over 60 sessions, per-block submit ownership, unrelated frames, timeouts/wrap/reset safety, smaller FSR footprint and allocation/map/unmap/release failure recovery");
}
