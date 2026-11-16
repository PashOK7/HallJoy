"""Windows compiler-tree budget; keep the publisher/restored application outside."""
from __future__ import annotations
import os
import atexit
import ctypes
from pathlib import Path
import subprocess
import sys


_applied_pid = None
_retained_handle = None

def apply_current() -> None:
    global _applied_pid, _retained_handle
    if os.name != "nt" or _applied_pid == os.getpid():
        return
    helper = Path(__file__).with_suffix(".ps1")
    result = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy",
                    "Bypass", "-File", str(helper), "-AttachProcessId", str(os.getpid())],
                   capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError("Cannot apply build CPU limit: " + result.stdout + result.stderr)
    _retained_handle = int(result.stdout.strip().split("handle=")[-1])
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    atexit.register(kernel.CloseHandle, _retained_handle)
    _applied_pid = os.getpid()
    print(result.stdout.strip().split(" handle=")[0], flush=True)


def main() -> int:
    command = sys.argv[1:]
    if not command or command.pop(0) != "--run" or not command:
        raise SystemExit("usage: build_cpu_budget.py --run executable [arguments...]")
    # Join BEFORE starting the compiler: every child inherits the same job.
    # Failure is explicit; never silently launch an uncapped compiler.
    apply_current()
    env = dict(os.environ, MSBUILDDISABLENODEREUSE="1")
    return subprocess.run(command, env=env).returncode


if __name__ == "__main__":
    raise SystemExit(main())
