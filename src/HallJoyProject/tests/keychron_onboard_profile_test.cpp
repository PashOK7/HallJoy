#include "../HallJoy/keychron_onboard_profile.h"
#include <array>
#include <cassert>
#include <iostream>

int main() {
    hjo_profile source{}, target{};
    source.mapping.sensitivity=.02f;
    memset(source.mapping.axes,HJO_UNBOUND,sizeof(source.mapping.axes));
    memset(source.mapping.triggers,HJO_UNBOUND,sizeof(source.mapping.triggers));
    source.mapping.axes[0][0]=0; source.mapping.axes[0][1]=113;
    source.mapping.buttons[113]=0x4001;
    for(auto &c:source.curves) {
        c.x[0]=0; c.x[1]=.3f; c.x[2]=.7f; c.x[3]=1;
        memcpy(c.y,c.x,sizeof(c.x)); c.weight[0]=c.weight[1]=1;
    }
    std::array<uint8_t,HJO_PROFILE_BYTES> wire{};
    assert(hjo_profile_encode(wire.data(),wire.size(),&source));
    assert(hjo_profile_decode(&target,wire.data(),wire.size()));
    assert(target.mapping.axes[0][1]==113 && target.mapping.buttons[113]==0x4001);
    assert(hjo_curve_apply(&target.curves[113],.5f)==hjo_curve_apply(&source.curves[113],.5f));
    const auto original=target;
    for(size_t i=0;i<wire.size();++i) {
        auto corrupt=wire; corrupt[i]^=0x01;
        assert(!hjo_profile_decode(&target,corrupt.data(),corrupt.size()));
        assert(memcmp(&original,&target,sizeof(target))==0);
        assert(!hjo_profile_decode(&target,wire.data(),i));
    }
    for(unsigned offset:{6u,7u,12u,22u,253u,292u,293u}) {
        auto corrupt=wire; corrupt[offset]=0xff;
        hjk4_put32(corrupt.data()+5040,hjk4_crc32(corrupt.data(),5040));
        // offset 253 turns x0 into an out-of-range/non-monotonic float.
        if(offset==253) hjk4_put32(corrupt.data()+252,0x7fc00000u);
        hjk4_put32(corrupt.data()+5040,hjk4_crc32(corrupt.data(),5040));
        if(offset==12) corrupt[12]=114;
        hjk4_put32(corrupt.data()+5040,hjk4_crc32(corrupt.data(),5040));
        assert(!hjo_profile_decode(&target,corrupt.data(),corrupt.size()));
        assert(memcmp(&original,&target,sizeof(target))==0);
    }
    std::cout << "onboard profile corruption/truncation/atomic decode PASS\n";
}
