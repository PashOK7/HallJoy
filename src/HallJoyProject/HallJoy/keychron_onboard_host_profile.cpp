#include "keychron_onboard_host_profile.h"
#include "backend_curve.h"
#include "profile_runtime_gate.h"
#include "settings.h"

namespace halljoy::k4_onboard {
ProfileResult CaptureProfile(hjo_profile& destination) {
    halljoy::profile_runtime::ReadLease lease;
    if (!lease) return ProfileResult::Busy;
    if (Settings_GetVirtualGamepadCount()!=1) return ProfileResult::MultiplePads;
    if (Settings_GetMouseToStickEnabled()) return ProfileResult::Mouse;
    BindingsSnapshot bindings;
    Bindings_Capture(bindings);
    hjo_profile next{};
    next.mapping.flags = (Settings_GetSnappyJoystick()?HJO_SNAP:0) |
        (Settings_GetLastKeyPriority()?HJO_LKP:0) |
        (Settings_GetBlockBoundKeys()?HJO_SUPPRESS:0);
    next.mapping.sensitivity=Settings_GetLastKeyPrioritySensitivity();
    for (unsigned a=0;a<4;++a) {
        const int minus=SlotForHid(bindings.axes[0][a].minusHid);
        const int plus=SlotForHid(bindings.axes[0][a].plusHid);
        if (minus<0 || plus<0) return ProfileResult::UnknownKey;
        next.mapping.axes[a][0]=static_cast<uint8_t>(minus);
        next.mapping.axes[a][1]=static_cast<uint8_t>(plus);
    }
    for (unsigned t=0;t<2;++t) {
        const int slot=SlotForHid(bindings.triggers[0][t]);
        if (slot<0) return ProfileResult::UnknownKey;
        next.mapping.triggers[t]=static_cast<uint8_t>(slot);
    }
    for (unsigned b=0;b<15;++b) {
        for (unsigned hid=1;hid<halljoy::keycode::kCount;++hid) {
            if (!(bindings.buttons[0][b][hid/64] & (uint64_t{1}<<(hid%64)))) continue;
            const int slot=SlotForHid(static_cast<uint16_t>(hid));
            if (slot<0) return ProfileResult::UnknownKey;
            next.mapping.buttons[slot] |= static_cast<uint16_t>(1u<<b);
        }
    }
    for (unsigned i=0;i<HJO_SLOTS;++i) {
        BackendCurve_ExportPrepared(kSlotHid[i], next.curves[i]);
        if (!hjo_curve_valid(&next.curves[i])) return ProfileResult::Invalid;
    }
    if (!hjo_mapping_valid(&next.mapping)) return ProfileResult::Invalid;
    destination=next;
    return ProfileResult::Ready;
}
}
