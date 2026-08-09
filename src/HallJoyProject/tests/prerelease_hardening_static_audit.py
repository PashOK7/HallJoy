#!/usr/bin/env python3
"""Preserve the analyzer, fuzzing and long-uptime hardening gates."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
TOOLS = ROOT / "tools"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


keyboard = read(HALL / "keyboard_subpages.cpp")
main = read(HALL / "main.cpp")
win_util = read(HALL / "win_util.cpp")
host = read(HALL / "analog_host_client.cpp")
realtime = read(HALL / "realtime_loop.cpp")
app_paths = read(HALL / "app_paths.cpp")
debug_log = read(HALL / "debug_log.cpp")
addressed = read(HALL / "addressed_analog_backend.cpp")
spark = read(HALL / "backend_sparklink.inc")
sayo = read(HALL / "backend_sayo.inc")
hex80 = read(HALL / "hex80_backend.cpp")
mad68 = read(HALL / "mad68pr_backend.cpp")
aula_protocol = read(HALL / "aula_win60he_protocol.cpp")
aula_policy = read(HALL / "aula_win60he_session_policy.cpp")
native_runner = read(TOOLS / "run_native_backend_checks.py")
sanitizer_runner = read(TOOLS / "run_protocol_fuzz_sanitizers.py")
overlay_runner = read(TOOLS / "run_production_smoke.ps1")
build = read(TOOLS / "build.ps1")

require(keyboard.count("ULONGLONG toastHideAt") == 2 and
        keyboard.count("toastHideAt = GetTickCount64() + TOAST_SHOW_MS") == 2,
        "absolute UI toast deadlines remain 64-bit across 49-day uptime")
require("if (!u32)\n        return;" in main and
        "if (!u32)\n        return 96;" in win_util,
        "dynamic user32 lookups reject an impossible null module handle")
require("if (!pi.hProcess || !pi.hThread)" in host and
        "TerminateProcess(pi.hProcess, invalidProcessInfo)" in host,
        "impossible partial CreateProcess success is contained defensively")
require("std::vector<unsigned char> leftBuffer(64 * 1024)" in app_paths and
        "std::vector<unsigned char> rightBuffer(64 * 1024)" in app_paths,
        "file comparison buffers no longer consume 128 KiB of stack")
require("std::vector<wchar_t> path(32768)" in debug_log and
        "std::vector<wchar_t> path(32768)" in addressed and
        "std::vector<wchar_t> buffer(32768)" in mad68 and
        "std::vector<wchar_t> exePath(32768)" in host,
        "large executable-path scratch buffers are heap-backed")
require("std::make_unique<addressed::PollScheduler>" in addressed and
        "g_scheduler = scheduler.get()" in addressed,
        "Addressed's 255-key scheduler no longer consumes worker stack")
require("struct HostPollBuffers" in host and
        "std::make_unique<HostPollBuffers>()" in host,
        "isolated UAP host poll snapshots are heap-backed")
require("struct RealtimeTraceBuffers" in realtime and
        "std::make_unique<RealtimeTraceBuffers>()" in realtime,
        "optional realtime percentile samples are absent from the normal stack")
require(all("const HANDLE wakeEvent = g_wakeEvent" in source for source in
            (addressed, hex80, mad68)) and
        all("worker started without wake event" in source for source in
            (addressed, hex80, mad68)),
        "native workers pin and validate their wake handle before polling")
require(addressed.count("if (!HidD_SetNumInputBuffers") == 2 and
        spark.count("if (!HidD_SetNumInputBuffers") == 1 and
        sayo.count("if (!HidD_SetNumInputBuffers") == 1 and
        hex80.count("if (!HidD_SetNumInputBuffers") == 1 and
        "buffersOk = HidD_SetNumInputBuffers" in mad68,
        "optional HID input-buffer tuning failures are observed and diagnosed")
require("protocol_parser_fuzz_smoke_test.cpp" in native_runner and
        "250000" in read(ROOT / "src" / "HallJoyProject" / "tests" /
                          "protocol_parser_fuzz_smoke_test.cpp"),
        "unified native tests retain deterministic random and mutation fuzzing")
require("static_cast<std::ptrdiff_t>" in aula_protocol and
        "report * kWireReportBytes" in aula_protocol,
        "Aula report flattening uses a defined signed iterator offset")
require("struct CanonicalSerial" in aula_policy and
        "std::string value" not in aula_policy and
        "std::make_unique" not in aula_policy,
        "noexcept Aula serial identity canonicalization is allocation-free")
require("-fsanitize=address,undefined" in sanitizer_runner and
        "protocol_parser_fuzz_smoke_test.cpp" in sanitizer_runner,
        "all standalone protocol parsers retain ASan and UBSan coverage")
require("fuzz_overlay_http.py" in overlay_runner and
        "--iterations $OverlayFuzzIterations --workers 8" in overlay_runner,
        "production overlay smoke retains bounded parallel malformed-input fuzzing")
require("run_protocol_fuzz_sanitizers.py" in build and
        "fuzz_overlay_http.py" in build and
        "prerelease_hardening_static_audit.py" in build,
        "official source manifest requires every prerelease hardening gate")

print("PRERELEASE_HARDENING_STATIC_AUDIT=PASS")
