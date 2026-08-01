#!/usr/bin/env python3
"""Static gate for V14-09A transactional settings/profile persistence."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"


def read(name: str) -> str:
    return (HALL / name).read_text(encoding="utf-8")


def require(text: str, marker: str, description: str) -> None:
    if marker not in text:
        raise SystemExit(f"FAIL: {description}: missing {marker!r}")
    print(f"PASS: {description}")


transaction = read("transactional_file_store.h")
ini = read("ini_util.cpp")
settings = read("settings_ini.cpp")
profile = read("profile_ini.cpp")
global_profiles = read("global_profiles.cpp")
subpages = read("keyboard_subpages.cpp")
project = read("HallJoy.vcxproj")

ordered = [
    "adapter.Prepare()",
    "adapter.Write()",
    "adapter.Flush()",
    "adapter.Validate()",
    "adapter.Replace()",
]
positions = [transaction.find(marker) for marker in ordered]
if any(position < 0 for position in positions) or positions != sorted(positions):
    raise SystemExit("FAIL: transaction stages are missing or out of order")
print("PASS: prepare/write/flush/validate/replace order is explicit")

require(transaction, "adapter.Cleanup();", "every failed stage cleans its temporary file")
require(ini, "CREATE_NEW", "temporary file creation rejects collisions")
require(ini, 'temporary_ = destination_ + suffix;', "temporary file stays beside destination")
require(ini, 'L".halljoy-new-', "temporary file name is process/thread/sequence unique")
require(ini, "FlushFileBuffers(file)", "durability uses checked physical flush")
require(ini, "MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH", "commit is an atomic write-through replace")
require(ini, "DeleteFileW(temporary_.c_str())", "failed transactions remove temporary files")
require(ini, 'L"save.failure"', "failures publish critical stability evidence")
require(ini, "MessageBoxW", "production UI reports save failures")

for stage in ("prepare", "write", "flush", "validate", "replace"):
    require(ini, f"--halljoy-test-persistence-failure-{stage}", f"simulator injects {stage} failure")

if '+ L".tmp"' in settings or '+ L".tmp"' in profile:
    raise SystemExit("FAIL: settings/profile still use a shared fixed .tmp path")
print("PASS: settings/profile no longer use shared fixed .tmp paths")

require(settings, "IniUtil_SaveAtomic(path, SettingsTransactionWrite, SettingsTransactionValidate", "settings use the common transaction")
require(settings, "IniUtil_CopyExistingForUpdate", "overlay update preserves unrelated base settings")
require(settings, 'L"HallJoyPersistence", L"SchemaVersion"', "settings write a schema marker")
require(settings, "SettingsTransactionValidate", "settings are parsed back before commit")
require(settings, "ok &= KeyboardLayout_SaveToIni", "embedded layout selection writes are checked")
require(profile, "f.flush();", "bindings stream is explicitly flushed")
require(profile, "return !f.fail();", "bindings stream close state is checked")
require(profile, "ProfileTransactionValidate", "bindings are parsed back before commit")
require(profile, "IniUtil_ReportSaveFailure", "bindings failure reaches the user-facing reporter")
require(global_profiles, "ActiveProfileTransactionValidate", "active-profile marker update is read back")
require(global_profiles, "IniUtil_SaveAtomic", "active-profile marker update is atomic")
require(subpages, "if (!previousSettingsSaved || !previousBindingsSaved)", "profile switching aborts after a failed old-profile save")
require(subpages, "if (!newSettingsSaved || !newBindingsSaved)", "profile creation rejects a partial new profile")
require(subpages, "if (!settingsSaved || !bindingsSaved)", "manual profile save keeps dirty state after failure")
require(project, '<ClInclude Include="transactional_file_store.h" />', "transaction header is part of the MSVC project")

print("PERSISTENCE_TRANSACTION_STATIC_AUDIT=PASS")
