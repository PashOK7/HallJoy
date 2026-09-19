#include "native_layout_state.h"
#include "native_layout_devices.h"
#include "physical_analog_state.h"
#include "irok_na87_factory.h"
#include "generated/ipi_models.h"
#include <cassert>
#include <set>
#include <thread>
#include <iostream>
int main() {
    halljoy::layout_devices::Containers devices;
    std::array<unsigned char,16> first{},second{};first[0]=1;second[0]=2;
    assert(devices.Result(true)==0 && devices.Result(false)==-2);
    devices.Add(first);devices.Add(first);assert(devices.Result(true)==1);
    devices.Missing();assert(devices.Result(true)==-2);
    devices.Add(second);assert(devices.Result(false)==2);
    using namespace halljoy::native_layout;
    Key keys[]={{4,26},{26,4},{7,4},{22,0}};
    assert(enabled && Publish(1,keys,4) && Publish(2,keys,4));
    Clear(1);keys[0].assigned=8;
    assert(Publish(2,keys,4) && Read(2).keys[0].assigned==8);
    unsigned slots=0;for (const auto& slot:sources) if(slot.token==2) ++slots;
    assert(slots==1);
    const auto before=Read(2).revision;
    Key duplicate[]={{4,5},{4,6}};assert(!Publish(2,duplicate,2) && Read(2).revision==before);
    assert(!Publish(0,keys,4) && !Publish(2,nullptr,4) && !Publish(2,keys,257));
    activeToken=2;assert(UsesRemapping(2));enabled=false;assert(!UsesRemapping(2));enabled=true;
    std::thread writer([&]{for(int i=0;i<2000;++i)assert(Publish(2,keys,4));});
    for(int i=0;i<2000;++i) {auto s=Read(2);assert(s.complete && s.count==4 && s.keys[0].assigned==8);}
    writer.join();Clear(2);assert(!UsesRemapping(2) && !Read(2).complete);
    for(const auto& model:ipi::models) {
        std::set<unsigned> factory;
        for(std::size_t i=0;i<model.count;++i) {
            const auto hid=ipi::factoryHids[model.ids[i]];
            assert(hid && factory.insert(hid).second);
        }
    }
    std::set<unsigned> factory;
    for(auto hid:irok_na87::FactoryMap()) if(hid)assert(factory.insert(hid).second);
    assert(factory.size()==90);
    halljoy::physical_analog::Publication values;
    assert(values.Bind(1,4) && values.Bind(2,4) && values.Bind(3,26));
    values.Publish(1,800,100);values.Publish(2,600,101);
    assert(values.Read(4,101).milli==800);
    values.Publish(1,0,102);assert(values.Read(4,102).milli==600);
    assert(!values.Read(4,1000).fresh && values.Read(4,1000,~std::uint64_t{0}).milli==600);
    values.Publish(2,0,1001);assert(values.Read(4,1001).milli==0);
    assert(!values.Bind(2,5));values.Clear();assert(!values.Read(4,1001).fresh);
    std::cout << "NATIVE_LAYOUT_STATE_TEST=PASS\n";
}
