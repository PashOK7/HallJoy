#include "coherent_telemetry_cache.h"
#include <cassert>
#include <thread>
#include <iostream>
struct Snapshot {unsigned generation=0,count=0,device=0;};
int main() {
    halljoy::telemetry::CoherentCache<Snapshot> cache;
    halljoy::telemetry::Owner owner{1,2,0};Snapshot out{};bool reused=false;
    assert(!cache.Resolve(owner,true,false,{},100,out,reused));
    assert(cache.Resolve(owner,true,true,{1,1,42},100,out,reused) && !reused);
    for(unsigned i=0;i<100000;++i) {
        assert(cache.Resolve(owner,true,false,{2,0,0},101,out,reused));
        assert(reused && out.generation==1 && out.count==1 && out.device==42);
    }
    // A coherent empty inventory is applied immediately, without debounce.
    assert(cache.Resolve(owner,true,true,{2,0,0},102,out,reused) && out.count==0 && !reused);
    assert(cache.Resolve(owner,true,true,{3,1,7},103,out,reused) && out.device==7);
    assert(!cache.Resolve(owner,false,false,{},104,out,reused) && out.count==0);
    assert(!cache.Resolve(owner,true,false,{},105,out,reused));
    assert(cache.Resolve(owner,true,true,{4,1,7},106,out,reused));
    ++owner.process;assert(!cache.Resolve(owner,true,false,{},107,out,reused));
    assert(cache.Resolve(owner,true,true,{1,1,8},108,out,reused));
    ++owner.session;assert(!cache.Resolve(owner,true,false,{},109,out,reused));
    assert(cache.Resolve(owner,true,true,{1,1,9},110,out,reused));
    assert(!cache.Resolve(owner,true,false,{},1111,out,reused));
    assert(cache.Resolve(owner,true,true,{2,1,10},1112,out,reused));
    assert(cache.Resolve(owner,true,true,{1,1,9},1113,out,reused) && out.device==10);
    std::thread writer([&]{Snapshot local{};bool used=false;
        for(unsigned i=3;i<2000;++i) assert(cache.Resolve(owner,true,true,{i,1,i},1114,local,used));});
    for(unsigned i=0;i<2000;++i) {
        Snapshot local{};bool used=false;assert(cache.Resolve(owner,true,false,{},1114,local,used));
        assert(local.count==1 && (local.generation==2 ? local.device==10 : local.generation==local.device));
    }
    writer.join();std::cout<<"COHERENT_TELEMETRY_CACHE_TEST=PASS contention=100000 explicit_empty=1 generation_invalidation=1\n";
}
