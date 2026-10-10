#!/usr/bin/env python3
"""Run HallJoy native-backend architecture audits and portable C++ tests.

This script is intentionally independent of Visual Studio. BUILD.cmd runs the same
static audits before the full MSVC build; contributors can use this runner on
Linux/macOS/MinGW while developing parsers and schedulers.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time
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


def compile_command(cxx: str, output: Path, sources: list[Path], include: Path) -> list[str]:
    extra_compile_args = (
        ["-DHALLJOY_VIGEM_TRANSPORT_FAKE_ONLY"]
        if output.name == "vigem_child_transport"
        else []
    )
    if output.name == "instance_guard_windows":
        extra_compile_args += ["-DHALLJOY_INSTANCE_GUARD_TEST"]
    if output.name == "input_path_log_windows":
        extra_compile_args += ["-DHALLJOY_INPUT_PATH_DIAGNOSTIC"]
    if output.name == "aula_mini60_log_windows":
        extra_compile_args += ["-DHALLJOY_AULA_MINI60_DIAGNOSTIC", "-DHALLJOY_DIAGNOSTIC", "-DHALLJOY_STABILITY_TRACE"]
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
        *(["-lsetupapi", "-lcfgmgr32", "-lhid", "-lshell32", "-lole32", "-luuid"] if output.name in ("support_log_windows", "input_path_log_windows", "native_layout_devices_windows", "mad68_dual_trial_session", "mchose_mix87_session", "mchose_jet75_session", "alumix104_session") else []),
        *(["-lbcrypt"] if output.name == "mchose_mix87_session" else []),
        "-o",
        str(output),
    ]
    return command


# Test-binary cache. A binary is reused only when the compiler executable, the
# full command line and the bytes of every file the compiler read (its -MD
# dependency list, system headers included) are identical. Every test still
# RUNS each time; only an identical recompilation is skipped.
# HALLJOY_NO_TEST_CACHE=1 forces fresh compilation.
CACHE_DIR = Path(__file__).resolve().parents[1] / "build" / "obj" / "portable-tests" / "cache"
ISOLATED_LONG_TESTS = {"support_log_windows", "input_path_log_windows", "process_generation_supervisor"}


def _digest_files(paths: list[str]) -> str | None:
    h = hashlib.sha256()
    for name in paths:
        try:
            h.update(name.encode("utf-8", "surrogateescape") + b"\0" + Path(name).read_bytes() + b"\0")
        except OSError:
            return None
    return h.hexdigest()


def _parse_depfile(text: str) -> list[str]:
    text = text.replace("\\\r\n", " ").replace("\\\n", " ")
    _, _, deps = text.partition(": ")
    items, current, i = [], "", 0
    while i < len(deps):
        c = deps[i]
        if c == "\\" and i + 1 < len(deps) and deps[i + 1] == " ":
            current += " "; i += 2; continue
        if c.isspace():
            if current: items.append(current); current = ""
        else:
            current += c
        i += 1
    if current: items.append(current)
    return sorted(set(items))


def _cache_key(command: list[str], output: Path) -> str:
    h = hashlib.sha256()
    compiler = Path(shutil.which(command[0]) or command[0])
    # Cache format 2: dependencies of every translation unit (format 1 kept only the
    # last unit's list for multi-source tests, so their binaries went stale).
    h.update(b"deps-per-unit-v2\0")
    try:
        st = compiler.stat(); h.update(f"{compiler}|{st.st_size}|{st.st_mtime_ns}".encode())
    except OSError:
        h.update(str(compiler).encode())
    for arg in command[1:]:
        h.update(("<out>" if arg == str(output) else arg).encode("utf-8", "surrogateescape") + b"\0")
    return h.hexdigest()[:32]


def _unit_dependencies(command: list[str], output: Path) -> list[str] | None:
    """Dependencies of every source on the command line, one -M pass per unit.

    A single -MD -MF for a multi-source compile is rewritten by each unit, so only
    the last unit's dependencies survived. None means: do not cache this binary."""
    sources = [a for a in command[1:] if a.lower().endswith((".cpp", ".cc", ".c")) and Path(a).is_file()]
    flags = [a for a in command[1:-2] if a not in sources and not a.startswith("-l")]
    deps: set[str] = set()
    for index, source in enumerate(sources):
        depfile = output.with_name(f"{output.name}.{index}.d")
        result = subprocess.run([command[0], *flags, "-M", "-MF", str(depfile), source],
                                capture_output=True, text=True, timeout=300)
        if result.returncode != 0 or not depfile.is_file():
            return None
        unit = _parse_depfile(depfile.read_text(encoding="utf-8", errors="surrogateescape"))
        if not any(Path(d).resolve() == Path(source).resolve() for d in unit):
            return None
        deps.update(unit)
    return sorted(deps) if sources else None


def _build(command: list[str], output: Path) -> str:
    """Compile (or reuse an identical cached binary); returns a log line."""
    use_cache = os.environ.get("HALLJOY_NO_TEST_CACHE") != "1"
    exe = output.with_suffix(".exe") if os.name == "nt" and output.suffix != ".exe" else output
    key = _cache_key(command, output)
    entry = CACHE_DIR / key
    if use_cache and (entry / "deps.txt").is_file() and (entry / "bin").is_file():
        deps = (entry / "deps.txt").read_text(encoding="utf-8").splitlines()
        if (entry / "digest.txt").read_text(encoding="utf-8") == _digest_files(deps):
            target = output.with_name((entry / "name.txt").read_text(encoding="utf-8"))
            shutil.copyfile(entry / "bin", target)
            return f"= cached (identical inputs) {output.name}"
    started = time.time_ns() - 2_000_000_000  # filesystem timestamp slack
    result = subprocess.run(command, capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        raise RuntimeError("+ " + " ".join(command) + "\n" + result.stdout + result.stderr)
    log = "+ " + " ".join(command) + ("\n" + (result.stdout + result.stderr).rstrip() if (result.stdout + result.stderr).strip() else "")
    built = exe if exe.is_file() else output
    deps = _unit_dependencies(command, output) if use_cache and built.is_file() else None
    if deps:
        digest = _digest_files(deps)
        try:  # an input edited during compilation must never be cached
            stable = all(Path(d).stat().st_mtime_ns < started for d in deps)
        except OSError:
            stable = False
        if digest and stable:
            tmp = CACHE_DIR / (key + ".tmp" + str(os.getpid()) + "_" + output.name)
            shutil.rmtree(tmp, ignore_errors=True); tmp.mkdir(parents=True)
            (tmp / "deps.txt").write_text("\n".join(deps), encoding="utf-8")
            (tmp / "digest.txt").write_text(digest, encoding="utf-8")
            (tmp / "name.txt").write_text(built.name, encoding="utf-8")
            shutil.copyfile(built, tmp / "bin")
            shutil.rmtree(entry, ignore_errors=True)
            try: tmp.rename(entry)
            except OSError: shutil.rmtree(tmp, ignore_errors=True)
    return log


def compile_and_run_many(cxx: str, out: Path, tests: list[tuple[str, list[Path]]], include: Path) -> None:
    """Compile all tests in parallel, then run each binary sequentially."""
    jobs = [(name, compile_command(cxx, out / name, sources, include)) for name, sources in tests]
    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, os.cpu_count() or 1)) as pool:
        futures = {pool.submit(_build, command, out / name): name for name, command in jobs}
        for future in concurrent.futures.as_completed(futures):
            try:
                print(future.result(), flush=True)
            except Exception as exc:  # report every failing compile, then fail
                failures.append(f"{futures[future]}: {exc}")
    if failures:
        print("\n".join(failures), flush=True)
        raise subprocess.CalledProcessError(1, "compile: " + ", ".join(f.split(":")[0] for f in failures))
    # Long tests that wait on real timers and use only private temp directories
    # and unnamed kernel objects run alongside the ordered sequential tests.
    background = {}
    for name, _ in jobs:
        if name in ISOLATED_LONG_TESTS:
            print("+ (parallel) " + str(out / name), flush=True)
            background[name] = subprocess.Popen([str(out / name)], stdout=subprocess.PIPE,
                                                stderr=subprocess.STDOUT, text=True, errors="replace")
    try:
        for name, _ in jobs:
            if name not in background:
                run([str(out / name)])
    finally:
        failed = []
        for name, process in background.items():
            try:
                output, _ = process.communicate(timeout=120)
            except subprocess.TimeoutExpired:
                process.kill(); process.communicate()
                raise subprocess.TimeoutExpired(str(out / name), 120)
            print(f"= {name}:\n{output.rstrip()}", flush=True)
            if process.returncode != 0:
                failed.append(name)
        if failed:
            raise subprocess.CalledProcessError(1, "tests: " + ", ".join(failed))


def compile_and_run(cxx: str, output: Path, sources: list[Path], include: Path) -> None:
    compile_and_run_many(cxx, output.parent, [(output.name, sources)], include)


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
    run([sys.executable, str(root / "tools" / "research_reference_checks.py")])
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
            ("diagnostic_rate_limit", [tests / "diagnostic_rate_limit_test.cpp"]),
            ("aula_mini60_native_model", [tests / "aula_mini60_native_model_test.cpp"]),
            ("attackshark_pro_diagnostic_model", [tests / "attackshark_pro_diagnostic_model_test.cpp"]),
            ("attackshark_pro_layout", [tests / "attackshark_pro_layout_test.cpp"]),
            ("public_diagnostic_fields", [tests / "public_diagnostic_fields_test.cpp"]),
            ("irok_na87_protocol", [tests / "irok_na87_protocol_test.cpp", hall / "irok_nd75_protocol.cpp"]),
            ("window_placement", [tests / "window_placement_test.cpp"]),
            ("overlay_text_edit", [tests / "overlay_text_edit_test.cpp"]),
            ("layout_editor_model", [tests / "layout_editor_model_test.cpp"]),
            ("layout_sort", [tests / "layout_sort_test.cpp"]),
            ("layout_identity", [tests / "layout_identity_test.cpp"]),
            ("remap_hint_motion", [tests / "remap_hint_motion_test.cpp"]),
            ("digital_keyboard_state", [tests / "digital_keyboard_state_test.cpp"]),
            ("input_privilege_warning", [tests / "input_privilege_warning_test.cpp"]),
            ("wooting_physical_keys", [tests / "wooting_physical_keys_test.cpp",
                hall / "provider_v2_controller_shadow.cpp", hall / "analog_provider_v2.cpp",
                hall / "configured_xusb_builder.cpp", hall / "bindings.cpp"]),
            ("block_keys_policy", [tests / "block_keys_policy_test.cpp"]),
            ("profile_runtime_gate", [tests / "profile_runtime_gate_test.cpp"]),
            ("game_profile_rules", [tests / "game_profile_rules_test.cpp"]),
            ("tab_transition_motion", [tests / "tab_transition_motion_test.cpp"]),
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
            ("ipi_native", [tests / "ipi_native_test.cpp"]),
            ("native_layout_state", [tests / "native_layout_state_test.cpp"]),
            ("coherent_telemetry_cache", [tests / "coherent_telemetry_cache_test.cpp"]),
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
            ("keychron_hj_protocol", [tests / "keychron_hj_protocol_test.cpp"]),
            ("gamepad_latency_model", [tests / "gamepad_latency_model_test.cpp"]),
            ("keychron_onboard_precision", [tests / "keychron_onboard_precision_test.cpp"]),
            ("keychron_onboard_session", [tests / "keychron_onboard_session_test.cpp"]),
            ("keychron_onboard_compact", [tests / "keychron_onboard_compact_test.cpp"]),
            ("keychron_onboard_client", [tests / "keychron_onboard_client_test.cpp"]),
            ("keychron_onboard_profile", [tests / "keychron_onboard_profile_test.cpp"]),
            ("keychron_onboard_profile_v2", [tests / "keychron_onboard_profile_v2_test.cpp"]),
            ("keychron_onboard_mapper", [tests / "keychron_onboard_mapper_test.cpp", hall / "configured_xusb_builder.cpp"]),
            ("keychron_onboard_multikey", [tests / "keychron_onboard_multikey_test.cpp", hall / "configured_xusb_builder.cpp"]),
            ("keychron_onboard_stray_report", [tests / "keychron_onboard_stray_report_test.cpp"]),
            ("uap_snapshot_pinning", [tests / "uap_snapshot_pinning_test.cpp"]),
            ("windows_command_line", [tests / "windows_command_line_test.cpp"]),
            ("mg75_pro_protocol", [tests / "mg75_pro_protocol_test.cpp"]),
            ("tartarus_protocol", [tests / "tartarus_protocol_test.cpp", hall / "keyboard_support_status.cpp"]),
            ("mchose_mix87_protocol", [tests / "mchose_mix87_protocol_test.cpp"]),
            ("mchose_jet75_protocol", [tests / "mchose_jet75_protocol_test.cpp"]),
            ("logitech_rapid_protocol", [tests / "logitech_rapid_protocol_test.cpp"]),
            ("redsquare_alumix68_protocol", [tests / "redsquare_alumix68_protocol_test.cpp"]),
            ("mad68_dual_trial_protocol", [tests / "mad68_dual_trial_protocol_test.cpp"]),
            ("input_shortcuts", [tests / "input_shortcuts_test.cpp"]),
            ("neo65_protocol", [tests / "neo65_protocol_test.cpp"]),
            ("steelseries_apex_protocol", [tests / "steelseries_apex_protocol_test.cpp"]),
            ("uap_discovery_policy", [tests / "uap_discovery_policy_test.cpp"]),
            ("sparklink_model_profiles", [tests / "sparklink_model_profiles_test.cpp"]),
            ("rongyuan_stream_protocol", [tests / "rongyuan_stream_protocol_test.cpp", hall / "keyboard_support_status.cpp"]),
            ("three_keyboard_protocol", [tests / "three_keyboard_protocol_test.cpp", hall / "keyboard_support_status.cpp"]),
            ("sparklink_hotplug_age", [tests / "sparklink_hotplug_age_test.cpp"]),
            ("sparklink_row_freshness", [tests / "sparklink_row_freshness_test.cpp"]),
            ("sayo_letter_matcher", [tests / "sayo_letter_matcher_test.cpp"]),
            ("sayo_o3c", [tests / "sayo_o3c_test.cpp"]),
            ("sparkplayjoy_layout", [tests / "sparkplayjoy_layout_test.cpp", hall / "aula_win60he_protocol.cpp"]),
            ("keyboard_support_status", [
                tests / "keyboard_support_status_test.cpp",
                hall / "keyboard_support_status.cpp",
            ]),
            ("runtime_arithmetic", [tests / "runtime_arithmetic_test.cpp"]),
            ("runtime_command_state", [tests / "runtime_command_state_test.cpp"]),
            ("curve_math", [tests / "curve_math_test.cpp", hall / "curve_math.cpp"]),
            ("engine_runtime_transaction", [tests / "engine_runtime_transaction_test.cpp"]),
            ("raw_input_packet_size", [tests / "raw_input_packet_size_test.cpp"]),
            ("extended_key_bindings", [
                tests / "extended_key_bindings_test.cpp",
                hall / "bindings.cpp",
            ]),
            ("axis_multikey_bindings", [
                tests / "axis_multikey_bindings_test.cpp",
                hall / "bindings.cpp",
            ]),
            ("axis_most_pressed_builder", [
                tests / "axis_most_pressed_builder_test.cpp",
                hall / "configured_xusb_builder.cpp",
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
            fixed_tests.append(("native_analog_telemetry_collect", [tests / "native_analog_telemetry_collect_test.cpp"]))
            fixed_tests.append(("pause_hotkeys_windows", [tests / "pause_hotkeys_windows_test.cpp"]))
            fixed_tests.append(("hid_io_control", [tests / "hid_io_control_test.cpp"]))
            fixed_tests.append(("keychron_onboard_host_profile", [tests / "keychron_onboard_host_profile_test.cpp", hall / "keychron_onboard_host_profile.cpp", hall / "bindings.cpp", hall / "key_settings.cpp", hall / "backend_curve.cpp", hall / "settings.cpp", hall / "curve_math.cpp"]))
            fixed_tests.append(("keychron_onboard_curve", [tests / "keychron_onboard_curve_test.cpp", hall / "key_settings.cpp", hall / "backend_curve.cpp", hall / "settings.cpp", hall / "curve_math.cpp"]))
            fixed_tests.append(("aula_mini60_log_windows", [
                tests / "aula_mini60_log_windows_test.cpp", hall / "stability_trace.cpp"
            ]))
            # Production settings expose Win32 types; test the actual linked
            # implementation on Windows, keeping pure curve math portable.
            fixed_tests.append(("key_settings_domain", [
                tests / "key_settings_domain_test.cpp", hall / "key_settings.cpp",
                hall / "backend_curve.cpp", hall / "settings.cpp", hall / "curve_math.cpp",
            ]))
            # This suite now includes real Win32 INI file roundtrips, not only
            # the original platform-independent numeric conversion cases.
            fixed_tests.append(("native_layout_devices_windows", [tests / "native_layout_devices_windows_test.cpp", hall / "native_layout_devices.cpp"]))
            fixed_tests.append(("bounded_ini_numeric", [tests / "bounded_ini_numeric_test.cpp"]))
            fixed_tests.append(("layout_ini_section", [tests / "layout_ini_section_windows_test.cpp"]))
            fixed_tests.append(("ini_read_snapshot", [tests / "ini_read_snapshot_windows_test.cpp"]))
            fixed_tests.append(("ini_write_batch", [tests / "ini_write_batch_windows_test.cpp"]))
            # Uses the real Windows ViGEm SDK ABI (including Windows packing
            # headers), even with fake device calls. Keep mandatory coverage in
            # the Windows build rather than substituting a fake SDK on Linux.
            fixed_tests.append(("vigem_child_transport", [
                tests / "vigem_child_transport_test.cpp",
                hall / "vigem_child_transport.cpp",
            ]))
            fixed_tests.append(("block_keys_hotkey_windows", [tests / "block_keys_hotkey_windows_test.cpp"]))
            fixed_tests.append(("input_path_log_windows", [
                tests / "support_log_windows_test.cpp", hall / "support_log.cpp"
            ]))
            fixed_tests.append(("mchose_mix87_session", [tests / "mchose_mix87_session_test.cpp"]))
            fixed_tests.append(("mchose_jet75_session", [tests / "mchose_jet75_session_test.cpp"]))
            fixed_tests.append(("mad68_dual_trial_session", [
                tests / "mad68_dual_trial_session_test.cpp"
            ]))
            fixed_tests.append(("alumix104_session", [
                tests / "alumix104_session_test.cpp", hall / "addressed_poll_scheduler.cpp"
            ]))
            fixed_tests.append(("support_log_windows", [
                tests / "support_log_windows_test.cpp", hall / "support_log.cpp"
            ]))
            # The bridge implementation includes <windows.h>: not portable.
            fixed_tests.append(("engine_runtime_ui_bridge", [tests / "engine_runtime_ui_bridge_test.cpp"]))
            fixed_tests.append(("engine_runtime_notification_windows", [
                tests / "engine_runtime_notification_windows_test.cpp", hall / "engine_runtime_owner.cpp", hall / "perf_trace.cpp"
            ]))
            fixed_tests.append(("debug_event_handles_windows", [
                tests / "debug_event_handles_windows_test.cpp"
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
        all_tests = list(fixed_tests)
        # Convention used by built-ins and tools/new_native_backend.py:
        # tests/<name>_protocol_test.cpp links HallJoy/<name>_protocol.cpp.
        explicit_sources = {source.resolve() for _, sources in fixed_tests for source in sources}
        for test in sorted(tests.glob("*_protocol_test.cpp")):
            if test.resolve() in explicit_sources:
                continue  # Already linked with its explicitly declared dependencies.
            protocol_source = hall / test.name.replace("_test.cpp", ".cpp")
            if not protocol_source.exists():
                raise SystemExit(f"Missing pure protocol source for {test.name}: {protocol_source}")
            all_tests.append((test.stem, [test, protocol_source]))
        compile_and_run_many(cxx, out, all_tests, hall)

    print("native backend checks: all static and portable C++ tests passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode)
    except subprocess.TimeoutExpired as exc:
        raise SystemExit(f"TEST/BUILD TIMEOUT (not a pass): {exc.cmd}")
