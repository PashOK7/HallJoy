#!/usr/bin/env python3
"""Validate tester admission contracts and receipts from the exact production EXE."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "docs/development/tester_delivery_cases.json"

def validate_contract(document, root=ROOT):
    if document.get("schema") != 1 or not document.get("cases"):
        raise ValueError("Missing versioned tester cases")
    seen = set()
    for case in document["cases"]:
        if case.get("id") in seen or not re.fullmatch(r"[a-z0-9-]+", case.get("id", "")):
            raise ValueError("Invalid or duplicate case identity")
        seen.add(case["id"])
        if not case.get("model") or not re.fullmatch(r"--halljoy-[a-z0-9-]+-self-test",case.get("argument", "")):
            raise ValueError("Missing model/isolated self-test")
        positives, negatives = case.get("positive_cases", []), case.get("negative_cases", [])
        required = case.get("required_receipt", {})
        if len(positives) < 2 or len(set(positives)) != len(positives) or not negatives:
            raise ValueError("Factory-only or missing negative evidence")
        if required.get("scenarios") != len(positives) or required.get("negatives") != len(negatives):
            raise ValueError("Scenario counts disagree with executable expectations")
        for stage in ("proof_to_subscribe", "first_sample", "multikey", "release", "cleanup"):
            if required.get(stage) != 1: raise ValueError("Missing end-to-end stage: " + stage)
        if required.get("hardware_opened") != 0: raise ValueError("Self-test must be hardware isolated")
        gates = case.get("hard_refusals", [])
        if {g.get("condition") for g in gates} != set(negatives) or any(len(g.get("basis", "")) < 20 for g in gates):
            raise ValueError("Every hard refusal needs an explained negative case")
        soft = set(case.get("metadata_only", []))
        if len(soft) < 3 or soft.intersection(g.get("condition") for g in gates):
            raise ValueError("Metadata-only differences cannot also be hard refusals")
        if not case.get("files"): raise ValueError("No production/test sources registered")
        for relative in case["files"]:
            path = (root / relative).resolve()
            if not path.is_relative_to(root.resolve()) or not path.is_file():
                raise ValueError("Invalid source path: " + relative)
    return document["cases"]

def validate_receipt(output, case):
    marker = re.escape(case["marker"])
    matches = re.findall(r"^" + marker + r"=(PASS|FAIL)\s+([^\r\n]+)$", output, flags=re.M)
    if len(matches) != 1 or matches[0][0] != "PASS":
        raise ValueError(case["id"] + ": missing unique PASS receipt")
    values = dict(re.findall(r"([a-z_]+)=(\d+)", matches[0][1]))
    for field, expected in case["required_receipt"].items():
        if values.get(field) != str(expected):
            raise ValueError(case["id"] + ": receipt mismatch for " + field)
    return {field: int(values[field]) for field in case["required_receipt"]}

def verify_image(exe, cases):
    before = exe.read_bytes()
    digest = hashlib.sha256(before).hexdigest()
    results = []
    for case in cases:
        flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
        completed = subprocess.run([str(exe), case["argument"]], capture_output=True,
            text=True, errors="replace", timeout=30, creationflags=flags)
        if completed.returncode:
            raise ValueError(case["id"] + ": self-test failed: " + completed.stdout[-2000:] + completed.stderr[-1000:])
        receipt = validate_receipt(completed.stdout, case)
        results.append({"id":case["id"],"model":case["model"],"receipt":receipt})
    if exe.read_bytes() != before: raise ValueError("Candidate changed during verification")
    return {"schema":1,"sha256":digest,"bytes":len(before),"cases":results}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path)
    args = parser.parse_args()
    cases = validate_contract(json.loads(CONTRACT.read_text(encoding="utf-8")))
    if args.exe:
        result = verify_image(args.exe.resolve(),cases)
        folder=ROOT/".local/tester-delivery";folder.mkdir(parents=True,exist_ok=True)
        # Content-addressed receipt is immutable, never overwrites another candidate.
        path=folder/(result["sha256"]+".json")
        encoded=(json.dumps(result,indent=2)+"\n").encode("utf-8")
        if path.exists():
            if path.read_bytes()!=encoded: raise ValueError("Existing receipt disagrees")
        else:
            with path.open("xb") as output:output.write(encoded)
        print("TESTER_DELIVERY_IMAGE=PASS sha256="+result["sha256"]+" cases="+str(len(cases)))
    else:
        print("TESTER_DELIVERY_CONTRACT=PASS registered_families="+str(len(cases))+" image_not_run=1")
if __name__ == "__main__": main()
