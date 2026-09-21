#include "halljoy_wooting_physical.h"
#include "halljoy_uap_provider_v2_projection.h"
#include "provider_v2_controller_shadow.h"
#include "configured_xusb_builder.h"
#include "bindings.h"
#include "block_keys_policy.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace halljoy;
    using namespace wooting_physical;
    using namespace analog_provider_v2;
    constexpr std::array<unsigned char,4> positions{0xa4,0xa8,0xa6,0xad};
    for(unsigned matrix=0;matrix<256;++matrix) {
        int expected=-1;
        for(unsigned i=0;i<4;++i)if(matrix==positions[i])expected=int(i);
        assert(Slot(0x31e3,0x1340,static_cast<unsigned char>(matrix))==expected);
        assert(Slot(0x31e3,0x1320,static_cast<unsigned char>(matrix))==-1);
        assert(Slot(0x0000,0x1340,static_cast<unsigned char>(matrix))==-1);
    }
    assert(Code(4)==0);
    assert(!block_keys::IsWindowsKeyBound(44,[](unsigned){return false;}));
    assert(block_keys::IsWindowsKeyBound(44,[](unsigned code){return code==kLeftSpace;}));
    assert(block_keys::IsWindowsKeyBound(44,[](unsigned code){return code==kRightSpace;}));
    assert(!block_keys::IsWindowsKeyBound(26,[](unsigned code){return code==kLeftSpace;}));
    assert(!block_keys::IsWindowsKeyBound(44,[](unsigned code){return code==kCenterFn;}));
    std::array<HallJoyUapProviderV2::ProjectionKey,4> keys{};
    for(unsigned i=0;i<4;++i) {
        const auto code=Code(i);
        assert(keycode::IsSupported(code) && !keycode::IsStandardHid(code));
        keys[i]={HallJoyUapProviderV2::IdentityFromLegacyCode(code),i};
        assert(keys[i].identity==UapExtendedKey(code));
    }
    std::array<float,4> values{256.0f/1023,768.0f/1023,0.4f,0.9f};
    HallJoyUapProviderV2::ProjectionDevice source{};
    source.descriptor.deviceId=1;source.values=values.data();source.valueCount=4;
    source.sampleTimestampUs=1;
    AnalogSnapshotHeaderV2 header{};AnalogDeviceV2 device{};
    std::array<AnalogSampleV2,4> samples{};
    provider_v2_shadow::RawInputMapV1 raw{};
    auto publish=[&](unsigned generation) {
        assert(HallJoyUapProviderV2::BuildSnapshot(keys.data(),4,&source,1,1,
            {1,generation,generation,1,generation,1},&header,&device,1,samples.data(),4)
            ==HallJoyUapProviderV2::ProjectionError::None);
        assert(provider_v2_shadow::ProjectCapturedSnapshot(header,&device,1,
            samples.data(),4,&raw)==provider_v2_shadow::ProjectionError::None);
        for(unsigned i=0;i<4;++i)assert(raw.owned.test(Code(i)) && raw.values[Code(i)]==values[i]);
    };
    publish(1);
    Bindings_SetTriggerForPad(0,Trigger::LT,kLeftSpace);
    Bindings_SetTriggerForPad(0,Trigger::RT,kRightSpace);
    Bindings_AddButtonHidForPad(0,GameButton::A,kCenterFn);
    Bindings_AddButtonHidForPad(0,GameButton::B,kRightFn);
    assert(Bindings_ButtonHasHidForPad(0,GameButton::A,kCenterFn));
    assert(!Bindings_ButtonHasHidForPad(0,GameButton::A,kRightFn));
    configured_xusb::PadConfiguration configuration{};
    configuration.triggers[0]=Bindings_GetTriggerForPad(0,Trigger::LT);
    configuration.triggers[1]=Bindings_GetTriggerForPad(0,Trigger::RT);
    for(unsigned b=0;b<2;++b)for(unsigned c=0;c<keycode::kMaskChunkCount;++c)
        configuration.buttonMasks[b][c]=Bindings_GetButtonMaskChunkForPad(0,static_cast<GameButton>(b),c);
    configured_xusb::BuilderState state{};
    auto frame=[&] {
        configured_xusb::InputValues input{};input.filtered=raw.values;
        return configured_xusb::BuildReport(configuration,input,state);
    };
    auto both=frame();assert(both.leftTrigger>0 && both.rightTrigger>both.leftTrigger);
    assert((both.buttons&3)==3);
    values[0]=0;values[2]=0;publish(2);
    auto released=frame();assert(released.leftTrigger==0 && released.rightTrigger==both.rightTrigger);
    assert((released.buttons&3)==2);
    values.fill(0);publish(3);auto neutral=frame();
    assert(neutral.leftTrigger==0 && neutral.rightTrigger==0 && neutral.buttons==0);
    // A complete empty/disconnected source replaces the prior captured map.
    header.deviceCount=header.requiredDeviceCount=0;
    header.sampleCount=header.requiredSampleCount=0;
    assert(provider_v2_shadow::ProjectCapturedSnapshot(header,nullptr,0,nullptr,0,&raw)
        ==provider_v2_shadow::ProjectionError::None);
    assert(raw.owned.none());
    std::cout<<"WOOTING_PHYSICAL_KEYS=PASS exact_pid matrix_slots provider_projection independent_depths bindings triggers buttons release disconnect\n";
}
