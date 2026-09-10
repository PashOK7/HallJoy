#!/usr/bin/env python3
"""Fail closed when the current release-risk/test manifest is incomplete."""

from __future__ import annotations

import json
import re
from pathlib import Path


repo = Path(__file__).resolve().parents[3]
manifest_path = repo / "docs" / "v1.4" / "RISK_TEST_MANIFEST_V1.json"
risk_register_path = repo / "docs" / "v1.4" / "RISK_REGISTER.md"
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
risk_register = risk_register_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def ledger_statuses() -> dict[str, str]:
    result: dict[str, str] = {}
    for line in risk_register.splitlines():
        if not line.startswith("| `HJ-V14-"):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != 6:
            continue
        risk_id = cells[0].strip("`")
        result[risk_id] = cells[4]
    return result


ledger = ledger_statuses()
expected_risks = {f"HJ-V14-P0-{number:03d}" for number in range(1, 7)}
expected_risks.update(f"HJ-V14-P1-{number:03d}" for number in range(9, 40))
expected_risks.add("HJ-V14-P1-008")

require(manifest.get("schema") == "halljoy.risk-test-manifest.v1",
        "risk/test manifest schema is explicit")
require(all(risk_id in ledger for risk_id in expected_risks),
        "every scoped risk still exists in the authoritative ledger")
require(all(not re.match(r"^(Verified|Closed)\b", ledger[risk_id])
            for risk_id in expected_risks),
        "manifest scope contains only unresolved or partially resolved risks")

risks = manifest.get("risks", [])
risk_ids = [entry.get("id") for entry in risks]
require(len(risk_ids) == len(set(risk_ids)), "risk IDs are unique")
require(set(risk_ids) == expected_risks,
        "manifest covers the complete current P0/P1 integration scope")

allowed_risk_statuses = {
    "PASS", "MISSING", "OLD_BUG_CONFIRMED", "FAIL", "HARDWARE_PENDING"
}
for entry in risks:
    require(entry.get("status") in allowed_risk_statuses,
            f"{entry['id']} has a fail-closed status")
    require(isinstance(entry.get("gate_ids"), list),
            f"{entry['id']} declares its gate IDs")
    if entry["status"] != "PASS":
        require(bool(entry.get("gap")), f"{entry['id']} names its blocking gap")

gates = manifest.get("gates", [])
gate_ids = [entry.get("id") for entry in gates]
require(len(gate_ids) == len(set(gate_ids)), "gate IDs are unique")
gate_by_id = {entry["id"]: entry for entry in gates}
for entry in risks:
    for gate_id in entry["gate_ids"]:
        require(gate_id in gate_by_id,
                f"{entry['id']} references an existing gate {gate_id}")

required_gate_fields = {
    "id", "risk_ids", "kind", "status", "command", "required_environment",
    "production_components", "negative_oracle", "timeout_seconds",
    "expected_exit_classification", "source_manifest", "artifact_hash", "evidence"
}
for gate in gates:
    require(required_gate_fields <= set(gate),
            f"{gate['id']} has the complete executable-gate schema")
    require(gate["status"] in allowed_risk_statuses,
            f"{gate['id']} has a fail-closed status")
    require(isinstance(gate["timeout_seconds"], int) and gate["timeout_seconds"] > 0,
            f"{gate['id']} has a bounded timeout")
    require(bool(gate["command"]) and bool(gate["required_environment"]),
            f"{gate['id']} declares command and environment")
    require(bool(gate["production_components"]) and bool(gate["negative_oracle"]),
            f"{gate['id']} declares exact components and negative oracle")
    require(set(gate["risk_ids"]) <= expected_risks,
            f"{gate['id']} references only scoped risks")
    for component in gate["production_components"]:
        require((repo / component).is_file(),
                f"{gate['id']} production component exists: {component}")
    if gate["status"] in {"PASS", "OLD_BUG_CONFIRMED"}:
        require(isinstance(gate["evidence"], str) and
                (repo / gate["evidence"]).is_file(),
                f"{gate['id']} has retained evidence")

blocking = [entry for entry in risks if entry["status"] != "PASS"]
require(bool(blocking), "current manifest truthfully retains release blockers")
require(manifest.get("release_state") == "BLOCKED",
        "release state cannot be green while any scoped risk is unresolved")
require({"HJ-GATE-VIGEM-O1-LEGACY", "HJ-GATE-VIGEM-O1-TARGET",
         "HJ-GATE-VIGEM-O2-LEGACY", "HJ-GATE-VIGEM-O2-TARGET",
         "HJ-GATE-VIGEM-IPC-PROCESS",
         "HJ-GATE-PROCESS-SUPERVISOR-FAKE-CHILD",
         "HJ-GATE-VIGEM-SELF-HOST-FAKE-EXACT-EXE",
         "HJ-GATE-VIGEM-REAL-CHILD-EXACT-EXE"} <= set(gate_ids),
        "ViGEm old-bug, target, IPC, supervisor, fake and real-child gates are distinct")

print("RISK_TEST_MANIFEST_STATIC_AUDIT=PASS release_state=BLOCKED "
      f"risks={len(risks)} gates={len(gates)}")
