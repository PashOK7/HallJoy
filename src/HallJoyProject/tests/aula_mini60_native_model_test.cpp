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
    std::array<std::uint8_t,4> r{};assert(Assigned(Factory,0,r.data())==41 && Assigned(Factory,85,r.data())==0x409 && Assigned(Factory,125,r.data())==0 && Assigned(Factory,126,r.data())==0);
    r={2,0,26,0};assert(Assigned(Factory,49,r.data())==26);
    r={2,2,0,0};assert(Assigned(Factory,49,r.data())==225);
    r={2,3,0,0};assert(Assigned(Factory,49,r.data())==0);
    r={2,1,26,0};assert(Assigned(Factory,49,r.data())==0);
    r={2,0,175,0};assert(Assigned(Factory,49,r.data())==0x409 && Assigned(Factory,85,r.data())==0x409);
    r={2,0,175,1};assert(Assigned(Factory,85,r.data())==0);
    r={6,0,0,0};assert(Assigned(Factory,49,r.data())==0);
    // AJAZZ AK820 MAX HE 0C45:80B1: 82 unique keys from the Driveall key list.
    std::array<bool,0x410> ak820Seen{};unsigned ak820Count=0;
    for(auto hid:Ak820Factory)if(hid){assert(hid<ak820Seen.size() && !ak820Seen[hid]);ak820Seen[hid]=true;++ak820Count;}
    assert(ak820Count==82 && KeyCount(Ak820Pid)==82 && KeyCount(0x80a2)==61 && &FactoryFor(Ak820Pid)==&Ak820Factory && &FactoryFor(0x8032)==&Factory);
    assert(Ak820Factory[0]==41 && Ak820Factory[1]==58 && Ak820Factory[12]==69 && Ak820Factory[34]==26 && Ak820Factory[49]==4);
    assert(Ak820Factory[85]==0x409 && Ak820Factory[86]==0 && Ak820Factory[88]==80 && Ak820Factory[91]==79 && Ak820Factory[106]==76 && Ak820Factory[108]==78);
    r={0,0,0,0};assert(Assigned(Ak820Factory,1,r.data())==58 && Assigned(Factory,1,r.data())==0 && Assigned(Ak820Factory,86,r.data())==0);
    assert(!Ak820Admitted && !SupportedProduct(Ak820Pid) && !SupportedProduct(0x80b0) && Token(Ak820Pid)==Ak820LayoutToken && Ak820LayoutToken==0xE73449D3F54AAB62ull);
    assert(Identity(Ak820Pid,0x0c45,Ak820Pid,0,0) && !Identity(Ak820Pid,0x0c45,0x80a2,0x0166,0x110c) && !Identity(0x80a2,0x0c45,Ak820Pid,0,0) && !Identity(Ak820Pid,0x0416,Ak820Pid,0,0));
    assert(!Identity(0x80a2,0x0c45,0x80a2,0,0x110c) && Identity(0x80a2,0x0c45,0x80a2,0x0166,0x110c));
    assert(InfoLength(Ak820Pid)==48 && InfoLength(0x80a2)==56);
    // AK820 hold: last depth stays until 0 or a value inside the top dead zone; no time expiry.
    assert(Held(Pack(170,34,0),0)==500 && Held(Pack(170,34,0),30)==500 && Held(Pack(28,34,0),30)==0);
    assert(Held(Pack(31,34,0),30)==91 && Held(Pack(16,34,0),0)==47 && Held(Pack(0,34,0),0)==0 && Held(0,0)==0);
    // Driveall advanced keys (SOCD/RS/DKS/MT/TGL) keep the physical key on AK820 only.
    for(std::uint8_t page:{8,9,10,11,12}){
        r={page,1,49,51};assert(Assigned(Ak820Factory,49,r.data(),true)==4 && Assigned(Ak820Factory,51,r.data(),true)==7);
        assert(Assigned(Factory,49,r.data())==0);
    }
    r={6,0,0,0};assert(Assigned(Ak820Factory,49,r.data(),true)==0);
    r={13,0,0,1};assert(Assigned(Ak820Factory,49,r.data(),true)==0);
    r={2,0,45,0};assert(Assigned(Ak820Factory,48,r.data(),true)==45);
}
