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
        (Settings_GetBlockBoundKeys()?HJO_SUPPRESS:0) |
        // Honoured only by firmware advertising HJO_CAP_KEEP_ALT_TAB; the
        // session strips it for older firmware, which would reject the profile.
        (Settings_GetBlockBoundKeys() && Settings_GetBlockKeysAllowAltTab()?HJO_KEEP_ALT_TAB:0);
    next.mapping.sensitivity=Settings_GetLastKeyPrioritySensitivity();
    // Every key of a stick direction (up to HJO_AXIS_KEYS) maps to its firmware
    // slot. Empty entries are HJO_UNBOUND; the firmware combines them as the most
    // pressed key, exactly like the host builder.
    for (unsigned a=0;a<4;++a) {
        for (unsigned side=0;side<2;++side) {
            const auto& keys = side==0 ? bindings.axes[0][a].minusHids : bindings.axes[0][a].plusHids;
            for (unsigned k=0;k<HJO_AXIS_KEYS;++k) {
                const int slot=SlotForHid(keys[k]);
                if (slot<0) return ProfileResult::UnknownKey;
                next.mapping.axes[a][side][k]=static_cast<uint8_t>(slot);
            }
        }
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
