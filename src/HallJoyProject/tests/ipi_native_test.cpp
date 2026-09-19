#include "../HallJoy/ipi_protocol.h"
#include "../HallJoy/generated/ipi_models.h"
#include "../HallJoy/generated/layout_pipeline/identities.h"
#include <cassert>
#include <iostream>
#include <set>
#include <thread>

static ipi::Frame Response(unsigned cmd, unsigned sub, const std::uint8_t* ids, std::size_t n) {
    ipi::Frame f{}; f[0]=9; f[1]=cmd; f[2]=sub; f[4]=1; f[6]=n*6;
    for (std::size_t i=0;i<n;++i) f[8+i*6]=ids[i];
    ipi::Finish(f); return f;
}
static void Value32(ipi::Frame& f, unsigned i, std::uint32_t v) {
    for (unsigned j=0;j<4;++j) f[9+i*6+j]=std::uint8_t(v>>(24-j*8));
    ipi::Finish(f);
}
int main() {
    std::set<std::uint64_t> uuids, tokens;
    for (const auto& model : ipi::models) {
        assert(uuids.insert(model.uuid).second && ipi::FindModel(model.uuid)==&model);
        const auto token=halljoy::layout_identity::Token("ipi-addressed",model.product);
        assert(token && halljoy::layout_identity::Match(token));tokens.insert(token);
        std::set<unsigned> ids(model.ids,model.ids+model.count);
        assert(ids.size()==model.count && *ids.begin()>0 && *ids.rbegin()<256);
    }
    assert(uuids.size()==8 && tokens.size()==4 && !ipi::FindModel(0x11000000005Bull));
    auto uuid=ipi::UuidRequest();assert(ipi::Header(uuid,0x82,1,6));
    const std::uint64_t wanted=0x11000000002Cull;
    for (unsigned i=0;i<6;++i) uuid[7+i]=std::uint8_t(wanted>>(40-8*i));
    ipi::Finish(uuid);assert(ipi::ParseUuid(uuid)==wanted);
    uuid[12]^=1;assert(!ipi::ParseUuid(uuid));
    const std::uint8_t ids[]={30,43,44,45,66,69,71,72,73};
    ipi::Frame request{};assert(ipi::Request(0x94,2,ids,9,request));
    assert(request[6]==18 && request[7]==0 && request[8]==30 && request[24]==73);
    const auto before=request;
    assert(!ipi::Request(0x94,0,ids,9,request) && request==before);
    assert(!ipi::Request(0x94,4,ids,9,request) && !ipi::Request(0x83,0,ids,0,request));
    assert(!ipi::Request(0x83,0,ids,10,request));
    const std::uint8_t duplicates[]={30,30};assert(!ipi::Request(0x83,0,duplicates,2,request));
    std::array<std::uint16_t,256> mapping{};
    auto map=Response(0x83,0,ids,9);
    const std::uint32_t codes[]={26,4,22,7,0x00200000,0x00040000,0x00400000,0x0D000000,0};
    for(unsigned i=0;i<9;++i)Value32(map,i,codes[i]);
    assert(ipi::Map(map,ids,9,mapping));
    assert(mapping[30]==26 && mapping[66]==229 && mapping[69]==226 && mapping[71]==230 && mapping[72]==0x409 && mapping[73]==0);
    assert(ipi::Hid(0x01000004)==0 && ipi::Hid(0x00030000)==0 && ipi::Hid(0x00040004)==0);
    const auto saved=mapping;auto bad=map;bad[8]=43;ipi::Finish(bad);
    assert(!ipi::Map(bad,ids,9,mapping) && mapping==saved);
    bad=map;bad[7]=1;ipi::Finish(bad);assert(!ipi::Map(bad,ids,9,mapping) && mapping==saved);
    bad=map;bad[5]=1;ipi::Finish(bad);assert(!ipi::Map(bad,ids,9,mapping));
    bad=map;bad[11]^=1;assert(!ipi::Map(bad,ids,9,mapping));
    std::array<ipi::Calibration,256> calibration{};
    auto cal=Response(0x94,5,ids,9);
    for(unsigned i=0;i<9;++i)Value32(cal,i,(std::uint32_t(10000+i*100)<<16)|(2000+i*100));
    assert(ipi::Calibrations(cal,ids,9,calibration));
    for(unsigned i=0;i<9;++i) {
        const auto c=calibration[ids[i]];
        assert(ipi::Normalise(c.released,c)==0 && ipi::Normalise(c.bottom,c)==1000);
        assert(ipi::Normalise(c.bottom+4000,c)==500 && ipi::Normalise(c.released+10,c)==0);
        assert(ipi::Normalise(c.bottom-100,c)==1000 && ipi::Normalise(0,c)==0);
    }
    bad=cal;Value32(bad,8,(100u<<16)|200u);
    assert(!ipi::Calibrations(bad,ids,9,calibration) && calibration[30].released==10000 && calibration[73].released==10800);
    auto sample=Response(0x94,2,ids,9);
    for(unsigned i=0;i<9;++i)Value32(sample,i,(std::uint32_t(0x8000|8400)<<16)|1234);
    std::array<ipi::Sample,9> samples{};assert(ipi::Samples(sample,ids,9,samples));
    assert(samples[0].raw==8400 && samples[0].pressed && samples[0].secondary==1234 && samples[8].id==73);
    bad=sample;Value32(bad,8,0);assert(!ipi::Samples(bad,ids,9,samples) && samples[8].raw==8400);
    ipi::Publication pub;
    assert(pub.Bind(30,26) && pub.Bind(43,26) && pub.Bind(72,0x409));
    assert(!pub.Bind(30,4) && !pub.Bind(0,4) && !pub.Bind(1,0xFFFF));
    pub.Publish(30,800,1000);pub.Publish(43,500,1100);pub.Publish(72,700,1100);
    assert(pub.Read(26,1100).milli==800 && pub.Read(0x409,1100).milli==700);
    pub.Publish(43,0,1200);assert(pub.Read(26,1200).milli==800);
    pub.Publish(43,400,1450);assert(pub.Read(26,1550).milli==400);
    assert(!pub.Read(26,2000).fresh && !pub.Read(26,999).fresh);
    std::thread writer([&]{for(unsigned i=0;i<10000;++i)pub.Publish(30,i%1001,3000+i);});
    for(unsigned i=0;i<10000;++i)assert(pub.Read(26,3000+i).milli<=1000);
    writer.join();assert(pub.Clear());assert(!pub.Read(26,13000).fresh && pub.Active(13000)==0);
    std::cout<<"IPI_NATIVE_TEST=PASS models=8 exact_maps=1 calibration=1 framing=1 aliases=1 fn=1 freshness=1\n";
}
