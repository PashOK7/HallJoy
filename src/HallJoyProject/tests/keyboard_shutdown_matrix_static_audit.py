#!/usr/bin/env python3
"""Require shutdown fault coverage for every production keyboard route."""

from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"
TOOLS = REPO / "tools"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="strict")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    catalog = read(HALL / "native_analog_backends.def")
    matrix = read(TOOLS / "run_keyboard_shutdown_matrix.ps1")
    runner = read(TOOLS / "run_analog_simulator.ps1")
    aula = read(HALL / "aula_win60he_backend.cpp")

    production_getters = (
        "Mad68ProR_GetNativeBackendDescriptor",
        "Hex80_GetNativeBackendDescriptor",
        "AddressedAnalog_GetNativeBackendDescriptor",
        "AulaWin60He_GetNativeBackendDescriptor",
        "BackendNative_GetSparkDescriptor",
        "BackendNative_GetSayoDescriptor",
    )
    require(all(catalog.count(getter) == 1 for getter in production_getters),
            "the production catalog has six unique native keyboard routes")

    native_scenarios = {
        "sparklink": "InjectSparkStopTimeout",
        "sayo": "InjectSayoStopTimeout",
        "addressed": "InjectAddressedStopTimeout",
        "hex80": "InjectHex80StopTimeout",
        "mad68-pro-r": "InjectMad68StopTimeout",
        "aula-win60he": "InjectAulaStopTimeout",
    }
    for name, switch in native_scenarios.items():
        require(f"Name = '{name}'" in matrix and switch in matrix and switch in runner,
                f"{name} has a process-level permanent-stop scenario")

    require("--halljoy-test-aula-stop-timeout" in aula and
            "test.stop_timeout.injected" in aula,
            "Aula's missing permanent-stop seam is simulator-only and traced")
    require("Name = 'uap-soup'" in matrix and
            "InjectAnalogHostChildStopHang" in matrix and
            "Name = 'process-watchdog'" in matrix and
            "InjectMad68OwnerStopHang" in matrix,
            "plugin keyboards and the final process boundary are both covered")
    require("Name = 'normal'" in matrix and "Switch = $null" in matrix,
            "the matrix includes a non-fault common-pipeline control")
    require("Copy-Item -LiteralPath $tracePath" in matrix and
            "trace_sha256" in matrix and "summary.json" in matrix,
            "every scenario retains a hashed trace and aggregate summary")
    require("HallJoyV14Simulator" in matrix and "survivors = 0" in matrix,
            "every scenario rejects a surviving HallJoy process")
    require("NOT hardware verification" in matrix and
            "hardware_verified = $false" in matrix,
            "simulator coverage cannot be mislabeled as hardware validation")

    print("KEYBOARD_SHUTDOWN_MATRIX_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
