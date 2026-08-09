#!/usr/bin/env python3
"""Verify the V14-10C bounded overlay HTTP framing contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    overlay = (ROOT / "HallJoy" / "overlay_server.cpp").read_text(encoding="utf-8-sig")
    runner = (REPO / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8-sig")
    production_smoke = (REPO / "tools" / "run_production_smoke.ps1").read_text(encoding="utf-8-sig")
    build = (REPO / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
    socket_test = REPO / "tools" / "check_overlay_http_framing.py"

    require("kOverlayMaxHeaderBytes = 8u * 1024u" in overlay,
            "request headers are limited to 8 KiB")
    require("kOverlayMaxBodyBytes = 4u * 1024u" in overlay,
            "request bodies are limited to 4 KiB")
    require("kOverlayMaxTargetBytes = 2u * 1024u" in overlay,
            "request targets are limited to 2 KiB")
    require("OverlayParseHttpRequest" in overlay and "OverlayHttpParseResult::NeedMore" in overlay,
            "HTTP parsing distinguishes incomplete input from rejection")
    require("std::string buffered" in overlay and "buffered.append" in overlay,
            "client handler accumulates fragmented recv data")
    require("request.frameBytes" in overlay and "buffered.erase(0, request.frameBytes)" in overlay,
            "client handler consumes exactly one request frame")
    require('OverlayAsciiEquals(name, "transfer-encoding")' in overlay and
            "implements only fixed-length framing" in overlay,
            "unsupported transfer coding is rejected")
    require("std::from_chars" in overlay and "parsed.ptr != end" in overlay,
            "numeric fields require complete overflow-safe conversion")
    require("kOverlayMaxClientMetric = 1000000000ull" in overlay,
            "client telemetry has an explicit upper bound")
    require("value = value * 10u" not in overlay,
            "wrapping decimal accumulator is absent")
    require('"invalid telemetry", false' in overlay,
            "invalid telemetry receives a closing error response")
    require(socket_test.is_file(), "socket framing regression tool is packaged")
    require("check_overlay_http_framing.py" in runner,
            "simulator overlay scenario runs socket framing regressions")
    require("StartOverlay" in production_smoke and "check_overlay_http_framing.py" in production_smoke,
            "production smoke can run socket framing regressions")
    require("check_overlay_http_framing.py" in build and
            "overlay_http_framing_static_audit.py" in build,
            "release preflight requires framing regression assets")

    print("OVERLAY_HTTP_FRAMING_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
