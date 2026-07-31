#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
TOOLS = ROOT / "tools"

header = (HALL / "stability_trace.h").read_text(encoding="utf-8")
source = (HALL / "stability_trace.cpp").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
build = (TOOLS / "build.ps1").read_text(encoding="utf-8")
spark = (HALL / "backend_sparklink.inc").read_text(encoding="utf-8")
collector = (TOOLS / "collect_stability_trace.ps1").read_text(encoding="utf-8")

required_source = [
    "kMaxTraceBytes = 1024u * 1024u",
    "HallJoyStabilityTrace.previous.log",
    "CreateFileMappingW",
    "MapViewOfFile",
    "FlushViewOfFile",
    "event=trace.capped",
    "StabilityTrace_WriteCritical",
    "HALLJOY_STABILITY_TRACE",
]
for token in required_source:
    assert token in source, token
assert "noexcept" in header
write_formatted = source[source.index("void WriteFormatted"):source.index("#endif", source.index("void WriteFormatted"))]
assert "WriteFile" not in write_formatted
assert "FlushViewOfFile" not in write_formatted
assert 'ClCompile Include="stability_trace.cpp"' in project
assert 'ClInclude Include="stability_trace.h"' in project
assert "HallJoyStabilityTrace" in project
assert "/p:HallJoyStabilityTrace=true" in build
assert "COLLECT_STABILITY_TRACE.cmd" in build
assert "HallJoyStabilityTraceBundle.zip" in collector
assert 'L"spark", L"worker.stats"' in spark
assert "changed_rows=%u input_notifies=%u" in spark
assert "poll_mode=%u row_limit=%u" in spark

spec = importlib.util.spec_from_file_location("trace_analyzer", TOOLS / "analyze_stability_trace.py")
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
assert spec.loader is not None
spec.loader.exec_module(module)

valid_lines = [
    "[2026-07-30T21:00:00.000][elapsed_ms=0][seq=1][pid=1][tid=1][level=INFO][component=main][event=session.start] schema=1 stage=S02V1",
    "[2026-07-30T21:00:00.001][elapsed_ms=1][seq=2][pid=1][tid=1][level=INFO][component=vigem][event=init.ok] pads=1",
    "[2026-07-30T21:00:00.002][elapsed_ms=2][seq=3][pid=1][tid=2][level=INFO][component=spark][event=worker.start] generation=1",
    "[2026-07-30T21:00:00.003][elapsed_ms=3][seq=4][pid=1][tid=2][level=INFO][component=spark][event=connected] vid=0x1234 poll_mode=0 row_limit=0",
    "[2026-07-30T21:00:00.004][elapsed_ms=4][seq=5][pid=1][tid=1][level=INFO][component=spark.settings][event=poll_mode] old=0 new=1",
    "[2026-07-30T21:00:00.005][elapsed_ms=5][seq=6][pid=1][tid=1][level=INFO][component=spark.settings][event=poll_mode] old=1 new=2",
    "[2026-07-30T21:00:00.006][elapsed_ms=6][seq=7][pid=1][tid=1][level=INFO][component=spark.settings][event=row_limit] old=0 new=4",
    "[2026-07-30T21:00:00.007][elapsed_ms=7][seq=8][pid=1][tid=2][level=WARN][component=spark][event=hotplug.stale] elapsed_ms=1801",
    "[2026-07-30T21:00:00.008][elapsed_ms=8][seq=9][pid=1][tid=2][level=INFO][component=spark][event=hotplug.reconnect] ok=1",
    "[2026-07-30T21:00:00.009][elapsed_ms=9][seq=10][pid=1][tid=2][level=INFO][component=spark][event=worker.stats] route_queries=100 route_ok=99 route_fail=1 changed_rows=5 input_notifies=5 released_any=1",
    "[2026-07-30T21:00:00.010][elapsed_ms=10][seq=11][pid=1][tid=2][level=INFO][component=spark][event=neutralized] reason=stop",
    "[2026-07-30T21:00:00.011][elapsed_ms=11][seq=12][pid=1][tid=2][level=INFO][component=spark][event=worker.exit] fault=0",
    "[2026-07-30T21:00:00.012][elapsed_ms=12][seq=13][pid=1][tid=1][level=INFO][component=main][event=session.end] exit_code=0",
]
valid = "\n".join(valid_lines) + "\n"
invalid = valid.replace("[seq=12]", "[seq=14]").replace(
    "[level=INFO][component=spark][event=worker.exit]",
    "[level=ERROR][component=spark][event=worker.fault]",
)
incomplete = "\n".join([valid_lines[0], valid_lines[-1].replace("[seq=13]", "[seq=2]")]) + "\n"
transport_disconnect = valid.replace(
    "[level=WARN][component=spark][event=hotplug.stale] elapsed_ms=1801",
    "[level=INFO][component=spark][event=disconnected] reason=transport",
)
with tempfile.TemporaryDirectory() as temp:
    good_path = Path(temp) / "good.log"
    bad_path = Path(temp) / "bad.log"
    incomplete_path = Path(temp) / "incomplete.log"
    transport_disconnect_path = Path(temp) / "transport-disconnect.log"
    good_path.write_text(valid, encoding="utf-8")
    bad_path.write_text(invalid, encoding="utf-8")
    incomplete_path.write_text(incomplete, encoding="utf-8")
    transport_disconnect_path.write_text(transport_disconnect, encoding="utf-8")
    verdict, failures, warnings, _ = module.analyze(module.parse_trace(good_path))
    assert verdict == "PASS", (verdict, failures, warnings)
    verdict, failures, _, _ = module.analyze(module.parse_trace(bad_path))
    assert verdict == "FAIL" and failures
    verdict, failures, warnings, _ = module.analyze(module.parse_trace(incomplete_path))
    assert verdict == "WARN" and not failures and warnings
    verdict, failures, warnings, _ = module.analyze(
        module.parse_trace(transport_disconnect_path))
    assert verdict == "PASS", (verdict, failures, warnings)

print("stability trace static audit: PASS")
