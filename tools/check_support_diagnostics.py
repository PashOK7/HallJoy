#!/usr/bin/env python3
"""Mandatory local Release gate for the production diagnostic collector/writer."""
import os
import sys
import tempfile
from pathlib import Path
from run_native_backend_checks import find_cxx, compile_and_run_many, run

def main():
    root=Path(__file__).resolve().parents[1]
    hall=root/"src/HallJoyProject/HallJoy";tests=hall.parent/"tests"
    run([sys.executable,str(root/"tools/check_tester_delivery.py")])
    run([sys.executable,str(tests/"tester_delivery_audit.py")])
    run([sys.executable,str(tests/"support_diagnostics_audit.py")])
    run([sys.executable,str(tests/"support_log_static_audit.py")])
    run([sys.executable,str(tests/"native_backend_architecture_static_audit.py")])
    if os.name!="nt":raise SystemExit("The real support writer Release gate requires Windows.")
    compiler=find_cxx()
    if not compiler:raise SystemExit("Install a C++20 g++/clang++ or set CXX for the mandatory diagnostic gate.")
    base=root/"build/obj/portable-tests";base.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="support-contract-",dir=base) as folder:
        compile_and_run_many(compiler,Path(folder),[
            ("native_analog_telemetry_collect",[tests/"native_analog_telemetry_collect_test.cpp"]),
            ("support_log_windows",[tests/"support_log_windows_test.cpp",hall/"support_log.cpp"]),
            ("input_path_log_windows",[tests/"support_log_windows_test.cpp",hall/"support_log.cpp"]),
            ("mchose_mix87_session",[tests/"mchose_mix87_session_test.cpp"]),
            ("mchose_jet75_session",[tests/"mchose_jet75_session_test.cpp"]),
            ("mad68_dual_trial_session",[tests/"mad68_dual_trial_session_test.cpp"]),
            ("alumix104_session",[tests/"alumix104_session_test.cpp",hall/"addressed_poll_scheduler.cpp"]),
        ],hall)
    print("SUPPORT_DIAGNOSTICS_RELEASE_GATE=PASS")
if __name__=="__main__":main()
