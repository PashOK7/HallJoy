#!/usr/bin/env python3
"""Run HallJoy native-backend architecture audits and portable C++ tests.

This script is intentionally independent of Visual Studio. BUILD.cmd runs the same
static audits before the full MSVC build; contributors can use this runner on
Linux/macOS/MinGW while developing parsers and schedulers.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


def run(command: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def find_cxx() -> str | None:
    configured = os.environ.get("CXX")
    if configured:
        return configured
    for candidate in ("g++", "clang++"):
        path = shutil.which(candidate)
        if path:
            return path
    return None


def compile_and_run(cxx: str, output: Path, sources: list[Path], include: Path) -> None:
    command = [
        cxx,
        "-std=c++20",
        "-O2",
        "-Wall",
        "-Wextra",
        "-pedantic",
        f"-I{include}",
        *map(str, sources),
        "-o",
        str(output),
    ]
    run(command)
    run([str(output)])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--static-only", action="store_true", help="skip portable C++ compilation")
    parser.add_argument("--require-compiler", action="store_true", help="fail when g++/clang++ is unavailable")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    project_root = root / "src" / "HallJoyProject"
    hall = project_root / "HallJoy"
    tests = project_root / "tests"

    ET.parse(hall / "HallJoy.vcxproj")
    ET.parse(hall / "HallJoy.vcxproj.filters")

    for script in sorted(tests.glob("*audit.py")):
        run([sys.executable, str(script)])

    if args.static_only:
        print("native backend checks: static audits passed")
        return 0

    cxx = find_cxx()
    if not cxx:
        message = "No g++/clang++ found; portable C++ tests were skipped. BUILD.cmd still performs the full MSVC build."
        if args.require_compiler:
            raise SystemExit(message)
        print(message)
        return 0

    with tempfile.TemporaryDirectory(prefix="halljoy-native-tests-") as temp:
        out = Path(temp)
        fixed_tests: list[tuple[str, list[Path]]] = [
            ("addressed_scheduler", [tests / "addressed_poll_scheduler_test.cpp", hall / "addressed_poll_scheduler.cpp"]),
            ("hid_lifecycle", [tests / "hid_io_operation_lifecycle_test.cpp"]),
            ("vigem_scheduler", [tests / "vigem_output_scheduler_test.cpp"]),
            ("native_contract", [tests / "native_analog_backend_contract_test.cpp"]),
            ("worker_lifecycle", [tests / "worker_lifecycle_test.cpp"]),
            ("worker_primitives", [tests / "worker_primitives_test.cpp"]),
            ("worker_exception_barrier", [tests / "worker_exception_barrier_test.cpp"]),
        ]
        for name, sources in fixed_tests:
            compile_and_run(cxx, out / name, sources, hall)

        # Convention used by built-ins and tools/new_native_backend.py:
        # tests/<name>_protocol_test.cpp links HallJoy/<name>_protocol.cpp.
        for test in sorted(tests.glob("*_protocol_test.cpp")):
            protocol_source = hall / test.name.replace("_test.cpp", ".cpp")
            if not protocol_source.exists():
                raise SystemExit(f"Missing pure protocol source for {test.name}: {protocol_source}")
            compile_and_run(cxx, out / test.stem, [test, protocol_source], hall)

    print("native backend checks: all static and portable C++ tests passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode)
