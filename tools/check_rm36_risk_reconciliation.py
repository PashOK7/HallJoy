#!/usr/bin/env python3
"""Ensure every historical P0/P1 risk has one explicit RM-36 disposition.

The check is documentary/source-only and intentionally does not reclassify
physical or release-publishing evidence as locally passed.
"""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "docs" / "v1.4" / "RISK_TEST_MANIFEST_V1.json"
RECONCILIATION = ROOT / "docs" / "v1.4" / "RM36_RISK_RECONCILIATION_2026-09-06.md"
ALLOWED = {"SOURCE_LOCAL", "HARDWARE_PENDING", "RELEASE_PENDING"}


def main() -> int:
    risks = json.loads(MANIFEST.read_text(encoding="utf-8"))["risks"]
    required = {risk["id"] for risk in risks if "-P0-" in risk["id"] or "-P1-" in risk["id"]}
    text = RECONCILIATION.read_text(encoding="utf-8")
    rows = re.findall(r"^\| (HJ-V14-P[01]-\d+) \| ([A-Z_]+) \|", text, flags=re.MULTILINE)
    found = {risk_id for risk_id, _ in rows}
    duplicate = sorted(risk_id for risk_id in found if sum(row[0] == risk_id for row in rows) != 1)
    unknown_status = sorted(f"{risk_id}={status}" for risk_id, status in rows if status not in ALLOWED)
    failures: list[str] = []
    if required - found:
        failures.append("unassigned risks: " + ", ".join(sorted(required - found)))
    if found - required:
        failures.append("unknown reconciliation risks: " + ", ".join(sorted(found - required)))
    if duplicate:
        failures.append("duplicate reconciliation rows: " + ", ".join(duplicate))
    if unknown_status:
        failures.append("unknown dispositions: " + ", ".join(unknown_status))
    if "There is no unassigned P0/P1." not in text:
        failures.append("explicit no-unassigned conclusion is missing")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("RM36_RISK_RECONCILIATION=PASS")
    print(f"risks={len(required)} source_local={sum(status == 'SOURCE_LOCAL' for _, status in rows)} "
          f"hardware_pending={sum(status == 'HARDWARE_PENDING' for _, status in rows)} "
          f"release_pending={sum(status == 'RELEASE_PENDING' for _, status in rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
