#!/usr/bin/env python3
"""Run deterministic native protocol parser fuzz smoke under ASan + UBSan."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    hall = root / "src" / "HallJoyProject" / "HallJoy"
    tests = root / "src" / "HallJoyProject" / "tests"
    clang = shutil.which("clang++")
    if not clang:
        fixed = Path(r"C:\Program Files\LLVM\bin\clang++.exe")
        if fixed.is_file():
            clang = str(fixed)
    if not clang:
        raise SystemExit("clang++ is required for the protocol fuzz sanitizer gate")

    resource_dir = subprocess.check_output(
        [clang, "--print-resource-dir"], text=True).strip()
    asan_dll = Path(resource_dir) / "lib" / "windows" / "clang_rt.asan_dynamic-x86_64.dll"
    if os.name == "nt" and not asan_dll.is_file():
        raise SystemExit(f"Clang ASan runtime is missing: {asan_dll}")

    environment = os.environ.copy()
    environment["ASAN_OPTIONS"] = (
        f"detect_leaks={'0' if os.name == 'nt' else '1'}:halt_on_error=1:strict_string_checks=1")
    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    with tempfile.TemporaryDirectory(prefix="halljoy-protocol-fuzz-") as temporary:
        output = Path(temporary)
        if os.name == "nt":
            shutil.copy2(asan_dll, output / asan_dll.name)
        executable = output / ("protocol-fuzz.exe" if os.name == "nt" else "protocol-fuzz")
        command = [
            clang,
            "-std=c++20",
            "-O1",
            "-g",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-fno-omit-frame-pointer",
            "-fsanitize=address,undefined",
            f"-I{hall}",
            str(tests / "protocol_parser_fuzz_smoke_test.cpp"),
            str(hall / "aula_win60he_protocol.cpp"),
            str(hall / "hex80_protocol.cpp"),
            str(hall / "mad68pr_protocol.cpp"),
            "-o",
            str(executable),
        ]
        print("+", " ".join(command), flush=True)
        subprocess.run(command, cwd=root, check=True)
        subprocess.run([str(executable)], cwd=output, env=environment, check=True)

    print("PROTOCOL_FUZZ_SANITIZERS=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
