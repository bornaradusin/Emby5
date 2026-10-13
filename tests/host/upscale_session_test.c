/* CPU state reset only: no PS5 GPU work is executed by this host test. */
#include <assert.h>
#include <stdio.h>
#include "../../app/engine/media/src/evo_agc_runtime.c"
int32_t sceKernelMunmap(void *p,size_t n){(void)p;(void)n;assert(0);return 0;}
int32_t sceKernelReleaseDirectMemory(int64_t o,size_t n){(void)o;(void)n;assert(0);return 0;}
void evo_boot_log(const char *fmt,...){(void)fmt;}
int main(void) {
    g_agc_dev.up.requested=EVO_AGC_UPSCALE_AUTO;
    g_agc_dev.up.cap=EVO_AGC_UPSCALE_OFF;
    g_agc_dev.up.net_cap=0;
    g_agc_dev.up.over_budget_windows=8;
    g_agc_dev.up.window_us=999999;
    g_agc_dev.up.window_frames=23;
    g_agc_dev.up.downgrade_notice=0;
    g_agc_dev.up.mem_base[0]=(void *)(uintptr_t)0x100000;
    g_agc_dev.up.alloc_state[0]=1;
    g_agc_dev.up.mem_base[1]=NULL;
    g_agc_dev.up.alloc_state[1]=-1;
    evo_agc_upscale_begin_session();
    assert(g_agc_dev.up.requested==EVO_AGC_UPSCALE_AUTO);
    assert(g_agc_dev.up.cap==EVO_AGC_UPSCALE_AI && g_agc_dev.up.net_cap==2);
    assert(!g_agc_dev.up.window_frames && !g_agc_dev.up.window_us && !g_agc_dev.up.over_budget_windows);
    assert(g_agc_dev.up.downgrade_notice==-1);
    assert(g_agc_dev.up.mem_base[0]==(void *)(uintptr_t)0x100000 && g_agc_dev.up.alloc_state[0]==1);
    assert(g_agc_dev.up.alloc_state[1]==0);
    assert(sizeof(k_up_mode_name)/sizeof(k_up_mode_name[0])>EVO_AGC_UPSCALE_AUTO);
    assert(strcmp(k_up_mode_name[EVO_AGC_UPSCALE_AUTO],"Auto")==0);
    puts("PASS: actual upscaler CPU state resets each playback, retains selected Auto and allocated buffers, retries failed allocations");
    return 0;
}
