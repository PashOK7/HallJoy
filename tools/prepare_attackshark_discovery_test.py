"""Materialize the actual metadata discovery loop with mocked Windows APIs.

Stop at the session boundary: this regression must never open real HID devices.
The source markers are deliberately strict so refactoring cannot silently test
an obsolete copy of production logic.
"""
from pathlib import Path


def prepare(out: Path, hall: Path, test: Path) -> Path:
    source = (hall / "attackshark_pro_diagnostic.cpp").read_text(encoding="utf-8")
    begin = source.index("struct Device {")
    end = source.index("struct Session {", begin)
    capture = source.index("int Capture(){")
    boundary = source.index("  if(topology!=identityTopology)", capture)
    code = source[begin:end] + source[capture:boundary] + "return 0;\n}\n}\n"
    include = out / "shark_discovery.inc"
    target = out / "attackshark_discovery_windows_test.cpp"
    if include.exists() or target.exists():
        raise RuntimeError("Discovery test output already exists")
    include.write_text(code, encoding="utf-8")
    target.write_bytes(test.read_bytes())
    return target
