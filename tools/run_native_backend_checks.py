#!/usr/bin/env python3
"""Run HallJoy native-backend architecture audits and portable C++ tests.

This script is intentionally independent of Visual Studio. BUILD.cmd runs the same
static audits before the full MSVC build; contributors can use this runner on
Linux/macOS/MinGW while developing parsers and schedulers.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


def run(command: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True, timeout=120)


def find_cxx() -> str | None:
    configured = os.environ.get("CXX")
    if configured:
        return configured
    for candidate in ("g++", "clang++"):
        path = shutil.which(candidate)
        if path:
            return path
    if os.name == "nt":
        fixed_candidates = (
            Path(r"C:\BuildTools\VC\Tools\Llvm\x64\bin\clang++.exe"),
            Path(r"C:\Program Files\LLVM\bin\clang++.exe"),
            Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang++.exe"),
        )
        for candidate in fixed_candidates:
            if candidate.is_file():
                return str(candidate)
    return None


def compile_and_run(cxx: str, output: Path, sources: list[Path], include: Path) -> None:
    extra_compile_args = (
        ["-DHALLJOY_VIGEM_TRANSPORT_FAKE_ONLY"]
        if output.name == "vigem_child_transport"
        else []
    )
    command = [
        cxx,
        "-std=c++20",
        "-O2",
        "-Wall",
        "-Wextra",
        "-pedantic",
        f"-I{include}",
        f"-I{include.parents[2] / 'third_party' / 'UniversalAnalogPluginFixed'}",
        f"-I{include.parent / 'third_party' / 'ViGEmClient' / 'include'}",
        *extra_compile_args,
        *map(str, sources),
        # MinGW often supplies this implicitly; clang/MSVC does not. Windows
        # token/SID integration tests must link their actual system dependency.
        *(["-ladvapi32", "-luser32"] if os.name == "nt" else []),
        *(["-lsetupapi", "-lhid", "-lshell32", "-lole32", "-luuid"] if output.name == "support_log_windows" else []),
        "-o",
        str(output),
    ]
    run(command)
    run([str(output)])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--static-only", action="store_true", help="skip portable C++ compilation")
    parser.add_argument("--require-compiler", action="store_true", help="fail when g++/clang++ is unavailable")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    project_root = root / "src" / "HallJoyProject"
    hall = project_root / "HallJoy"
    tests = project_root / "tests"

    ET.parse(hall / "HallJoy.vcxproj")
    ET.parse(hall / "HallJoy.vcxproj.filters")

    run([sys.executable, str(root / "tools" / "test_block_keys_group.py")])
    run([sys.executable, "-m", "unittest", "discover", "-s", str(root / "tools" / "tests"), "-p", "test_layout_pipeline.py"])
    for brand in ("Keychron", "Lemokey", "DrunkDeer", "Aula", "Redragon", "Razer", "NuPhy", "Wooting"):
        run([sys.executable, str(root / "tools" / "layout_pipeline.py"), "check", brand])
    run([sys.executable, str(project_root / "tools" / "validate_addressed_protocol_backend.py")])

    for script in sorted(tests.glob("*audit.py")):
        run([sys.executable, str(script)])

    if args.static_only:
        print("native backend checks: static audits passed")
        return 0

    cxx = find_cxx()
    if not cxx:
        message = "No g++/clang++ found; portable C++ tests were skipped. BUILD.cmd still performs the full MSVC build."
        if args.require_compiler:
            raise SystemExit(message)
        print(message)
        return 0

    build_temp = root / "build" / "obj" / "portable-tests"
    build_temp.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="halljoy-native-tests-", dir=build_temp) as temp:
        out = Path(temp)
        fixed_tests: list[tuple[str, list[Path]]] = [
            ("window_placement", [tests / "window_placement_test.cpp"]),
            ("overlay_text_edit", [tests / "overlay_text_edit_test.cpp"]),
            ("layout_editor_model", [tests / "layout_editor_model_test.cpp"]),
            ("layout_sort", [tests / "layout_sort_test.cpp"]),
            ("layout_identity", [tests / "layout_identity_test.cpp"]),
            ("remap_hint_motion", [tests / "remap_hint_motion_test.cpp"]),
            ("digital_keyboard_state", [tests / "digital_keyboard_state_test.cpp"]),
            ("input_privilege_warning", [tests / "input_privilege_warning_test.cpp"]),
            ("block_keys_policy", [tests / "block_keys_policy_test.cpp"]),
            ("profile_runtime_gate", [tests / "profile_runtime_gate_test.cpp"]),
            ("analog_simulator", [tests / "analog_simulator_model_test.cpp", hall / "analog_simulator_model.cpp"]),
            ("addressed_scheduler", [tests / "addressed_poll_scheduler_test.cpp", hall / "addressed_poll_scheduler.cpp"]),
            ("hid_lifecycle", [tests / "hid_io_operation_lifecycle_test.cpp"]),
            ("vigem_scheduler", [tests / "vigem_output_scheduler_test.cpp"]),
            ("latest_value_mailbox", [tests / "latest_value_mailbox_test.cpp"]),
            ("vigem_output_channel", [
                tests / "vigem_output_channel_test.cpp",
                hall / "vigem_output_channel.cpp",
            ]),
            ("vigem_output_telemetry", [
                tests / "vigem_output_telemetry_test.cpp",
                hall / "vigem_output_channel.cpp",
            ]),
            ("vigem_output_producer_lease", [
                tests / "vigem_output_producer_lease_test.cpp",
            ]),
            ("vigem_child_transport", [
                tests / "vigem_child_transport_test.cpp",
                hall / "vigem_child_transport.cpp",
            ]),
            ("vigem_output_channel_process", [
                tests / "vigem_output_channel_process_test.cpp",
                hall / "vigem_output_channel.cpp",
            ]),
            ("process_generation_supervisor", [
                tests / "process_generation_supervisor_test.cpp",
                hall / "process_generation_supervisor.cpp",
            ]),
            ("analog_provider_v2", [
                tests / "analog_provider_v2_test.cpp",
                hall / "analog_provider_v2.cpp",
            ]),
            ("native_analog_snapshot_adapter", [
                tests / "native_analog_snapshot_adapter_test.cpp",
                hall / "native_analog_snapshot_adapter.cpp",
                hall / "analog_provider_v2.cpp",
            ]),
            ("provider_v2_data_plane_layout", [
                tests / "provider_v2_data_plane_layout_test.cpp",
                hall / "provider_v2_data_plane_layout.cpp",
                hall / "analog_provider_v2.cpp",
            ]),
            ("provider_v2_snapshot_broker", [
                tests / "provider_v2_snapshot_broker_test.cpp",
                hall / "provider_v2_snapshot_broker.cpp",
                hall / "analog_provider_v2.cpp",
            ]),
            ("uap_provider_v2_projection", [
                tests / "uap_provider_v2_projection_test.cpp",
                hall / "analog_provider_v2.cpp",
            ]),
            ("uap_parent_snapshot", [
                tests / "uap_parent_snapshot_test.cpp",
                hall / "uap_parent_snapshot.cpp",
                hall / "analog_provider_v2.cpp",
            ]),
            ("provider_v2_controller_shadow", [
                tests / "provider_v2_controller_shadow_test.cpp",
                hall / "provider_v2_controller_shadow.cpp",
                hall / "analog_provider_v2.cpp",
            ]),
            ("provider_v2_qualification_model", [
                tests / "provider_v2_qualification_model_test.cpp",
                hall / "provider_v2_qualification_model.cpp",
            ]),
            ("configured_xusb_builder", [
                tests / "configured_xusb_builder_test.cpp",
                hall / "configured_xusb_builder.cpp",
                hall / "xusb_output_adapter.cpp",
            ]),
            ("native_contract", [tests / "native_analog_backend_contract_test.cpp"]),
            ("native_hid_interface_claim", [tests / "native_hid_interface_claim_test.cpp"]),
            ("native_lifecycle_registry", [tests / "native_backend_lifecycle_registry_test.cpp"]),
            ("worker_lifecycle", [tests / "worker_lifecycle_test.cpp"]),
            ("worker_join_policy", [tests / "worker_join_policy_test.cpp"]),
            ("worker_primitives", [tests / "worker_primitives_test.cpp"]),
            ("worker_exception_barrier", [tests / "worker_exception_barrier_test.cpp"]),
            ("input_wake_sequence", [tests / "input_wake_sequence_test.cpp"]),
            ("publication_generation", [tests / "publication_generation_test.cpp"]),
            ("transactional_file_store", [tests / "transactional_file_store_test.cpp"]),
            ("dependency_guidance_policy", [tests / "dependency_guidance_policy_test.cpp"]),
            ("uap_cabi_guard", [tests / "uap_cabi_guard_test.cpp"]),
            ("uap_device_identity", [tests / "uap_device_identity_test.cpp"]),
            ("drunkdeer_identity", [tests / "drunkdeer_identity_test.cpp"]),
            ("uap_poll_pacing", [tests / "uap_poll_pacing_test.cpp"]),
            ("uap_snapshot_pinning", [tests / "uap_snapshot_pinning_test.cpp"]),
            ("windows_command_line", [tests / "windows_command_line_test.cpp"]),
            ("sparklink_hotplug_age", [tests / "sparklink_hotplug_age_test.cpp"]),
            ("sparklink_row_freshness", [tests / "sparklink_row_freshness_test.cpp"]),
            ("sayo_letter_matcher", [tests / "sayo_letter_matcher_test.cpp"]),
            ("keyboard_support_status", [
                tests / "keyboard_support_status_test.cpp",
                hall / "keyboard_support_status.cpp",
            ]),
            ("runtime_arithmetic", [tests / "runtime_arithmetic_test.cpp"]),
            ("runtime_command_state", [tests / "runtime_command_state_test.cpp"]),
            ("bounded_ini_numeric", [tests / "bounded_ini_numeric_test.cpp"]),
            ("curve_math", [tests / "curve_math_test.cpp", hall / "curve_math.cpp"]),
            ("engine_runtime_transaction", [tests / "engine_runtime_transaction_test.cpp"]),
            ("raw_input_packet_size", [tests / "raw_input_packet_size_test.cpp"]),
            ("extended_key_bindings", [
                tests / "extended_key_bindings_test.cpp",
                hall / "bindings.cpp",
            ]),
            ("key_settings_domain", [
                tests / "key_settings_domain_test.cpp",
                hall / "key_settings.cpp",
                hall / "backend_curve.cpp",
                hall / "settings.cpp",
                hall / "curve_math.cpp",
            ]),
            ("protocol_parser_fuzz_smoke", [
                tests / "protocol_parser_fuzz_smoke_test.cpp",
                hall / "aula_win60he_protocol.cpp",
                hall / "hex80_protocol.cpp",
                hall / "mad68pr_protocol.cpp",
            ]),
            ("aula_win60he_oracle", [
                tests / "aula_win60he_oracle_test.cpp",
                hall / "aula_win60he_protocol.cpp",
            ]),
            ("aula_win60he_end_to_end", [
                tests / "aula_win60he_end_to_end_test.cpp",
                hall / "aula_win60he_protocol.cpp",
                hall / "aula_win60he_client.cpp",
            ]),
            ("aula_win60he_diagnostic_metrics", [
                tests / "aula_win60he_diagnostic_metrics_test.cpp",
                hall / "aula_win60he_diagnostic_metrics.cpp",
                hall / "aula_win60he_protocol.cpp",
            ]),
            ("aula_win60he_session_policy", [
                tests / "aula_win60he_session_policy_test.cpp",
                hall / "aula_win60he_session_policy.cpp",
            ]),
        ]
        if os.name == "nt":
            fixed_tests.append(("block_keys_hotkey_windows", [tests / "block_keys_hotkey_windows_test.cpp"]))
            fixed_tests.append(("support_log_windows", [
                tests / "support_log_windows_test.cpp", hall / "support_log.cpp"
            ]))
            fixed_tests.append(("engine_runtime_notification_windows", [
                tests / "engine_runtime_notification_windows_test.cpp", hall / "engine_runtime_owner.cpp"
            ]))
            fixed_tests.append(("instance_guard_windows", [
                tests / "instance_guard_windows_test.cpp", hall / "instance_guard.cpp"
            ]))
            fixed_tests.append(("profile_loader_windows", [
                tests / "profile_loader_windows_test.cpp", hall / "profile_ini.cpp", hall / "bindings.cpp"
            ]))
            fixed_tests.append(("provider_v2_data_plane_windows", [
                tests / "provider_v2_data_plane_windows_test.cpp",
                hall / "provider_v2_data_plane_windows.cpp",
                hall / "provider_v2_data_plane_layout.cpp",
                hall / "analog_provider_v2.cpp",
            ]))
        for name, sources in fixed_tests:
            compile_and_run(cxx, out / name, sources, hall)

        # Convention used by built-ins and tools/new_native_backend.py:
        # tests/<name>_protocol_test.cpp links HallJoy/<name>_protocol.cpp.
        for test in sorted(tests.glob("*_protocol_test.cpp")):
            protocol_source = hall / test.name.replace("_test.cpp", ".cpp")
            if not protocol_source.exists():
                raise SystemExit(f"Missing pure protocol source for {test.name}: {protocol_source}")
            compile_and_run(cxx, out / test.stem, [test, protocol_source], hall)

    print("native backend checks: all static and portable C++ tests passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode)
    except subprocess.TimeoutExpired as exc:
        raise SystemExit(f"TEST/BUILD TIMEOUT (not a pass): {exc.cmd}")
