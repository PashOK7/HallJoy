#include "aula_mini60_native_model.h"
#include <cassert>
#include <array>
int main(){
    using namespace halljoy::mini60;
    std::array<bool,0x410> seen{};unsigned count=0;
    for(auto hid:Factory)if(hid){assert(hid<seen.size() && !seen[hid]);seen[hid]=true;++count;}
    assert(count==61 && Factory[0]==41 && Factory[34]==26 && Factory[49]==4 && Factory[50]==22 && Factory[51]==7 && Factory[85]==0x409);
    assert(Milli(0,34)==0 && Milli(85,34)==250 && Milli(170,34)==500 && Milli(340,34)==1000 && Milli(370,34)==1000);
    assert(Milli(100,0)==0 && Milli(100,65535)==0 && Milli(65535,34)==0);
    auto held=Pack(170,34,1000),other=Pack(340,34,1040);
    assert(Read(held,1050)==500 && Read(held,1051)==0 && Read(other,1051)==1000);
    assert(Read(Pack(0,34,1045),1051)==0);
    assert(Read(Pack(170,34,0xfffffff0u),10)==500 && Read(Pack(170,34,0xfffffff0u),100)==0);
    assert(Read(Pack(170,34,1001),1000)==0);
    std::array<std::uint8_t,4> r{};assert(Assigned(0,r.data())==41 && Assigned(85,r.data())==0x409 && Assigned(125,r.data())==0 && Assigned(126,r.data())==0);
    r={2,0,26,0};assert(Assigned(49,r.data())==26);
    r={2,2,0,0};assert(Assigned(49,r.data())==225);
    r={2,3,0,0};assert(Assigned(49,r.data())==0);
    r={2,1,26,0};assert(Assigned(49,r.data())==0);
    r={2,0,175,0};assert(Assigned(49,r.data())==0);
    r={6,0,0,0};assert(Assigned(49,r.data())==0);
}
