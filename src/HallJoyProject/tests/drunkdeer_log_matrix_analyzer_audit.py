#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[3]
tool = ROOT / "tools" / "analyze_drunkdeer_matrix_log.py"
backend = (ROOT / "src" / "HallJoyProject" / "HallJoy" /
           "drunkdeer_backend.cpp").read_text(encoding="utf-8")

assert tool.is_file()
subprocess.run([sys.executable, str(tool), "--self-test"], check=True)

# Digital input remains evidence-only. It cannot publish, select, learn or
# rewrite the analogue map used by ProcessMatrix/ViGEm at runtime.
digital = backend[backend.index("void DrunkDeerDiagnostic_RecordRawKeyboardEvent"):]
assert "g_milli" not in digital
assert "ProcessMatrix" not in digital
assert "GenericUapMap" not in digital
assert "BeginAutomaticDigitalMapping" not in digital
assert "EndAutomaticDigitalMapping" not in digital
process = backend[backend.index("void ProcessMatrix("):
                  backend.index("void ObserveHeaders(")]
assert "digital" not in process.lower()
assert "AutomaticMapping" not in process

source = tool.read_text(encoding="utf-8")
assert "runs after HallJoy exits" in source
assert "resulting static model map" in source
assert "interval.start_ms <= frame.time_ms" in source
assert "episode.end_ms >= interval.start_ms" in source
assert "FN_CANDIDATE" in source

print("drunkdeer offline matrix analyzer audit passed")
