; Only called after explicit uninstall consent. Roots are fixed by the recipe.
; No recursive RMDir: inspect every entry and unlink reparse points themselves.
Var CleanupErrors
Var UserDataRoot
Var TempDataRoot
Var CrashDumpRoot

Function un.DeleteDataTree
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  System::Call 'kernel32::GetFileAttributesW(w r0) i .r1 ?e'
  Pop $4
  ${If} $1 == -1
    StrCpy $1 $4
    ${If} $1 != 2
    ${AndIf} $1 != 3
      StrCpy $CleanupErrors 1
      DetailPrint "Cannot inspect: $0"
    ${EndIf}
    Goto data_tree_done
  ${EndIf}
  IntOp $2 $1 & 0x10
  IntOp $4 $1 & 0x400
  ${If} $2 != 0
    ${If} $4 == 0
      ClearErrors
      FindFirst $2 $3 "$0\*"
      ${If} ${Errors}
        StrCpy $CleanupErrors 1
        DetailPrint "Cannot enumerate: $0"
        Goto data_tree_done
      ${EndIf}
      data_tree_next:
        StrCmp $3 "" data_tree_close
        StrCmp $3 "." data_tree_skip
        StrCmp $3 ".." data_tree_skip
        Push "$0\$3"
        Call un.DeleteDataTree
        data_tree_skip:
        FindNext $2 $3
        Goto data_tree_next
      data_tree_close:
      FindClose $2
    ${EndIf}
    ; RemoveDirectory unlinks a junction instead of walking its target.
    ClearErrors
    RMDir "$0"
  ${Else}
    ClearErrors
    Delete "$0"
  ${EndIf}
  ${If} ${Errors}
    StrCpy $CleanupErrors 1
    DetailPrint "Could not remove: $0"
  ${EndIf}
  data_tree_done:
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd

; Stack: container then pattern. Never enumerate through a reparse container.
Function un.DeleteDataMatches
  Pop $R1
  Pop $R0
  Push $0
  Push $1
  Push $2
  Push $3
  Push $4
  System::Call 'kernel32::GetFileAttributesW(w "$R0") i .r1 ?e'
  Pop $4
  ${If} $1 == -1
    StrCpy $1 $4
    ${If} $1 != 2
    ${AndIf} $1 != 3
      StrCpy $CleanupErrors 1
    ${EndIf}
    Goto data_matches_done
  ${EndIf}
  IntOp $1 $1 & 0x400
  ${If} $1 != 0
    StrCpy $CleanupErrors 1
    DetailPrint "Not following linked container: $R0"
    Goto data_matches_done
  ${EndIf}
  Push $5
  System::Alloc 592
  Pop $5
  ${If} $5 == 0
    StrCpy $CleanupErrors 1
    Goto data_matches_free
  ${EndIf}
  ; Capture GetLastError in the same native call; a later plugin call can reset it.
  System::Call 'kernel32::FindFirstFileW(w "$R0\$R1", p r5) p .r0 ?e'
  Pop $1
  ${If} $0 == -1
    ${If} $1 != 2
    ${AndIf} $1 != 18
      StrCpy $CleanupErrors 1
    ${EndIf}
    Goto data_matches_free
  ${EndIf}
  data_matches_next:
    IntOp $4 $5 + 44
    System::Call '*$4(&w260 .r2)'
    ; Match the producer's entry type, not only its name. Atomic writes and
    ; WER dumps are files; driver extracts are directories.
    System::Call '*$5(i .r4)'
    IntOp $4 $4 & 0x10
    ${If} $R1 == "HallJoy-ViGEm-*"
      ${If} $4 == 0
        Goto data_matches_skip
      ${EndIf}
      StrCpy $3 $2 "" 14
      StrLen $4 $3
      ${If} $4 != 32
        Goto data_matches_skip
      ${EndIf}
      System::Call 'shlwapi::StrSpnW(w r3, w "0123456789ABCDEFabcdef") i .r4'
      ${If} $4 != 32
        Goto data_matches_skip
      ${EndIf}
    ${ElseIf} $4 != 0
      Goto data_matches_skip
    ${EndIf}
    Push "$R0\$2"
    Call un.DeleteDataTree
    data_matches_skip:
    System::Call 'kernel32::FindNextFileW(p r0, p r5) i .r1 ?e'
    Pop $4
    ${If} $1 != 0
      Goto data_matches_next
    ${EndIf}
    ${If} $4 != 18
      StrCpy $CleanupErrors 1
    ${EndIf}
  System::Call 'kernel32::FindClose(p r0)'
  data_matches_free:
  System::Free $5
  Pop $5
  data_matches_done:
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd

Function un.DeleteInstalledData
  ; Stack: absolute path followed by the expected directory attribute.
  Pop $1
  Pop $0
  System::Call 'kernel32::GetFileAttributesW(w r0) i .r2 ?e'
  Pop $3
  ${If} $2 == -1
    ${If} $3 != 2
    ${AndIf} $3 != 3
      StrCpy $CleanupErrors 1
      DetailPrint "Cannot inspect: $0"
    ${EndIf}
    Return
  ${EndIf}
  IntOp $2 $2 & 0x10
  ${If} $2 == $1
    Push $0
    Call un.DeleteDataTree
  ${EndIf}
FunctionEnd

!macro RemoveInstalledFile LEAF
  Push "$INSTDIR\${LEAF}"
  Push 0
  Call un.DeleteInstalledData
!macroend
!macro RemoveInstalledDirectory LEAF
  Push "$INSTDIR\${LEAF}"
  Push 16
  Call un.DeleteInstalledData
!macroend
!macro RemoveInstalledMatches PATTERN
  Push "$INSTDIR"
  Push "${PATTERN}"
  Call un.DeleteDataMatches
!macroend

!macro RequireSeparateDataRoot ERRORCODE
  ; Win32 canonicalization also accepts paths not created yet (NSIS' own
  ; GetFullPathName can return empty for a missing directory).
  System::Call 'kernel32::GetFullPathNameW(w "$UserDataRoot", i ${NSIS_MAX_STRLEN}, w .r0, p 0) i .r4'
  ${If} $4 == 0
  ${OrIf} $4 >= ${NSIS_MAX_STRLEN}
    SetErrorLevel ${ERRORCODE}
    Abort
  ${EndIf}
  System::Call 'kernel32::GetFullPathNameW(w "$INSTDIR", i ${NSIS_MAX_STRLEN}, w .r1, p 0) i .r4'
  ${If} $4 == 0
  ${OrIf} $4 >= ${NSIS_MAX_STRLEN}
    SetErrorLevel ${ERRORCODE}
    Abort
  ${EndIf}
  StrCpy $0 "$0\"
  StrCpy $1 "$1\"
  StrLen $2 $0
  StrCpy $3 $1 $2
  ${If} $0 == $3
    MessageBox MB_ICONSTOP "Choose a program folder separate from the HallJoy data folder." /SD IDOK
    SetErrorLevel ${ERRORCODE}
    Abort
  ${EndIf}
  StrLen $2 $1
  StrCpy $3 $0 $2
  ${If} $1 == $3
    MessageBox MB_ICONSTOP "Choose a program folder separate from the HallJoy data folder." /SD IDOK
    SetErrorLevel ${ERRORCODE}
    Abort
  ${EndIf}
!macroend

Function un.RemoveUserData
  !insertmacro RequireSeparateDataRoot 6
  StrCpy $CleanupErrors 0
  ; Dedicated application root includes settings, backups, logs and runtime cache.
  Push "$UserDataRoot"
  Call un.DeleteDataTree
  ; A legacy/portable data root may coexist with the installed program files.
  ; Remove only known application names here; preserve arbitrary user files.
  !insertmacro RemoveInstalledFile "settings.ini"
  !insertmacro RemoveInstalledFile "settings.ini.bak"
  !insertmacro RemoveInstalledFile "settings.ini.pre-bundle.bak"
  !insertmacro RemoveInstalledFile "bindings.ini"
  !insertmacro RemoveInstalledFile "GameProfiles.ini"
  !insertmacro RemoveInstalledFile "HallJoy.portable"
  !insertmacro RemoveInstalledDirectory "GlobalProfiles"
  !insertmacro RemoveInstalledDirectory "Layouts"
  !insertmacro RemoveInstalledDirectory "CurvePresets"
  !insertmacro RemoveInstalledDirectory "MigrationBackups"
  !insertmacro RemoveInstalledDirectory "FactoryResetBackups"
  !insertmacro RemoveInstalledDirectory ".internal"
  !insertmacro RemoveInstalledFile ".factory-reset-request.ini"
  !insertmacro RemoveInstalledFile "HallJoy.log"
  !insertmacro RemoveInstalledFile "HallJoy.log.tmp"
  !insertmacro RemoveInstalledFile "HallJoyCrash.txt"
  !insertmacro RemoveInstalledFile "HallJoyStabilityTrace.log"
  !insertmacro RemoveInstalledFile "HallJoyStabilityTrace.previous.log"
  !insertmacro RemoveInstalledFile "HallJoyAnalogHost.log"
  !insertmacro RemoveInstalledFile "HallJoyAnalogHostCrashLatest.txt"
  !insertmacro RemoveInstalledFile "HallJoyAnalogHostCrashLatest.dmp"
  !insertmacro RemoveInstalledFile "HallJoyDiagnosticCrash.txt"
  !insertmacro RemoveInstalledFile "HallJoyDiagnosticVectored.txt"
  !insertmacro RemoveInstalledFile "HallJoyDiagnosticExit.txt"
  !insertmacro RemoveInstalledFile "HallJoyMAD68ProR.log"
  !insertmacro RemoveInstalledFile "HallJoyAddressedAnalogTrace.log"
  !insertmacro RemoveInstalledFile "HallJoyProviderV2Qualification.txt"
  !insertmacro RemoveInstalledFile "HallJoyProviderV2Qualification.tmp"
  !insertmacro RemoveInstalledMatches "HallJoyAnalogHostCrash_*.txt"
  !insertmacro RemoveInstalledMatches "HallJoyAnalogHostCrash_*.dmp"
  !insertmacro RemoveInstalledMatches "settings.ini.halljoy-new-*"
  !insertmacro RemoveInstalledMatches "bindings.ini.halljoy-new-*"
  !insertmacro RemoveInstalledMatches "settings.ini.bak.halljoy-new-*"
  !insertmacro RemoveInstalledMatches "settings.ini.pre-bundle.bak.halljoy-new-*"
  !insertmacro RemoveInstalledMatches "GameProfiles.ini.halljoy-new-*"
  !insertmacro RemoveInstalledMatches ".factory-reset-request.ini.halljoy-new-*"
  !insertmacro RemoveInstalledMatches ".migration-from-exe-*.ini.halljoy-new-*"
  !insertmacro RemoveInstalledMatches ".halljoy-write-probe-*.tmp"
  !insertmacro RemoveInstalledMatches ".migration-from-exe-*.ini"
  Push "$TempDataRoot"
  Push "HallJoy-ViGEm-*"
  Call un.DeleteDataMatches
  Push "$CrashDumpRoot"
  Push "HallJoy.exe.*.dmp"
  Call un.DeleteDataMatches
  ${If} $CleanupErrors != 0
    MessageBox MB_ICONSTOP "Some HallJoy data could not be removed. Close other HallJoy copies and check file permissions, then retry. The app and uninstaller have been kept." /SD IDOK
    SetErrorLevel 6
    Abort
  ${EndIf}
FunctionEnd
