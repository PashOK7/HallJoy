"""Shared fail-closed AddressSanitizer health check for portable test runners."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path


def require_address_sanitizer_health(
    *,
    root: Path,
    tests: Path,
    output: Path,
    clang: str,
    environment: dict[str, str],
    compile_prefix: list[str],
) -> None:
    """Require the active ASan runtime to reject the intentional test-only OOB."""
    executable = output / ("sanitizer-health.exe" if os.name == "nt" else "sanitizer-health")
    command = [
        clang,
        *compile_prefix,
        str(tests / "sanitizer_health_control_test.cpp"),
        "-o",
        str(executable),
    ]
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=root, check=True)
    health = subprocess.run(
        [str(executable)],
        cwd=output,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if health.returncode == 0 or "AddressSanitizer" not in health.stdout:
        raise SystemExit(
            "sanitizer health control was not detected; "
            f"exit={health.returncode}\n{health.stdout}"
        )
    print("SANITIZER_HEALTH_CONTROL=PASS expected_asan_failure_detected")
