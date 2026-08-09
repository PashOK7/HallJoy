#!/usr/bin/env python3
"""Generate and register a safe HallJoy native analogue backend skeleton.

The generated module compiles but claims no devices and sends no HID commands
until the author implements a documented capability proof and worker.
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

MSBUILD_NS = "http://schemas.microsoft.com/developer/msbuild/2003"
ET.register_namespace("", MSBUILD_NS)


def q(tag: str) -> str:
    return f"{{{MSBUILD_NS}}}{tag}"


def patch_project(path: Path, tag: str, include: str, filter_name: str | None = None) -> None:
    tree = ET.parse(path)
    root = tree.getroot()
    if any(node.get("Include") == include for node in root.iter(q(tag))):
        return
    groups = [group for group in root.findall(q("ItemGroup")) if group.find(q(tag)) is not None]
    group = groups[-1] if groups else ET.SubElement(root, q("ItemGroup"))
    node = ET.SubElement(group, q(tag), Include=include)
    if filter_name:
        ET.SubElement(node, q("Filter")).text = filter_name
    tree.write(path, encoding="utf-8", xml_declaration=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--slug", required=True, help="lowercase file/namespace slug, e.g. foo_matrix")
    parser.add_argument("--prefix", required=True, help="C++ public prefix, e.g. FooMatrix")
    parser.add_argument("--enum", required=True, help="NativeAnalogProtocol enum member")
    parser.add_argument("--protocol-value", required=True, type=int, help="unused numeric enum value")
    parser.add_argument("--display-name", required=True)
    parser.add_argument(
        "--start-phase",
        choices=["BeforeUap", "AfterRealtime", "AfterRawInput"],
        default="AfterRealtime",
    )
    args = parser.parse_args()

    if not re.fullmatch(r"[a-z][a-z0-9_]*", args.slug):
        parser.error("--slug must match [a-z][a-z0-9_]*")
    if not re.fullmatch(r"[A-Z][A-Za-z0-9]*", args.prefix):
        parser.error("--prefix must be a C++ identifier beginning with uppercase")
    if not re.fullmatch(r"[A-Z][A-Za-z0-9]*", args.enum):
        parser.error("--enum must be a C++ enum identifier")
    if not (1 <= args.protocol_value <= 255):
        parser.error("--protocol-value must be 1..255")

    root = Path(__file__).resolve().parents[1]
    source = root / "src" / "HallJoyProject" / "HallJoy"
    tests = root / "src" / "HallJoyProject" / "tests"
    docs = root / "docs" / "protocols"

    backend_header = source / f"{args.slug}_backend.h"
    backend_cpp = source / f"{args.slug}_backend.cpp"
    protocol_header = source / f"{args.slug}_protocol.h"
    protocol_cpp = source / f"{args.slug}_protocol.cpp"
    test = tests / f"{args.slug}_protocol_test.cpp"
    protocol_doc = docs / f"{args.slug.upper()}_PROTOCOL.md"
    generated = (backend_header, backend_cpp, protocol_header, protocol_cpp, test, protocol_doc)
    for path in generated:
        if path.exists():
            raise SystemExit(f"Refusing to overwrite: {path}")

    routing = source / "native_analog_routing.h"
    routing_text = routing.read_text(encoding="utf-8-sig")
    if re.search(rf"\b{re.escape(args.enum)}\b", routing_text):
        raise SystemExit(f"Protocol enum already exists: {args.enum}")
    if re.search(rf"=\s*{args.protocol_value}\s*,", routing_text):
        raise SystemExit(f"Protocol value already used: {args.protocol_value}")
    anchor = "};\n\n// Central startup-time ownership registry"
    if anchor not in routing_text:
        raise SystemExit("NativeAnalogProtocol enum anchor was not found")
    routing.write_text(
        routing_text.replace(
            anchor,
            f"    {args.enum} = {args.protocol_value},\n}};\n\n// Central startup-time ownership registry",
            1,
        ),
        encoding="utf-8",
    )

    protocol_header.write_text(
        f'''#pragma once

#include <cstdint>
#include <span>

namespace {args.slug}
{{
struct CapabilityResponse
{{
    // TODO: replace with protocol-specific capability fields.
    std::uint16_t rawMinimum = 0;
    std::uint16_t rawMaximum = 0;
}};

// Pure parser: no HID handles, logging, global state or Windows APIs.
bool ParseCapabilityResponse(std::span<const std::uint8_t> packet, CapabilityResponse* out);
std::uint16_t NormalizeRawToMilli(
    std::uint16_t raw,
    std::uint16_t minimum,
    std::uint16_t maximum);
}}
''',
        encoding="utf-8",
    )

    protocol_cpp.write_text(
        f'''#include "{args.slug}_protocol.h"

#include <cstdint>

namespace {args.slug}
{{
bool ParseCapabilityResponse(std::span<const std::uint8_t> packet, CapabilityResponse* out)
{{
    (void)packet;
    if (out) *out = CapabilityResponse{{}};
    // TODO: validate report ID, command echo, length, checksum and semantics.
    return false;
}}

std::uint16_t NormalizeRawToMilli(
    std::uint16_t raw,
    std::uint16_t minimum,
    std::uint16_t maximum)
{{
    if (maximum <= minimum || raw <= minimum) return 0;
    if (raw >= maximum) return 1000;
    const std::uint32_t numerator = static_cast<std::uint32_t>(raw - minimum) * 1000u;
    return static_cast<std::uint16_t>(numerator / (maximum - minimum));
}}
}}
''',
        encoding="utf-8",
    )

    backend_header.write_text(
        f'''#pragma once

#include <cstdint>

#include "native_analog_backend.h"

bool {args.prefix}_PrepareProtocolRouting();
bool {args.prefix}_Start();
void {args.prefix}_Stop();
void {args.prefix}_NotifyDeviceChange();
bool {args.prefix}_IsProtocolDevicePresent();
bool {args.prefix}_IsConnected();
bool {args.prefix}_OwnsHid(std::uint16_t hidUsage);
std::uint16_t {args.prefix}_GetMilli(std::uint16_t hidUsage);
const NativeAnalogBackendDescriptor& {args.prefix}_GetNativeBackendDescriptor();
''',
        encoding="utf-8",
    )

    backend_cpp.write_text(
        f'''#include "{args.slug}_backend.h"

#include "{args.slug}_protocol.h"
#include "native_analog_routing.h"
#include "realtime_loop.h"

#include <array>
#include <atomic>
#include <cwchar>

namespace
{{
std::atomic<bool> g_present{{ false }};
std::atomic<bool> g_connected{{ false }};
std::array<std::atomic<std::uint16_t>, 256> g_milli{{}};

void FillTelemetry(NativeAnalogBackendTelemetry* out)
{{
    if (!out) return;
    *out = NativeAnalogBackendTelemetry{{}};
    out->present = g_present.load(std::memory_order_acquire);
    out->connected = g_connected.load(std::memory_order_acquire);
    // TODO: publish exact VID/PID, HID fingerprint, report sizes, update rate,
    // mapped/active keys and protocol-specific status.
    _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
        L"scaffold only: capability proof not implemented");
}}
}}

bool {args.prefix}_PrepareProtocolRouting()
{{
    // REQUIRED: enumerate only plausible HID candidates and perform a documented
    // read-only or fully reversible capability proof. Call NativeAnalogRouting_Claim
    // only after response framing, semantics and requested IDs are validated.
    return false;
}}

bool {args.prefix}_Start()
{{
    // REQUIRED: open only a VID/PID already claimed by this exact protocol.
    return false;
}}

void {args.prefix}_Stop()
{{
    g_connected.store(false, std::memory_order_release);
    for (auto& value : g_milli) value.store(0, std::memory_order_relaxed);
    RealtimeLoop_NotifyInputChanged();
}}

void {args.prefix}_NotifyDeviceChange() {{}}
bool {args.prefix}_IsProtocolDevicePresent() {{ return g_present.load(std::memory_order_acquire); }}
bool {args.prefix}_IsConnected() {{ return g_connected.load(std::memory_order_acquire); }}
bool {args.prefix}_OwnsHid(std::uint16_t hidUsage) {{ (void)hidUsage; return false; }}
std::uint16_t {args.prefix}_GetMilli(std::uint16_t hidUsage)
{{
    return hidUsage < g_milli.size() ? g_milli[hidUsage].load(std::memory_order_relaxed) : 0;
}}

const NativeAnalogBackendDescriptor& {args.prefix}_GetNativeBackendDescriptor()
{{
    static const NativeAnalogBackendDescriptor descriptor{{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "{args.slug.replace('_', '-')}",
        L"{args.display_name}",
        NativeAnalogProtocol::{args.enum},
        NativeAnalogStartPhase::{args.start_phase},
        NativeAnalogBackendFlag_None,
        &{args.prefix}_PrepareProtocolRouting,
        &{args.prefix}_Start,
        [] {{ {args.prefix}_Stop(); return true; }},
        &{args.prefix}_NotifyDeviceChange,
        &{args.prefix}_IsProtocolDevicePresent,
        &{args.prefix}_IsConnected,
        &{args.prefix}_OwnsHid,
        &{args.prefix}_GetMilli,
        &FillTelemetry,
    }};
    return descriptor;
}}
''',
        encoding="utf-8",
    )

    test.parent.mkdir(parents=True, exist_ok=True)
    test.write_text(
        f'''#include "../HallJoy/{args.slug}_protocol.h"

#include <array>
#include <cassert>

int main()
{{
    {args.slug}::CapabilityResponse proof{{}};
    const std::array<std::uint8_t, 1> malformed{{ 0 }};
    assert(!{args.slug}::ParseCapabilityResponse(malformed, &proof));
    assert({args.slug}::NormalizeRawToMilli(0, 0, 100) == 0);
    assert({args.slug}::NormalizeRawToMilli(50, 0, 100) == 500);
    assert({args.slug}::NormalizeRawToMilli(100, 0, 100) == 1000);
    return 0;
}}
''',
        encoding="utf-8",
    )

    docs.mkdir(parents=True, exist_ok=True)
    protocol_doc.write_text(
        f'''# {args.display_name} protocol

## Evidence

- Device models and firmware versions:
- VID/PID and HID interfaces:
- Source of captures/specification:

## Safe capability proof

Document every request byte, response invariant and why the operation is read-only
or completely reversible. A brand name, VID/PID or report length alone is not proof.

## Transport

- Start phase: `{args.start_phase}`
- Stream or polled:
- Report sizes:
- Timeout/reconnect rules:

## Analogue semantics

- Key/slot to HID mapping:
- Raw range and direction:
- Dead zones/calibration:
- Freshness/release behavior:

## Safety allow-list

List the exact runtime commands. Explicitly list known write/flash/calibration
commands that HallJoy must never send.

## Test evidence

- Parser fixtures:
- Hardware logs:
- Multi-device/UAP arbitration:
- Disconnect/reconnect:
''',
        encoding="utf-8",
    )

    catalog = source / "native_analog_backends.def"
    catalog_text = catalog.read_text(encoding="utf-8")
    entry = f"HALLJOY_NATIVE_BACKEND({args.prefix}_GetNativeBackendDescriptor)\n"
    if entry not in catalog_text:
        catalog.write_text(catalog_text.rstrip() + "\n" + entry, encoding="utf-8")

    project = source / "HallJoy.vcxproj"
    filters = source / "HallJoy.vcxproj.filters"
    patch_project(project, "ClInclude", backend_header.name)
    patch_project(project, "ClInclude", protocol_header.name)
    patch_project(project, "ClCompile", backend_cpp.name)
    patch_project(project, "ClCompile", protocol_cpp.name)
    patch_project(filters, "ClInclude", backend_header.name, "Header Files")
    patch_project(filters, "ClInclude", protocol_header.name, "Header Files")
    patch_project(filters, "ClCompile", backend_cpp.name, "Source Files")
    patch_project(filters, "ClCompile", protocol_cpp.name, "Source Files")

    for path in generated:
        print(f"Created {path.relative_to(root)}")
    print("Next: implement capability proof/worker, complete parser fixtures, then run:")
    print("  python tools/run_native_backend_checks.py")
    print("  BUILD.cmd")
    return 0


if __name__ == "__main__":
    sys.exit(main())
