#include "../HallJoy/native_analog_backend.h"

#include <cassert>

namespace
{
bool Start() { return true; }
bool Stop() { return true; }
bool Present() { return true; }
bool Connected() { return true; }
bool Owns(std::uint16_t hid) { return hid == 0x1A; }
std::uint16_t Read(std::uint16_t hid) { return hid == 0x1A ? 500 : 0; }
void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (out) out->connected = true;
}
}

int main()
{
    NativeAnalogBackendDescriptor valid{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "test",
        L"Test backend",
        NativeAnalogProtocol::Addressed09402,
        NativeAnalogStartPhase::AfterRealtime,
        NativeAnalogBackendFlag_ReadOnlyProbe,
        nullptr,
        &Start,
        &Stop,
        nullptr,
        &Present,
        &Connected,
        &Owns,
        &Read,
        &Telemetry,
    };
    assert(NativeAnalogBackendDescriptor_IsValid(valid));
    assert(valid.ownsHid(0x1A));
    assert(valid.getMilli(0x1A) == 500);

    auto invalid = valid;
    invalid.id = "";
    assert(!NativeAnalogBackendDescriptor_IsValid(invalid));
    invalid = valid;
    invalid.stop = nullptr;
    assert(!NativeAnalogBackendDescriptor_IsValid(invalid));
    invalid = valid;
    invalid.abiVersion += 1;
    assert(!NativeAnalogBackendDescriptor_IsValid(invalid));
    invalid = valid;
    invalid.startPhase = static_cast<NativeAnalogStartPhase>(0);
    assert(!NativeAnalogBackendDescriptor_IsValid(invalid));
    invalid = valid;
    invalid.flags = 0x80000000u;
    assert(!NativeAnalogBackendDescriptor_IsValid(invalid));
    return 0;
}
