/* Exercises the production scratch allocator with fake kernel allocations/fences.
 * No GPU rendering is performed. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "../../app/engine/media/src/evo_agc_runtime.c"
static int allocations, releases, fail_allocate, fail_map, fail_unmap;
static int64_t next_offset=0x200000;
int32_t sceKernelAllocateDirectMemory(int64_t a,int64_t b,size_t bytes,size_t alignment,int type,int64_t *off) {
    (void)a;(void)b;(void)type; assert(bytes && bytes%alignment==0);
    if(fail_allocate) return -12;
    *off=next_offset;next_offset+=(int64_t)bytes;++allocations;return 0;
}
int32_t sceKernelMapDirectMemory(void **p,size_t bytes,int prot,int flags,int64_t off,size_t align) {
    (void)bytes;(void)prot;(void)flags;(void)align;
    if(fail_map) return -12;
    *p=(void *)(uintptr_t)off;return 0;
}
int32_t sceKernelReleaseDirectMemory(int64_t off,size_t bytes){assert(off>=0&&bytes);++releases;return 0;}
int32_t sceKernelMunmap(void *p,size_t bytes){assert(p&&bytes);return fail_unmap?-1:0;}
void evo_boot_log(const char *fmt,...){(void)fmt;}
static void reset(void){memset(&g_agc_dev,0,sizeof g_agc_dev);for(int i=0;i<3;++i)g_agc_dev.up.mem_offset[i]=-1;}
int main(void){
    reset();
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    int count=allocations;
    for(int i=0;i<80;++i){
        evo_agc_upscale_begin_session();
        assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);
        assert(agc_upscale_alloc(0,1280,720,3840,2160,EVO_AGC_UPSCALE_AI)==0);
        assert(agc_upscale_alloc(0,1920,800,3840,1600,EVO_AGC_UPSCALE_AI)==0);
    }
    assert(allocations==count); // changing films does not consume more tiers
    assert(g_agc_dev.up.slot_capacity[1]==up_surface_bytes(1920,1080,8));
    // UL is right-sized, and an existing large footprint can be reused for smaller videos.
    assert(agc_upscale_alloc(1,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0); count=allocations;
    assert(agc_upscale_alloc(1,640,360,3840,2160,EVO_AGC_UPSCALE_AI)==0&&allocations==count);
    agc_upscale_release();
    // Beyond 1080p, grow after a completed GPU fence, never release live targets.
    reset();
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    assert(agc_upscale_alloc(0,2048,1152,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    assert(agc_upscale_alloc(0,2304,1296,3840,2160,EVO_AGC_UPSCALE_AI)<0);
    assert(g_agc_dev.up.reclaim_mask);
    static volatile uint32_t fence __attribute__((aligned(64)))=1;
    g_agc_dev.fences[0]=&fence;g_agc_dev.fence_expect[0]=0;g_agc_dev.up.block_fence[0][0]=2; // render timeout may clear its separate wait value
    int before=releases;
    agc_upscale_maintenance();assert(releases==before);
    fence=2;g_agc_dev.frame_active=1;
    agc_upscale_maintenance();assert(releases==before);
    g_agc_dev.frame_active=0;fail_unmap=1;
    agc_upscale_maintenance();assert(releases==before&&g_agc_dev.up.reclaim_mask);
    fail_unmap=0;g_agc_dev.up.frame_serial+=120;
    agc_upscale_maintenance();assert(releases==before+1&&!g_agc_dev.up.reclaim_mask);
    assert(agc_upscale_alloc(0,2304,1296,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    assert(agc_upscale_alloc(1,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    assert(agc_upscale_alloc(1,2304,1296,3840,2160,EVO_AGC_UPSCALE_AI)<0);
    agc_upscale_maintenance();
    assert(agc_upscale_alloc(1,2304,1296,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    agc_upscale_release();
    // An undersized surviving tier must not permanently prevent growth after OOM.
    reset();
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    fail_allocate=1;
    assert(agc_upscale_alloc(0,2304,1296,3840,2160,EVO_AGC_UPSCALE_AI)<0);
    assert(agc_upscale_alloc(0,2304,1296,3840,2160,EVO_AGC_UPSCALE_AI)<0);
    assert(g_agc_dev.up.reclaim_mask);
    before=releases;agc_upscale_maintenance();assert(releases==before+1);
    fail_allocate=0;
    assert(agc_upscale_alloc(0,2304,1296,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    agc_upscale_release();
    // A real out-of-memory condition backs off, but recovers without relaunch.
    reset();fail_allocate=1;
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)<0);
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)<0);
    fail_allocate=0;
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)<0);
    g_agc_dev.up.frame_serial+=120;
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    agc_upscale_release();
    reset();fail_map=1;before=releases;
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)<0&&releases==before+1);
    fail_map=0;evo_agc_upscale_begin_session();
    assert(agc_upscale_alloc(0,1920,1080,3840,2160,EVO_AGC_UPSCALE_AI)==0);
    agc_upscale_release();
    assert(allocations==releases);
    puts("PASS: real GPU scratch allocator reuses 80 playback sequences, grows only after idle fences, retries failures and releases each mock allocation once");
}
