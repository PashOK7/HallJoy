#!/usr/bin/env python3
"""Guard the production-linked file-only profile runner against backend entry."""

from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"

backend_h = (HALL / "backend.h").read_text(encoding="utf-8")
backend = (HALL / "backend.cpp").read_text(encoding="utf-8")
app = (HALL / "app.cpp").read_text(encoding="utf-8")
profile_test = (PROJECT / "tests" / "profile_transaction_windows_test.cpp").read_text(encoding="utf-8")
runner = (ROOT / "tools" / "run_profile_transaction_tests.ps1").read_text(encoding="utf-8")
backend_init = backend[backend.index("bool Backend_Init()"):backend.index("bool Backend_Shutdown()")]

checks = {
    "file-only runner passes the guard to both launches":
        runner.count("--halljoy-test-forbid-backend-init") == 2,
    "backend guard precedes output lifecycle":
        backend_init.index("--halljoy-test-forbid-backend-init") < backend_init.index("VigemOutput_Stop()"),
    "backend guard counts the attempted entry":
        "g_fileOnlyTestForbiddenInitAttempts.fetch_add" in backend,
    "simulator-only oracle is declared":
        "Backend_FileOnlyTestForbiddenInitAttempts" in backend_h,
    "transaction mode requires zero forbidden entries":
        "passed && forbiddenBackendInits == 0 ? 0 : 1" in app,
    "startup-only mode requires zero forbidden entries":
        "if (forbiddenBackendInits != 0)" in app,
    "production-linked transaction writes the zero count":
        "backend_init_attempts=0" in profile_test and
        "Backend_FileOnlyTestForbiddenInitAttempts() == 0" in profile_test,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("PROFILE FILE-ONLY STATIC AUDIT FAILED\n" +
                     "\n".join(f" - {name}" for name in failed))
print("PROFILE_FILE_ONLY_STATIC_AUDIT=PASS")
