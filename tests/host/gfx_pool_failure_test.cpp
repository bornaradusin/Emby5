/* Production texture pool must release physical pages if a mapping fails. */
#include <cassert>
#include <cstdlib>
#include <iostream>
#include "../../app/src/gfx/gfx_pool.cpp"
static int allocated=0,released=0,maps=0,fail_maps=3;
extern "C" {
int sceKernelAllocateDirectMemory(off_t,off_t,size_t,size_t,int,off_t *off){*off=++allocated;return 0;}
int sceKernelMapDirectMemory(void **p,size_t n,int,int,off_t,size_t){++maps;if(maps<=fail_maps)return -1;*p=std::malloc(n);return *p?0:-2;}
int sceKernelReleaseDirectMemory(off_t,size_t){++released;return 0;}
}
int main(){
 const auto result=gfx::pool_init(256u<<20);assert(result>0&&allocated==released+1&&maps==4);
 void *a=gfx::pool_alloc(4096),*b=gfx::pool_alloc(8192);assert(a&&b&&a!=b);gfx::pool_free(a);gfx::pool_free(b);
 assert(gfx::s_used==0&&gfx::s_free.size()==1);int old=allocated;assert(gfx::pool_init(256u<<20)==result&&allocated==old);
 std::free(gfx::s_base);gfx::s_base=nullptr;gfx::s_size=0;
 std::cout<<"PASS: actual texture-pool mapping failure releases physical pages, retries smaller allocations and reuses/coalesces live blocks\n";
}
