#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]
header = (HALL / "analog_provider_v2.h").read_text(encoding="utf-8")
contract = (REPO / "third_party" / "UniversalAnalogPluginFixed" /
            "halljoy_analog_provider_v2_contract.h").read_text(encoding="utf-8")
impl = (HALL / "analog_provider_v2.cpp").read_text(encoding="utf-8")
test = (ROOT / "tests" / "analog_provider_v2_test.cpp").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")

required_header = (
    "KeyIdentityV1",
    "KeyNamespace::UsbHidUsage",
    "KeyNamespace::UapExtended",
    "KeyNamespace::HallJoySemantic",
    "AnalogSnapshotHeaderV2",
    "providerGeneration",
    "sampleGeneration",
    "valueGeneration",
    "ownershipGeneration",
    "requiredDeviceCount",
    "requiredSampleCount",
    "AnalogValueFlag_LegacyQuantized",
)
for marker in required_header:
    assert marker in header + contract, marker

assert "std::uint8_t usage" not in header + contract
assert "milli" not in (header + contract).lower().replace("legacyquantized", "") or "ValueFromLegacyMilli" in header
assert '#include "halljoy_analog_provider_v2_contract.h"' in header
assert "AnalogSnapshotFlag_Complete" in impl
assert "AnalogSnapshotFlag_Truncated" in impl
assert "DuplicateSample" in impl
assert "RequiresRealtimeWake" in impl

for marker in (
    "UsbHidKey(0x07, 0xf0)",
    "HallJoySemanticKey(1, 0xf0)",
    "UsbHidKey(0x0c, 0xcd)",
    "UapExtendedKey(0x409)",
    "ValueFromRaw(1, 4095)",
    "{ 1u, 8u, 16u, 32u }",
    "authoritative release",
    "ProviderGenerationRegressed",
):
    assert marker in test, marker

assert '<ClInclude Include="analog_provider_v2.h"' in project
assert '<ClCompile Include="analog_provider_v2.cpp"' in project
print("ANALOG_PROVIDER_V2_STATIC_AUDIT=PASS")
