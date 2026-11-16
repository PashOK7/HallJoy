"""Fail-closed receipt validation and mandatory normal-build integration."""
from pathlib import Path
import copy
import importlib.util
import json
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location("tester_gate",root/"tools/check_tester_delivery.py")
gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
document=json.loads(gate.CONTRACT.read_text(encoding="utf-8"))
cases=gate.validate_contract(document)
for case in cases:
    fields=" ".join(key+"="+str(value) for key,value in case["required_receipt"].items())
    good=case["marker"]+"=PASS "+fields+"\n"
    gate.validate_receipt(good,case)
    for invalid in ("",good.replace("=PASS","=FAIL"),good+good,good.replace("first_sample=1","first_sample=0"),
                    good.replace("cleanup=1","cleanup=0"),good.replace("hardware_opened=0","hardware_opened=1")):
        try:gate.validate_receipt(invalid,case)
        except ValueError:pass
        else:raise AssertionError("Invalid receipt accepted")
for mutate in (lambda c:c.update(positive_cases=["factory"]),lambda c:c["required_receipt"].update(multikey=0),
               lambda c:c["hard_refusals"][0].update(basis="guess"),lambda c:c["files"].append("../outside.cpp")):
    bad=copy.deepcopy(document);mutate(bad["cases"][0])
    try:gate.validate_contract(bad)
    except ValueError:pass
    else:raise AssertionError("Invalid tester contract accepted")
wrapper=(root/"tools/build_release.ps1").read_text(encoding="utf-8")
assert wrapper.index("check_tester_delivery.py") < wrapper.index("publish_halljoy_build.ps1")
assert "--exe $candidate" in wrapper
full=(root/"tools/build.ps1").read_text(encoding="utf-8")
assert full.index("check_tester_delivery.py") < full.index("publish_halljoy_build.ps1")
assert "--exe $exe" in full
assert "TESTER_BUILD_CONTRACT" in (root/"AGENTS.md").read_text(encoding="utf-8")
print("TESTER_DELIVERY_AUDIT=PASS bad_receipts_rejected metadata_not_hard factory_only_rejected normal_build_enforced")
