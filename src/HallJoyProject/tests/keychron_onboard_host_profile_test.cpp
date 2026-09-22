#include "keychron_onboard_host_profile.h"
#include "backend_curve.h"
#include "key_settings.h"
#include "settings.h"
#include "profile_runtime_gate.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <iostream>
using namespace halljoy::k4_onboard;
int main() {
    Settings_SetVirtualGamepadCount(1);
    Settings_SetMouseToStickEnabled(false);
    BindingsSnapshot b{};
    b.axes[0][0]={0x04,0x07};
    b.axes[0][1]={0x16,0x1a};
    b.triggers[0]={0x409,0x404};
    b.buttons[0][0][0x2c/64] |= uint64_t{1}<<(0x2c%64);
    b.buttons[0][1][0x2c/64] |= uint64_t{1}<<(0x2c%64);
    Bindings_Apply(b);
    Settings_SetSnappyJoystick(true); Settings_SetLastKeyPriority(true);
    Settings_SetBlockBoundKeys(true);
    hjo_profile p{};
    assert(CaptureProfile(p)==ProfileResult::Ready);
    assert(p.mapping.axes[0][0]==58 && p.mapping.axes[0][1]==60);
    assert(p.mapping.triggers[0]==106 && p.mapping.triggers[1]==18);
    assert(p.mapping.axes[2][0]==HJO_UNBOUND);
    assert(p.mapping.buttons[101]==3 && p.mapping.flags==7);
    unsigned physical=0;
    for (unsigned i=0;i<HJO_SLOTS;++i) if (kSlotHid[i]) {
        ++physical;
        assert(SlotForHid(kSlotHid[i])==static_cast<int>(i));
        for (unsigned x=0;x<=100;++x) {
            const float raw=x/100.f;
            assert(std::fabs(hjo_curve_apply(&p.curves[i],raw)-
                BackendCurve_ApplyByHid(kSlotHid[i],raw))<=1e-6f);
        }
    }
    assert(physical==100);
    uint8_t wire[HJO_PROFILE_BYTES]; hjo_profile decoded{};
    assert(hjo_profile_encode(wire,sizeof(wire),&p));
    assert(hjo_profile_decode(&decoded,wire,sizeof(wire)));
    assert(decoded.mapping.buttons[101]==3);
    const auto unchanged=p;
    auto rejects=[&](ProfileResult expected) {
        assert(CaptureProfile(p)==expected);
        assert(std::memcmp(&p,&unchanged,sizeof(p))==0);
    };
    Settings_SetVirtualGamepadCount(2); rejects(ProfileResult::MultiplePads);
    Settings_SetVirtualGamepadCount(1);
    Settings_SetMouseToStickEnabled(true); rejects(ProfileResult::Mouse);
    Settings_SetMouseToStickEnabled(false);
    b.axes[0][0].minusHid=0x68; Bindings_Apply(b); rejects(ProfileResult::UnknownKey);
    b.axes[0][0].minusHid=4; b.buttons[0][2][0x68/64]|=uint64_t{1}<<(0x68%64);
    Bindings_Apply(b); rejects(ProfileResult::UnknownKey);
    { halljoy::profile_runtime::CommitLease writer; assert(writer);
      rejects(ProfileResult::Busy); }
    std::cout << "Host profile export, matrix, wire roundtrip and rejection PASS\n";
}
