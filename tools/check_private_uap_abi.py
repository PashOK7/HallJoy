#!/usr/bin/env python3
"""Exercise the private ABI1 null/state/bounded-unload contract."""

from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", type=Path)
    parser.add_argument("--exclude-hid", default="vid_1ca6&pid_0529")
    args = parser.parse_args()

    dll = args.dll.resolve()
    if not dll.is_file():
        raise SystemExit(f"private UAP DLL is missing: {dll}")
    os.environ["HALLJOY_UAP_NATIVE_HID_IDS"] = args.exclude_hid

    api = ctypes.CDLL(str(dll))
    abi = ctypes.c_uint32.in_dll(api, "ANALOG_SDK_PLUGIN_ABI_VERSION").value
    if abi != 1:
        raise RuntimeError(f"unexpected ABI version: {abi}")

    # SOUP_CEXPORT strips the source-level leading underscore on Windows.
    plugin_name_export = api["name"]
    plugin_name_export.argtypes = []
    plugin_name_export.restype = ctypes.c_char_p
    plugin_name = plugin_name_export().decode("utf-8", errors="strict")
    if "pinned-snapshot stable-identity deadline-paced telemetry" not in plugin_name:
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

    initial_devices = api.initialise(None, None)
    if initial_devices < 0 or not api.is_initialised():
        raise RuntimeError(f"plugin failed truthful initialisation: {initial_devices}")
    if not api.halljoy_unload_bounded(3000):
        raise RuntimeError("bounded unload did not join all plugin workers")
    if api.is_initialised():
        raise RuntimeError("plugin remained initialised after unload")
    if api.read_full_buffer(codes, values, 1, 0) != 0 or api.read_analog(4, 0) != 0.0:
        raise RuntimeError("plugin exposed stale data after unload")
    if not api.halljoy_unload_bounded(3000):
        raise RuntimeError("idempotent bounded unload failed")

    print(
        f"PRIVATE_UAP_ABI_RUNTIME=PASS abi={abi} initial_devices={initial_devices} "
        f"name={plugin_name!r}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
