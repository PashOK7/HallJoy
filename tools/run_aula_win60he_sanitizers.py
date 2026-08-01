#!/usr/bin/env python3
"""Compile and run the pure Aula WIN60HE stack with Clang ASan + UBSan."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def run(command: list[str], *, cwd: Path, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, env=env, check=True)


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
        raise SystemExit("clang++ is required for the Aula sanitizer gate")

    resource_dir = subprocess.check_output(
        [clang, "--print-resource-dir"], text=True).strip()
    asan_dll = Path(resource_dir) / "lib" / "windows" / "clang_rt.asan_dynamic-x86_64.dll"
    if os.name == "nt" and not asan_dll.is_file():
        raise SystemExit(f"Clang ASan runtime is missing: {asan_dll}")

    suites: list[tuple[str, list[Path]]] = [
        ("protocol", [tests / "aula_win60he_protocol_test.cpp", hall / "aula_win60he_protocol.cpp"]),
        ("oracle", [tests / "aula_win60he_oracle_test.cpp", hall / "aula_win60he_protocol.cpp"]),
        ("end_to_end", [
            tests / "aula_win60he_end_to_end_test.cpp",
            hall / "aula_win60he_protocol.cpp",
            hall / "aula_win60he_client.cpp",
        ]),
        ("session_policy", [
            tests / "aula_win60he_session_policy_test.cpp",
            hall / "aula_win60he_session_policy.cpp",
        ]),
    ]
    environment = os.environ.copy()
    leak_detection = "0" if os.name == "nt" else "1"
    environment["ASAN_OPTIONS"] = (
        f"detect_leaks={leak_detection}:halt_on_error=1:strict_string_checks=1")
    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    with tempfile.TemporaryDirectory(prefix="halljoy-aula-sanitizers-") as temporary:
        output = Path(temporary)
        if os.name == "nt":
            shutil.copy2(asan_dll, output / asan_dll.name)
        for name, sources in suites:
            executable = output / (name + (".exe" if os.name == "nt" else ""))
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
                *map(str, sources),
                "-o",
                str(executable),
            ]
            run(command, cwd=root)
            run([str(executable)], cwd=output, env=environment)

    print("AULA_WIN60HE_SANITIZERS=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
