#pragma once
#include "analog_observation_snapshot.h"

namespace halljoy::observation {
// Encoder at the isolated host's validated publication boundary. No state or
// scanning is created until capture is requested. It cannot expose SDK-internal
// samples that the SDK did not return to this host.
class StreamEncoder {
    struct Device {
        std::uint64_t identity=0,connection=1;
        unsigned instance=0;
        bool online=false,seen=false;
        SnapshotValues values{};
        SnapshotValidity valid;
    };
    std::array<Device,16> devices_{};
public:
    template<class Visit,class Emit,class Loss> void Frame(Visit visit,Emit emit,Loss loss) {
        for(auto& d:devices_) d.seen=false;
        visit([&](std::uint64_t identity,const SnapshotValues& values,std::uint64_t timeUs,
                  unsigned instance=0,const SnapshotValidity* validity=nullptr) {
            if(!identity) {loss();return;}
            unsigned index=0;
            for(;index<devices_.size();++index)
                if(devices_[index].identity==identity && devices_[index].instance==instance) break;
            if(index==devices_.size()) {
                for(index=0;index<devices_.size();++index) if(!devices_[index].online && !devices_[index].seen) break;
                if(index==devices_.size()) {loss();return;}
            }
            auto& d=devices_[index];
            if(d.seen) {loss();return;}
            if(d.identity!=identity || d.instance!=instance) {++d.connection;d.online=false;d.identity=identity;d.instance=instance;}
            for(unsigned code=1;code<values.size();++code) {
                const bool valid=validity ? validity->test(code) : true;
                const bool previousValid=d.online && d.valid.test(code);
                if(valid==previousValid && (!valid || values[code]==d.values[code])) continue;
                Event e{};e.source=index+1;e.connection=d.connection;
                e.physical=e.hid=static_cast<std::uint16_t>(code);e.milli=values[code];
                e.previous=d.values[code];e.previousValid=previousValid;e.available=valid;
                e.timeUs=timeUs;e.timing=Timing::HostSnapshot;e.keySpace=KeySpace::BindableCode;emit(e);
            }
            d.values=values;if(validity) d.valid=*validity;else d.valid.set();
            d.online=true;d.seen=true;
        });
        for(unsigned i=0;i<devices_.size();++i) {
            auto& d=devices_[i];if(!d.online || d.seen) continue;
            Event e{};e.source=i+1;e.connection=d.connection;e.kind=Kind::Disconnect;
            e.timing=Timing::HostSnapshot;emit(e);++d.connection;d.online=false;
        }
    }
};
}
