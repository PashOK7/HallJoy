// Explicit hardware/lifecycle probe: production worker, neutral bindings, no UI.
#include "keychron_onboard_backend.h"
#include "settings.h"
#include "bindings.h"
#include <windows.h>
#include <iostream>
#include <cstring>
void RealtimeLoop_NotifyInputChanged() {} // no realtime consumer in headless probe
int main() {
    BindingsSnapshot empty{};Bindings_Apply(empty);
    Settings_SetVirtualGamepadCount(1);Settings_SetMouseToStickEnabled(false);
    Settings_SetVirtualGamepadsEnabled(true);Settings_SetBlockBoundKeys(false);
    const auto& d=KeychronOnboard_GetNativeBackendDescriptor();
    if(!d.prepareRouting() || !KeychronOnboard_OwnsOutput()) return 2;
    if(!d.start()) return 3;
    KeychronOnboard_SetAdmission(true);
    bool active=false,pad=false;uint64_t updates=0;
    for(unsigned i=0;i<80;++i) {
        KeychronOnboard_MonitorVisible(true);Sleep(100);
        NativeAnalogBackendTelemetry t{};d.getTelemetry(&t);updates=t.successfulUpdates;
        if(wcsstr(t.status,L": active")) active=true;
        uint8_t report[20]{};KeychronOnboard_CopyPad(report,20);
        if(report[1]==20) {
            bool neutral=true;for(unsigned j=2;j<20;++j)neutral=neutral && report[j]==0;
            if(!neutral) {d.stop(halljoy::lifecycle::GenerationId{1});return 4;}
            pad=true;
        }
        if(i==40) Settings_SetSnappyJoystick(!Settings_GetSnappyJoystick());
    }
    KeychronOnboard_SetAdmission(false);
    const auto stopped=d.stop(halljoy::lifecycle::GenerationId{1});
    std::cout<<"active="<<active<<" actual_neutral_report="<<pad<<" telemetry_updates="<<updates
        <<" stop_status="<<unsigned(stopped.status)<<" reserved_after="<<KeychronOnboard_OwnsOutput()<<std::endl;
    return active && pad && updates>5 && stopped.status==halljoy::lifecycle::StopStatus::Joined && !KeychronOnboard_OwnsOutput()?0:5;
}
