#!/usr/bin/env python3
"""Guard the physically corrected W669 streaming architecture."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
backend = (root / "HallJoy" / "aula_w669_backend.cpp").read_text(encoding="utf-8")
protocol = (root / "HallJoy" / "aula_w669_protocol.cpp").read_text(encoding="utf-8")

checks = {
    "runtime never requests the sensor-domain snapshot": "BuildSnapshotRequest" not in backend,
    "runtime never decodes the sensor-domain snapshot": "DecodeSnapshotPacket" not in backend,
    "session declares one live-only publication strategy":
        "strategy=live_subscription_only snapshot_publish=disabled" in backend,
    "idle cancellation preserves the original wait timeout":
        "wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError()" in backend,
    "only non-timeout receive failures increment the session counter":
        "if (!received && GetLastError() != WAIT_TIMEOUT) g_failures.fetch_add(1);" in backend,
    "firmware-default poll code is accepted without a fabricated rate":
        "case 0: hz = 0; break;" in protocol and
        "nominal_poll_hz=unspecified mode=firmware_default" in backend,
    "official firmware product identity is queried read-only before mapping":
        "BuildDeviceInfoRequest" in backend and
        "DecodeDeviceInfo" in backend and
        "source=read_only_0x0D" in backend,
    "all official Standard/W669 geometries have distinct factory profiles":
        "Si2825Win60" in protocol and
        "Si2828Win68" in protocol and
        "Si2851KpTe153Uk" in protocol and
        "Win68FactoryMap" in protocol and
        "KpTe153UkFactoryMap" in protocol,
    "unknown products never inherit a guessed known factory layout":
        "default: return {};" in protocol and
        "unknown_explicit_only" in backend,
    "runtime session must preserve the proved firmware identity":
        "identityStable" in backend and
        "sessionInfo.product.data(), proof.deviceInfo.product.data()" in backend,
}

failed = False
for name, passed in checks.items():
    print(f"{'PASS' if passed else 'FAIL'}: {name}")
    failed |= not passed

if failed:
    raise SystemExit(1)
print("AULA_W669_BACKEND_STATIC_AUDIT=PASS")
