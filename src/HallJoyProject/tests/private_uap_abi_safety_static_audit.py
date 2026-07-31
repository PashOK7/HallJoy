#!/usr/bin/env python3
"""Verify the V14-07C private UAP C ABI and bounded-unload contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PLUGIN = ROOT / "third_party" / "UniversalAnalogPluginFixed"
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"

uap = (PLUGIN / "main.cpp").read_text(encoding="utf-8-sig")
guard = (PLUGIN / "halljoy_uap_cabi_guard.h").read_text(encoding="utf-8-sig")
host = (HALL / "analog_host_client.cpp").read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


require("catch (...)" in guard and "noexcept" in guard,
        "portable C ABI guard contains every C++ exception")
require("class LockGuard final" in guard and "~LockGuard() noexcept" in guard,
        "recursive mutex ownership is exception-safe RAII")
require(".lock()" not in uap and ".unlock()" not in uap,
        "plugin implementation has no manual mutex ownership")
require(uap.count("halljoy::uap::CAbiInvoke") >= 9,
        "throwing-value exports use the common C ABI barrier")
require("CAbiInvokeVoid" in uap and "SOUP_CEXPORT void unload() noexcept" in uap,
        "void unload export cannot leak a C++ exception")
require("running.load(std::memory_order_acquire)" in uap and
        "halljoy_restart_blocked.load" in uap,
        "is_initialised reports truthful running and poison state")
require("buffer == nullptr || len == 0 || !is_initialised()" in uap,
        "device_info rejects null and invalid-state calls")
require(uap.count("!is_initialised()") >= 5,
        "all private UAP data exports reject inactive generations")
require("SOUP_CEXPORT bool halljoy_unload_bounded" in uap and
        "WaitForSingleObject(thread.handle, wait_ms)" in uap,
        "private unload has a bounded worker join")
workers_pos = uap.index("std::vector<Device*> workers")
wait_pos = uap.index("halljoy_wait_thread_until(dev->thrd", workers_pos)
final_lock_pos = uap.index(
    "LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);", wait_pos)
require(workers_pos < wait_pos < final_lock_pos < uap.index("devices.clear()", final_lock_pos),
        "device workers are cancelled and joined outside the global devices mutex")
require(uap.count("catch (...)") >= 2 and "halljoy_mark_plugin_fault" in uap,
        "UAP worker entries contain exceptions and publish poison")
require("halljoy_unload_bounded" in host and "UnloadHostPlugin" in host,
        "isolated host consumes the bounded private unload export")
require("bounded plugin unload incomplete" in host and
        "TerminateProcess(GetCurrentProcess()" in host,
        "incomplete unload selects child-process containment without FreeLibrary")

print("PRIVATE_UAP_ABI_SAFETY_STATIC_AUDIT=PASS")
