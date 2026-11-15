"""Owner contract: setup is optional; ordinary delivery stays a single EXE."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
batch=(ROOT/'BUILD.cmd').read_text(encoding='utf-8-sig')
release=(ROOT/'tools/build_release.ps1').read_text()
optional=(ROOT/'tools/build_windows_installer.ps1').read_text()
assert 'tools\\build.ps1' in batch and 'installer' not in batch.lower()
assert '-RequireClang' not in release
assert 'build/bin/Release/x64/HallJoy.exe' in optional
assert 'package_windows_installer.ps1' in optional
assert 'publish_windows_outputs.ps1' not in optional and 'publish_halljoy_build.ps1' not in optional
assert 'build/HallJoy.exe' not in optional and 'build/HallJoy-setup.exe' not in optional
assert not (ROOT/'tools/publish_windows_outputs.ps1').exists()
recipe=(ROOT/'tools/installer/windows.nsi').read_text()
cleanup=(ROOT/'tools/installer/data_cleanup.nsh').read_text()
assert 'UninstPage custom un.DataPage un.DataPageLeave' in recipe
assert 'StrCpy $RemoveUserData ${BST_UNCHECKED}' in recipe
assert '${NSD_SetState} $RemoveDataCheckbox $RemoveUserData' in recipe
assert '${NSD_GetState} $RemoveDataCheckbox $RemoveUserData' in recipe
assert '${If} $RemoveUserData == ${BST_CHECKED}' in recipe
assert '/REMOVEUSERDATA=' in recipe and '${ElseIf} $1 != "0"' in recipe
assert 'RMDir /r' not in recipe+cleanup
assert '& 0x400' in cleanup and 'RequireSeparateDataRoot' in cleanup
assert (ROOT/'tools/test_windows_uninstall_data.ps1').exists()
print('OPTIONAL_INSTALLER_STATIC_AUDIT=PASS portable_default single_ordinary_exe optional_cleanup_unchecked no_recursive_rmdir')
