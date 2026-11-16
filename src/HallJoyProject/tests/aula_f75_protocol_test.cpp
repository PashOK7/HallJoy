#include "aula_f75_protocol.h"
#include <cassert>
#include <set>
#include <string_view>
int main() {
    using namespace halljoy::f75;
    assert(!kStockF75TrialEnabled); // Owner ended stock trials; protocol research stays executable.
    Candidate c{L"fixture",L"AULA F75 HE Dongle",0x0c45,0xfefe,0xff60,0x61,33,33};
    assert(Collection(c));
    assert(!RuntimeCollection(c));
    Candidate boom{L"boom",L"BOOM68-USB",0x0c45,0x80b5,0xff68,0x61,65,65};
    assert(RuntimeCollection(boom));
    auto usb=c;usb.vid=0x38a6;usb.pid=0x2742;usb.page=0xff68;
    c.path=L"radio";usb.path=L"usb";
    std::vector<Candidate> priority{c,usb};PreferWired(priority);assert(priority.front().vid==0x38a6);
    assert(NeedsSwitch(c,priority) && !NeedsSwitch(usb,priority));
    assert(!NeedsSwitch(c,{c}) && NeedsSwitch(usb,{c}) && NeedsSwitch(c,{}));
    assert(CompatibleInfo(c,{0x38a6,0x2742,0xffff,0xffff,255,255}));
    assert(CompatibleInfo(c,{0x0c45,0xfefe}));
    assert(!CompatibleInfo(c,{0x38a6,0x2728}));
    assert(std::wstring_view(halljoy::layout_identity::Match(Token(c)))==L"Aula F75 HE (2742 and 2.4G) ANSI");
    c.name=L"unrelated";assert(!Collection(c));
    c.vid=0x38a6;c.pid=0x2729;c.page=0xff68;assert(Collection(c));
    assert(std::wstring_view(halljoy::layout_identity::Match(Token(c)))==L"Aula F75 HE (2729) ANSI");
    c.pid=0x2742;assert(Collection(c));c.page=0xff60;assert(!Collection(c));
    const auto positions=Positions();assert(positions.size()==80 && positions.front()!=0 && positions.back()==0);
    std::set<unsigned> physical;
    for(auto i:positions)assert(Factory[i] && physical.insert(Factory[i]).second);
    auto r=Request(0x68,24,256);r[1]=0x55;
    assert(Header(r,33,0x68,24,256));assert(!Header(r,32,0x68,24,256));
    assert(!Header(r,33,0x68,24,0));r[0]=1;assert(!Header(r,33,0x68,24,256));
    for(unsigned bit=0;bit<8;++bit) {
        std::uint8_t key[]={2,std::uint8_t(1u<<bit),0,0};assert(Assigned(key,17)==224+bit);
    }
    std::uint8_t key[]={2,0,132,0};assert(Assigned(key,17)==132);
    key[0]=3;assert(!Assigned(key,17));key[0]=0;assert(Assigned(key,17)==Factory[17]);
    key[0]=2;key[2]=0xaf;assert(Assigned(key,17)==1033);
    assert(Milli(0,34)==0 && Milli(170,34)==500 && Milli(340,34)==1000);
}
