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
    // A75 Ultra (official driver type 756): own model, A75 ANSI tracking positions,
    // no automatic layout (no Ultra preset), wide FD tracking.
    report[5]=11;report[6]=4;report[7]=4;
    assert(Parse(report.data(),64).model==Model::A75Ultra);
    assert(MatchesProduct(Model::A75Ultra,0x2383) && !MatchesProduct(Model::A75Ultra,0x2382));
    assert(TrackingSource(Model::A75Ultra)==Model::A75Ansi && TrackingSource(Model::G65)==Model::G65);
    assert(TrackingMap(TrackingSource(Model::A75Ultra))==&kA75Ansi && !TrackingMap(Model::A75Ultra));
    assert(!halljoy::layout_selection::MatchDrunkDeer(0x352d,0x2383,6,21,true,Name(Model::A75Ultra)));
    // Firmware number = digits of payload[8] then payload[7] (native r[9], r[8]).
    for(auto entry : {std::array<unsigned,3>{3,5,35},{1,7,17},{0,35,35},{1,12,112},{0,0,0},{2,0,20}}) {
        report[9]=entry[0];report[8]=entry[1];
        assert(Parse(report.data(),64).firmware==entry[2]);
    }
    assert(TrackingMode(Model::A75Ultra,0)==Tracking::Wide);
    assert(TrackingMode(Model::A75Ansi,34)==Tracking::Legacy && TrackingMode(Model::A75Ansi,35)==Tracking::HighPrecision);
    assert(TrackingMode(Model::A75Pro,16)==Tracking::Legacy && TrackingMode(Model::A75Pro,17)==Tracking::HighPrecision);
    assert(TrackingMode(Model::A75Iso,24)==Tracking::Legacy && TrackingMode(Model::A75Iso,25)==Tracking::HighPrecision);
    assert(TrackingMode(Model::G65,99)==Tracking::Legacy && TrackingMode(Model::Unknown,99)==Tracking::Legacy);
    assert(Travel(Tracking::Legacy,0)==0.0f && Travel(Tracking::Legacy,1)==1.0f/40 && Travel(Tracking::Legacy,40)==1.0f);
    assert(Travel(Tracking::Legacy,255)==1.0f);
    assert(Travel(Tracking::HighPrecision,1)==0.0f && Travel(Tracking::HighPrecision,100)==0.5f && Travel(Tracking::HighPrecision,255)==1.0f);
    assert(Travel(Tracking::Wide,39)==0.0f && Travel(Tracking::Wide,330)==0.5f && Travel(Tracking::Wide,660)==1.0f && Travel(Tracking::Wide,700)==1.0f);
    const auto wide=WideTrackingRequest();
    assert(wide[0]==4 && wide[1]==0xFD && wide[2]==3 && wide[3]==1);
    for(std::size_t i=4;i<wide.size();++i) assert(wide[i]==0);
    std::array<std::uint8_t,64> frame{};frame[0]=4;frame[1]=0xFD;frame[2]=6;
    std::size_t total=0;
    for(std::size_t chunk=0;chunk<kWideChunks;++chunk) {
        frame[3]=static_cast<std::uint8_t>(chunk);std::size_t out=99;
        assert(WideChunk(frame.data(),64,out) && out==chunk);
        assert(!WideChunk(frame.data(),63,out) && !WideChunk(frame.data(),65,out));
        assert(4+2*WideChunkValues(chunk)<=64);total+=WideChunkValues(chunk);
    }
    assert(total==126);
    std::size_t out=0;
    frame[3]=5;assert(!WideChunk(frame.data(),64,out));
    frame[3]=0;frame[2]=8;assert(!WideChunk(frame.data(),64,out));
    frame[2]=6;frame[1]=0xB7;assert(!WideChunk(frame.data(),64,out));
    frame[4]=0x94;frame[5]=0x02;assert(WideValue(frame.data(),0)==660);
    assert(kG60[21]==41 && kG60[0]==0);
    assert(kG65[35]==76 && kG65[36]==0 && kG65[119]==79);
    assert(kA75Iso[75]==50 && kA75Iso[85]==100 && kA75Iso[55]==0);
    assert(kG75Jis[34]==137 && kG75Jis[96]==135 && kG75Jis[75]==50);
    std::cout<<"DRUNKDEER_IDENTITY=PASS\n";
}
