#!/usr/bin/env python3
"""Exercise the private ABI1 null/state/bounded-unload contract."""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import math
import os
from pathlib import Path
import time


def assert_no_halljoy_processes() -> None:
    """Fail closed before the DLL can open a physical analogue HID interface."""
    if os.name != "nt":
        raise RuntimeError("physical UAP isolation can only be proved on Windows")

    class ProcessEntry32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD),
            ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD),
            ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD),
            ("pcPriClassBase", wintypes.LONG),
            ("dwFlags", wintypes.DWORD),
            ("szExeFile", wintypes.WCHAR * 260),
        ]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Process32FirstW.argtypes = [
        wintypes.HANDLE, ctypes.POINTER(ProcessEntry32W)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [
        wintypes.HANDLE, ctypes.POINTER(ProcessEntry32W)]
    kernel32.Process32NextW.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
    if snapshot == wintypes.HANDLE(-1).value:
        raise RuntimeError(
            "cannot prove physical UAP isolation: process snapshot failed "
            f"with Win32 error {ctypes.get_last_error()}")
    owners: list[int] = []
    try:
        entry = ProcessEntry32W()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            error = ctypes.get_last_error()
            if error != 18:  # ERROR_NO_MORE_FILES
                raise RuntimeError(
                    "cannot prove physical UAP isolation: process enumeration "
                    f"failed with Win32 error {error}")
        else:
            while True:
                if entry.szExeFile.casefold() == "halljoy.exe":
                    owners.append(int(entry.th32ProcessID))
                if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                    error = ctypes.get_last_error()
                    if error != 18:  # ERROR_NO_MORE_FILES
                        raise RuntimeError(
                            "cannot prove physical UAP isolation: process enumeration "
                            f"failed with Win32 error {error}")
                    break
    finally:
        kernel32.CloseHandle(snapshot)
    if owners:
        raise RuntimeError(
            "physical UAP ABI test requires every HallJoy instance to be closed; "
            f"active PIDs: {', '.join(map(str, owners))}")


class KeyIdentityV1(ctypes.Structure):
    _fields_ = [
        ("schema_version", ctypes.c_uint16),
        ("key_namespace", ctypes.c_uint16),
        ("usage_page", ctypes.c_uint32),
        ("usage", ctypes.c_uint32),
    ]


class AnalogValueV2(ctypes.Structure):
    _fields_ = [
        ("normalized", ctypes.c_float),
        ("raw_numerator", ctypes.c_uint32),
        ("raw_domain", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
    ]


class AnalogDeviceV2(ctypes.Structure):
    _fields_ = [
        ("schema_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("device_id", ctypes.c_uint64),
        ("exact_interface_id", ctypes.c_uint64),
        ("flags", ctypes.c_uint32),
        ("protocol_id", ctypes.c_uint32),
        ("layout_id", ctypes.c_uint32),
        ("vendor_id", ctypes.c_uint16),
        ("product_id", ctypes.c_uint16),
        ("usage_page", ctypes.c_uint16),
        ("usage", ctypes.c_uint16),
    ]


class AnalogSampleV2(ctypes.Structure):
    _fields_ = [
        ("schema_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("key", KeyIdentityV1),
        ("device_index", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("value", AnalogValueV2),
    ]


class AnalogSnapshotHeaderV2(ctypes.Structure):
    _fields_ = [
        ("schema_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("provider_id", ctypes.c_uint64),
        ("provider_generation", ctypes.c_uint64),
        ("sample_generation", ctypes.c_uint64),
        ("value_generation", ctypes.c_uint64),
        ("ownership_generation", ctypes.c_uint64),
        ("sample_timestamp_us", ctypes.c_uint64),
        ("value_timestamp_us", ctypes.c_uint64),
        ("flags", ctypes.c_uint32),
        ("device_count", ctypes.c_uint32),
        ("device_capacity", ctypes.c_uint32),
        ("required_device_count", ctypes.c_uint32),
        ("sample_count", ctypes.c_uint32),
        ("sample_capacity", ctypes.c_uint32),
        ("required_sample_count", ctypes.c_uint32),
    ]


class DenseDeviceV1(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("version", ctypes.c_uint32),
        ("device_id", ctypes.c_uint64),
        ("generation", ctypes.c_uint64),
        ("timestamp_us", ctypes.c_uint64),
        ("active_key_count", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("vendor_id", ctypes.c_uint16),
        ("product_id", ctypes.c_uint16),
        ("usage_page", ctypes.c_uint16),
        ("usage", ctypes.c_uint16),
        ("values", ctypes.c_float * 256),
    ]


def validate_provider_snapshot(
    header: AnalogSnapshotHeaderV2,
    devices: ctypes.Array[AnalogDeviceV2],
    samples: ctypes.Array[AnalogSampleV2],
) -> None:
    if header.schema_version != 2 or header.struct_size != ctypes.sizeof(header):
        raise RuntimeError("provider V2 returned an invalid snapshot header")
    if header.provider_id != 0x3256504155594A48:
        raise RuntimeError(f"provider V2 returned an unexpected provider: {header.provider_id:#x}")
    if min(header.provider_generation, header.sample_generation,
           header.value_generation, header.ownership_generation) == 0:
        raise RuntimeError("provider V2 returned a zero generation")
    if header.flags not in (1, 2):
        raise RuntimeError(f"provider V2 returned invalid completeness flags: {header.flags:#x}")
    if (header.device_count > header.device_capacity or
            header.sample_count > header.sample_capacity or
            header.required_device_count < header.device_count or
            header.required_sample_count < header.sample_count):
        raise RuntimeError("provider V2 returned inconsistent capacities")
    insufficient = (header.required_device_count > header.device_capacity or
                    header.required_sample_count > header.sample_capacity)
    if (header.flags == 2) != insufficient:
        raise RuntimeError("provider V2 hid or invented snapshot truncation")
    if header.flags == 1 and (
            header.device_count != header.required_device_count or
            header.sample_count != header.required_sample_count):
        raise RuntimeError("provider V2 marked an incomplete snapshot complete")
    if header.device_count > len(devices) or header.sample_count > len(samples):
        raise RuntimeError("provider V2 exceeded caller storage")

    device_ids: set[int] = set()
    for device in devices[:header.device_count]:
        if (device.schema_version != 2 or
                device.struct_size != ctypes.sizeof(AnalogDeviceV2) or
                device.device_id == 0 or (device.flags & 1) == 0 or
                device.flags & ~0x1F):
            raise RuntimeError("provider V2 returned an invalid device")
        if device.device_id in device_ids:
            raise RuntimeError("provider V2 returned duplicate device identities")
        device_ids.add(device.device_id)

    sample_ids: set[tuple[int, int, int, int]] = set()
    for sample in samples[:header.sample_count]:
        key = sample.key
        valid_key = (key.schema_version == 1 and key.usage != 0 and
                     ((key.key_namespace == 1 and key.usage_page != 0) or
                      (key.key_namespace == 2 and key.usage_page == 0)))
        value = sample.value
        if (sample.schema_version != 2 or
                sample.struct_size != ctypes.sizeof(AnalogSampleV2) or
                sample.device_index >= header.device_count or
                sample.flags != 0x7 or not valid_key or
                not math.isfinite(value.normalized) or
                not 0.0 <= value.normalized <= 1.0 or
                value.raw_numerator != 0 or value.raw_domain != 0 or
                value.flags != 0):
            raise RuntimeError("provider V2 returned an invalid sample")
        identity = (sample.device_index, key.key_namespace,
                    key.usage_page, key.usage)
        if identity in sample_ids:
            raise RuntimeError("provider V2 returned a duplicate sample identity")
        sample_ids.add(identity)


def validate_dual_view(
    header: AnalogSnapshotHeaderV2,
    devices: ctypes.Array[AnalogDeviceV2],
    samples: ctypes.Array[AnalogSampleV2],
    dense_devices: ctypes.Array[DenseDeviceV1],
) -> None:
    """Prove that both ABI views came from the same accepted capture."""
    validate_provider_snapshot(header, devices, samples)
    if header.device_count > len(dense_devices):
        raise RuntimeError("dual-view snapshot exceeded legacy dense storage")

    projected = [[0.0] * 256 for _ in range(header.device_count)]
    for sample in samples[:header.sample_count]:
        key = sample.key
        if key.key_namespace != 1 or key.usage_page != 0x07 or key.usage >= 256:
            continue
        value = float(sample.value.normalized)
        projected[sample.device_index][key.usage] = max(
            projected[sample.device_index][key.usage], value)

    for index in range(header.device_count):
        provider = devices[index]
        dense = dense_devices[index]
        if (dense.struct_size != ctypes.sizeof(DenseDeviceV1) or
                dense.version != 1 or dense.device_id != provider.device_id or
                dense.generation == 0 or dense.timestamp_us == 0 or
                (dense.flags & 1) == 0 or dense.flags & ~0x0F or
                dense.vendor_id != provider.vendor_id or
                dense.product_id != provider.product_id or
                dense.usage_page != provider.usage_page or
                dense.usage != provider.usage):
            raise RuntimeError(
                f"dual-view returned inconsistent device {index}: "
                f"size={dense.struct_size}/{ctypes.sizeof(DenseDeviceV1)}, "
                f"version={dense.version}, id={dense.device_id}/{provider.device_id}, "
                f"generation={dense.generation}, timestamp_us={dense.timestamp_us}, "
                f"flags={dense.flags:#x}, "
                f"vidpid={dense.vendor_id:04x}:{dense.product_id:04x}/"
                f"{provider.vendor_id:04x}:{provider.product_id:04x}, "
                f"usage={dense.usage_page:04x}:{dense.usage:04x}/"
                f"{provider.usage_page:04x}:{provider.usage:04x}")
        active = 0
        for usage in range(256):
            dense_value = float(dense.values[usage])
            if not math.isfinite(dense_value) or not 0.0 <= dense_value <= 1.0:
                raise RuntimeError(
                    f"dual-view dense value is invalid at device {index}, usage {usage:#x}")
            if dense_value != projected[index][usage]:
                raise RuntimeError(
                    f"dual-view mismatch at device {index}, usage {usage:#x}: "
                    f"dense={dense_value!r}, provider_v2={projected[index][usage]!r}")
            active += dense_value > 0.0
        if dense.active_key_count != active:
            raise RuntimeError(
                f"dual-view active-key count mismatch at device {index}: "
                f"dense={dense.active_key_count}, measured={active}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", type=Path)
    parser.add_argument(
        "--exclude-token",
        default="path_b8d4f30578af404c_0000004a",
        help="exact normalized HallJoy HID interface claim token",
    )
    args = parser.parse_args()

    dll = args.dll.resolve()
    if not dll.is_file():
        raise SystemExit(f"private UAP DLL is missing: {dll}")
    assert_no_halljoy_processes()
    os.environ["HALLJOY_UAP_NATIVE_HID_PATHS"] = args.exclude_token

    api = ctypes.CDLL(str(dll))
    abi = ctypes.c_uint32.in_dll(api, "ANALOG_SDK_PLUGIN_ABI_VERSION").value
    if abi != 1:
        raise RuntimeError(f"unexpected ABI version: {abi}")

    # SOUP_CEXPORT strips the source-level leading underscore on Windows.
    plugin_name_export = api["name"]
    plugin_name_export.argtypes = []
    plugin_name_export.restype = ctypes.c_char_p
    plugin_name = plugin_name_export().decode("utf-8", errors="strict")
    if "interface-path pinned-snapshot stable-identity deadline-paced telemetry" not in plugin_name:
        raise RuntimeError(f"unexpected private UAP build identity: {plugin_name!r}")

    api.is_initialised.argtypes = []
    api.is_initialised.restype = ctypes.c_bool
    api.device_info.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_uint32]
    api.device_info.restype = ctypes.c_int
    api.initialise.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    api.initialise.restype = ctypes.c_int
    api.read_full_buffer.argtypes = [
        ctypes.POINTER(ctypes.c_uint16), ctypes.POINTER(ctypes.c_float),
        ctypes.c_uint32, ctypes.c_uint64,
    ]
    api.read_full_buffer.restype = ctypes.c_int
    api.read_analog.argtypes = [ctypes.c_uint16, ctypes.c_uint64]
    api.read_analog.restype = ctypes.c_float
    api.halljoy_unload_bounded.argtypes = [ctypes.c_uint32]
    api.halljoy_unload_bounded.restype = ctypes.c_bool
    api.halljoy_get_provider_snapshot_v2.argtypes = [
        ctypes.POINTER(AnalogSnapshotHeaderV2), ctypes.c_uint32,
        ctypes.POINTER(AnalogDeviceV2), ctypes.c_uint32, ctypes.c_uint32,
        ctypes.POINTER(AnalogSampleV2), ctypes.c_uint32, ctypes.c_uint32,
    ]
    api.halljoy_get_provider_snapshot_v2.restype = ctypes.c_bool
    api.halljoy_get_dual_snapshot_v2.argtypes = [
        ctypes.POINTER(AnalogSnapshotHeaderV2), ctypes.c_uint32,
        ctypes.POINTER(AnalogDeviceV2), ctypes.c_uint32, ctypes.c_uint32,
        ctypes.POINTER(AnalogSampleV2), ctypes.c_uint32, ctypes.c_uint32,
        ctypes.POINTER(DenseDeviceV1), ctypes.c_uint32, ctypes.c_uint32,
    ]
    api.halljoy_get_dual_snapshot_v2.restype = ctypes.c_bool

    if api.is_initialised():
        raise RuntimeError("fresh plugin incorrectly reports initialised")
    if api.device_info(None, 1) != 0:
        raise RuntimeError("device_info did not reject a null buffer")
    if api.read_full_buffer(None, None, 1, 0) != 0:
        raise RuntimeError("read_full_buffer did not reject null buffers")
    codes = (ctypes.c_uint16 * 1)()
    values = (ctypes.c_float * 1)()
    if api.read_full_buffer(codes, values, 1, 0) != 0 or api.read_analog(4, 0) != 0.0:
        raise RuntimeError("fresh plugin exposed data before initialisation")
    inactive_header = AnalogSnapshotHeaderV2()
    if api.halljoy_get_provider_snapshot_v2(
            ctypes.byref(inactive_header), ctypes.sizeof(inactive_header),
            None, 0, ctypes.sizeof(AnalogDeviceV2),
            None, 0, ctypes.sizeof(AnalogSampleV2)):
        raise RuntimeError("provider V2 exposed a snapshot before initialisation")
    inactive_dense = (DenseDeviceV1 * 1)()
    if api.halljoy_get_dual_snapshot_v2(
            ctypes.byref(inactive_header), ctypes.sizeof(inactive_header),
            None, 0, ctypes.sizeof(AnalogDeviceV2),
            None, 0, ctypes.sizeof(AnalogSampleV2),
            inactive_dense, len(inactive_dense), ctypes.sizeof(DenseDeviceV1)):
        raise RuntimeError("dual-view exposed a snapshot before initialisation")

    # Recheck immediately before the first operation that can open hardware.
    # The earlier ABI/null tests intentionally run with the plugin inactive.
    assert_no_halljoy_processes()
    initial_devices = api.initialise(None, None)
    if initial_devices < 0 or not api.is_initialised():
        raise RuntimeError(f"plugin failed truthful initialisation: {initial_devices}")

    # Device ownership is visible before the first hardware acquisition.  Drive
    # synchronous transports through the ordinary ABI and give asynchronous
    # workers a bounded window; V2 must never manufacture a Fresh constructor-
    # zero frame while that first sample is pending.
    acquisition_codes = (ctypes.c_uint16 * 256)()
    acquisition_values = (ctypes.c_float * 256)()
    ready_deadline = time.monotonic() + 3.0
    # First ask only for the exact demand.  The intentionally zero-capacity
    # generation must be honestly marked truncated whenever devices exist.
    while True:
        result = api.read_full_buffer(
            acquisition_codes, acquisition_values, len(acquisition_codes), 0)
        demand = AnalogSnapshotHeaderV2()
        provider_ready = api.halljoy_get_provider_snapshot_v2(
            ctypes.byref(demand), ctypes.sizeof(demand),
            None, 0, ctypes.sizeof(AnalogDeviceV2),
            None, 0, ctypes.sizeof(AnalogSampleV2))
        if result >= 0 and provider_ready:
            break
        if time.monotonic() >= ready_deadline:
            raise RuntimeError(
                "provider V2 did not publish a real first acquisition within 3 seconds")
        time.sleep(0.005)
    if demand.required_device_count > 4096 or demand.required_sample_count > 1048576:
        raise RuntimeError("provider V2 returned an unreasonable storage demand")

    device_storage = max(1, demand.required_device_count)
    sample_storage = max(1, demand.required_sample_count)
    devices = (AnalogDeviceV2 * device_storage)()
    samples = (AnalogSampleV2 * sample_storage)()
    snapshot = AnalogSnapshotHeaderV2()
    if not api.halljoy_get_provider_snapshot_v2(
            ctypes.byref(snapshot), ctypes.sizeof(snapshot),
            devices, device_storage, ctypes.sizeof(AnalogDeviceV2),
            samples, sample_storage, ctypes.sizeof(AnalogSampleV2)):
        raise RuntimeError("provider V2 full snapshot query failed")
    validate_provider_snapshot(snapshot, devices, samples)

    # Exercise one bounded, same-generation export independently of the demand
    # query above.  The plugin itself compares all captured ordinary HID cells;
    # this caller repeats that comparison across the C ABI boundary.
    dual_devices = (AnalogDeviceV2 * device_storage)()
    dual_samples = (AnalogSampleV2 * sample_storage)()
    dual_dense = (DenseDeviceV1 * device_storage)()
    dual_snapshot = AnalogSnapshotHeaderV2()
    if not api.halljoy_get_dual_snapshot_v2(
            ctypes.byref(dual_snapshot), ctypes.sizeof(dual_snapshot),
            dual_devices, len(dual_devices), ctypes.sizeof(AnalogDeviceV2),
            dual_samples, len(dual_samples), ctypes.sizeof(AnalogSampleV2),
            dual_dense, len(dual_dense), ctypes.sizeof(DenseDeviceV1)):
        raise RuntimeError("same-generation dual-view snapshot query failed")
    validate_dual_view(dual_snapshot, dual_devices, dual_samples, dual_dense)
    if dual_snapshot.flags != 1:
        raise RuntimeError("negotiated dual-view snapshot remained truncated")
    dual_device_count = dual_snapshot.device_count
    dual_sample_count = dual_snapshot.sample_count

    if not api.halljoy_unload_bounded(3000):
        raise RuntimeError("bounded unload did not join all plugin workers")
    if api.is_initialised():
        raise RuntimeError("plugin remained initialised after unload")
    if api.read_full_buffer(codes, values, 1, 0) != 0 or api.read_analog(4, 0) != 0.0:
        raise RuntimeError("plugin exposed stale data after unload")
    if api.halljoy_get_provider_snapshot_v2(
            ctypes.byref(snapshot), ctypes.sizeof(snapshot),
            devices, device_storage, ctypes.sizeof(AnalogDeviceV2),
            samples, sample_storage, ctypes.sizeof(AnalogSampleV2)):
        raise RuntimeError("provider V2 exposed stale data after unload")
    if api.halljoy_get_dual_snapshot_v2(
            ctypes.byref(dual_snapshot), ctypes.sizeof(dual_snapshot),
            dual_devices, len(dual_devices), ctypes.sizeof(AnalogDeviceV2),
            dual_samples, len(dual_samples), ctypes.sizeof(AnalogSampleV2),
            dual_dense, len(dual_dense), ctypes.sizeof(DenseDeviceV1)):
        raise RuntimeError("dual-view exposed stale data after unload")
    if not api.halljoy_unload_bounded(3000):
        raise RuntimeError("idempotent bounded unload failed")

    print(
        f"PRIVATE_UAP_ABI_RUNTIME=PASS abi={abi} initial_devices={initial_devices} "
        f"provider_v2_devices={snapshot.device_count} "
        f"provider_v2_samples={snapshot.sample_count} "
        f"dual_view_devices={dual_device_count} "
        f"dual_view_samples={dual_sample_count} dual_view_equivalent=1 "
        f"negotiated_capacity=1 "
        f"name={plugin_name!r}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
