#!/usr/bin/env python3
"""Verify the V14-10D bounded overlay concurrency and origin contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]


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
    overlay = (ROOT / "HallJoy" / "overlay_server.cpp").read_text(encoding="utf-8-sig")
    simulator = (REPO / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8-sig")
    production = (REPO / "tools" / "run_production_smoke.ps1").read_text(encoding="utf-8-sig")
    build = (REPO / "tools" / "build.ps1").read_text(encoding="utf-8-sig")

    accept_worker = function_body(overlay, "static DWORD OverlayThreadBody(SOCKET listenSocket)")
    client_start = function_body(overlay, "static bool OverlayStartClientWorker(SOCKET client)")
    client_entry = function_body(overlay, "static DWORD WINAPI OverlayClientThreadProc(LPVOID param) noexcept")
    join_clients = function_body(overlay, "static bool OverlayJoinAllClientWorkers()")
    stop = function_body(overlay, "halljoy::lifecycle::StopResult OverlayServer_Stop()")

    require("kOverlayMaxConcurrentClients = 16" in overlay and
            "std::array<OverlayClientSlot, kOverlayMaxConcurrentClients>" in overlay,
            "overlay uses a fixed 16-client ownership table")
    require("CreateThread" in client_start and "OverlayClientThreadProc" in client_start,
            "accept worker delegates clients to independent bounded workers")
    require("RunWorkerEntryBarrier" in client_entry and "OverlayClientThreadOnFault" in client_entry,
            "every client worker has a C++ exception boundary")
    require("OverlayReapCompletedClientWorkers" in accept_worker and
            "503 Service Unavailable" in accept_worker,
            "completed slots are reaped and saturation is rejected promptly")
    require("WaitForMultipleObjects" in join_clients and "TRUE, INFINITE" in join_clients,
            "accept worker retains ownership until all client workers complete")
    require("OverlayShutdownClientSockets" in stop and
            stop.index("OverlayShutdownClientSockets") < stop.index("WaitForSingleObject"),
            "stop wakes every active client before joining the accept worker")
    require("client_workers_retained=%u" in stop and "wsa_retained=1" in stop,
            "incomplete join truthfully retains client and WSA ownership")

    require("BCryptGenRandom" in overlay and "randomBytes.size() * 2u" in overlay,
            "each server generation creates a 128-bit CSPRNG session token")
    require("HttpOnly; SameSite=Strict" in overlay and "OverlayRequestHasSessionCookie" in overlay,
            "state and telemetry require a hardened session cookie")
    require("response.status===401" in overlay and
            "await fetch('/',{cache:'no-store'})" in overlay,
            "an open browser overlay refreshes a stale generation cookie")
    require("OverlayOriginAllowed" in overlay and '"http://127.0.0.1:"' in overlay,
            "browser origins are restricted to the exact bound loopback origin")
    require("Access-Control-Allow-Origin: *" not in overlay,
            "wildcard CORS access is absent")
    require("Vary: Origin" in overlay and "403 Forbidden" in overlay and "401 Unauthorized" in overlay,
            "origin and session failures have explicit closing responses")

    for runner, name in ((simulator, "simulator"), (production, "production")):
        require("check_overlay_concurrency_origin.py" in runner,
                f"{name} smoke runs concurrency/origin socket regressions")
    require("shutdown-probe-clients" in simulator,
            "simulator shutdown holds multiple clients until server stop")
    require("overlay_concurrency_origin_static_audit.py" in build and
            "check_overlay_concurrency_origin.py" in build,
            "official build requires V14-10D regression assets")

    print("OVERLAY_CONCURRENCY_ORIGIN_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
