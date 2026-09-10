#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_drunkdeer_identity.h"
#include <cassert>
#include <iostream>
#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_drunkdeer_maps.h"
#include "../HallJoy/first_run_layout.h"
using namespace halljoy::drunkdeer_identity;
int main() {
    auto report=Request();
    assert(report[0]==4 && report[1]==0xA0 && report[2]==2);
    for(std::size_t i=3;i<report.size();++i) assert(report[i]==0);
    assert(Parse(nullptr,64).model==Model::Unknown);
    for(auto entry : {std::array<int,4>{11,1,1,int(Model::A75Ansi)},
        {11,4,1,int(Model::A75Ansi)},{11,4,3,int(Model::A75Pro)},
        {11,4,2,int(Model::A75Iso)},{11,3,1,int(Model::G60)},
        {11,2,1,int(Model::G65)},{15,1,1,int(Model::G65)},
        {11,4,5,int(Model::G75Ansi)},{11,4,7,int(Model::G75Jis)}}) {
        report[5]=entry[0];report[6]=entry[1];report[7]=entry[2];
        assert(int(Parse(report.data(),64).model)==entry[3]);
        assert(Name(Parse(report.data(),64).model));
        for(std::size_t size=0;size<64;++size) assert(Parse(report.data(),size).model==Model::Unknown);
        assert(Parse(report.data(),65).model==Model::Unknown);
        for(int i=0;i<4;++i) { auto corrupt=report;corrupt[i]^=1;assert(Parse(corrupt.data(),64).model==Model::Unknown); }
    }
    for(unsigned a=0;a<256;++a) for(unsigned b=0;b<256;++b) {
        report[5]=a;report[6]=b;report[7]=255;
        assert(Parse(report.data(),64).model==Model::Unknown);
    }
    assert(!TrackingMap(Model::Unknown));
    assert(!MatchesProduct(Model::Unknown,0));
    for(unsigned i=1;i<=7;++i) {
        const auto model=static_cast<Model>(i);
        assert(MatchesProduct(model,Product(model)));
        assert(!MatchesProduct(model,0xffff));
        const auto* map=TrackingMap(model);
        assert(map && map->size()==126);
        assert(halljoy::layout_selection::MatchDrunkDeer(0x352d,Product(model),6,21,true,Name(model)));
        assert(!halljoy::layout_selection::MatchDrunkDeer(0x352d,Product(model),6,21,false,Name(model)));
        assert(!halljoy::layout_selection::MatchDrunkDeer(0x352d,0xffff,6,21,true,Name(model)));
        assert(!halljoy::layout_selection::MatchDrunkDeer(0x352d,Product(model),6,20,true,Name(model)));
        for(size_t a=0;a<126;++a) if((*map)[a])
            for(size_t b=a+1;b<126;++b) assert((*map)[a]!=(*map)[b]);
    }
    assert(kG60[21]==41 && kG60[0]==0);
    assert(kG65[35]==76 && kG65[36]==0 && kG65[119]==79);
    assert(kA75Iso[75]==50 && kA75Iso[85]==100 && kA75Iso[55]==0);
    assert(kG75Jis[34]==137 && kG75Jis[96]==135 && kG75Jis[75]==50);
    std::cout<<"DRUNKDEER_IDENTITY=PASS\n";
}
