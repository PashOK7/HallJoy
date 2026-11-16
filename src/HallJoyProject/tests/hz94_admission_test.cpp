#include "../HallJoy/irok_hz94_admission_cases.h"
#include "../HallJoy/irok_na87_protocol.h"
#include <cassert>
#include <iostream>
int main() {
    for (unsigned scenario=0;scenario<irok_hz94::kAdmissionCaseCount;++scenario) {
        const auto raw=irok_hz94::AdmissionCase(scenario);
        irok_na87::MapReader reader(true,true);
        for (unsigned row=0;row<6;++row) {
            auto packet=irok_na87::MapRequest();packet[3]=row==5?255:0;
            packet[4]=row==5?255:static_cast<std::uint8_t>(row+1);packet[5]=22;
            std::copy_n(raw.begin()+row*22,22,packet.begin()+6);
            assert(reader.Feed(packet));assert(reader.Complete()==(row==5));
        }
        const auto filtered=irok_hz94::PhysicalAssignments(reader.Map());
        for (unsigned pos=0;pos<filtered.size();++pos) {
            if (!irok_hz94::kFactoryMap[pos]) assert(filtered[pos]==0);
            else assert(filtered[pos]==0 || (filtered[pos]>=4 && filtered[pos]<=0xe7) || filtered[pos]==0xfa);
        }
        const auto mask=irok_nd75::SubscriptionMask(irok_hz94::kFactoryMap);
        unsigned channels=0;for(auto rows:mask)for(unsigned bit=0;bit<6;++bit)channels+=(rows>>bit)&1;
        assert(channels==94);
        if (scenario==1) {
            const auto evidence=irok_hz94::InspectMap(raw);
            assert(evidence.assigned==104 && evidence.factory==94 && evidence.outside==10);
            assert(filtered==irok_hz94::kFactoryMap);
        }
    }
    // Count and assignment bytes are metadata only after exact HZ94 identity proof.
    for (unsigned count=0;count<=132;++count) {
        irok_na87::MapReader reader(true,true);
        for (unsigned row=0;row<6;++row) {
            auto packet=irok_na87::MapRequest();packet[3]=row==5?255:0;
            packet[4]=row==5?255:static_cast<std::uint8_t>(row+1);packet[5]=22;
            for (unsigned col=0;col<22;++col) if(row*22+col<count) packet[6+col]=255;
            assert(reader.Feed(packet));
        }
        assert(reader.Complete());
    }
    // Structural bounds and inconsistent rows invalidate aliases, not the physical sensor route.
    irok_na87::MapReader corrupt(true,true);
    auto packet=irok_na87::MapRequest();packet[4]=1;packet[5]=22;
    assert(corrupt.Feed(packet));packet[6]=4;assert(corrupt.Feed(packet));
    assert(corrupt.Conflict() && !corrupt.Complete());
    irok_na87::MapReader incomplete(true,true);packet[4]=6;assert(!incomplete.Feed(packet));
    assert(!incomplete.Complete());
    irok_nd75::DeviceInfo identity{};
    std::strcpy(identity.product.data(),"7187_AUSXHERGB");assert(irok_hz94::Matches(identity));
    std::strcpy(identity.product.data(),"7186_AUSHERGB");assert(!irok_hz94::Matches(identity));
    std::cout<<"HZ94_ADMISSION_TEST=PASS log38=104_94_10 variants=8 counts=0..132 physical_channels=94 invalid_metadata=2 foreign_identity=1\n";
}
