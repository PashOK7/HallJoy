#!/usr/bin/env python3
"""Verify the V14-06C overlay cooperative-shutdown ownership contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    require(start >= 0, f"found function: {signature}")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:index + 1]
    raise SystemExit(f"FAIL: unterminated function: {signature}")


def main() -> int:
    overlay = (HALL / "overlay_server.cpp").read_text(encoding="utf-8-sig")
    header = (HALL / "overlay_server.h").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    simulator_runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")

    start = function_body(overlay, "bool OverlayServer_Start(uint16_t port)")
    stop = function_body(overlay, "halljoy::lifecycle::StopResult OverlayServer_Stop()")
    thread_body = function_body(overlay, "static DWORD OverlayThreadBody(SOCKET listenSocket)")
    handle_request = function_body(overlay, "static bool OverlayHandleClientRequest(")
    handle_client = function_body(overlay, "static void OverlayHandleClient(SOCKET client)")

    require("TerminateThread" not in overlay,
            "overlay has no forced thread termination")
    require("StopResult OverlayServer_Stop()" in header,
            "public stop contract reports exact completion")
    require("BeginStart" in start and "ConfirmRunning" in start and "FailStartBeforeWorker" in start,
            "start publishes one truthful lifecycle generation")
    require("g_overlayLifecycleMutex" in start and "g_overlayLifecycleMutex" in stop,
            "start and stop transitions are serialized")
    require("shutdown(g_overlayClientSocket" in stop and "closesocket(listenSocket)" in stop and
            stop.index("closesocket(listenSocket)") < stop.index("WaitForSingleObject"),
            "stop wakes accept and recv before joining")
    require("ObserveWorkerJoin" in stop,
            "stop uses the common join policy")
    require("MarkPoisoned" in stop and "thread_handle_retained=1" in stop and
            "wsa_retained=1" in stop and "restart_blocked=1" in stop,
            "incomplete join retains ownership and poisons restart")
    require(stop.index("if (!observedJoin.Completed())") < stop.index("CloseHandle(g_overlayThread)"),
            "thread handle closes only after confirmed completion")
    require(stop.index("if (!observedJoin.Completed())") < stop.index("WSACleanup()"),
            "WSA cleanup occurs only after confirmed completion")
    require("overlayStop.RestartSafe()" in app and "dependent_cleanup_skipped=1" in app,
            "final shutdown skips dependent cleanup after overlay poison")
    require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in thread_body and
            "--halljoy-test-overlay-stop-timeout" in thread_body,
            "runtime timeout injection is simulator-only")
    require("InjectOverlayStopTimeout" in simulator_runner and
            "--overlay-server" in simulator_runner and "expected 2" in simulator_runner,
            "simulator runner starts overlay and verifies process containment")
    require('path == "/client_perf"' in handle_request and
            "closeAfterResponse = true" in handle_request and
            '"204 No Content"' in handle_request and
            '"", false' in handle_request,
            "periodic client telemetry explicitly closes its response connection")
    require("!ok || !request.keepAlive || closeAfterResponse" in handle_client,
            "server honors response-directed close without a five-second idle wait")
    require("StartOverlay" in simulator_runner and "check_overlay_responsiveness.py" in simulator_runner and
            "[component=overlay][event=stop.end]" in simulator_runner,
            "simulator runner verifies HTTP responsiveness and graceful join")

    print("OVERLAY_COOPERATIVE_SHUTDOWN_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
