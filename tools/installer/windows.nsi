; Optional per-user package. Keep the original HallJoy executable unchanged.
Unicode true
ManifestDPIAware true
ManifestSupportedOS all
RequestExecutionLevel user
SetCompressor /SOLID lzma
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "FileFunc.nsh"

!define APPID "HallJoy.WindowsInstaller"
!ifdef TEST_ROOT
  !define REGKEY "Software\HallJoyInstallerTests\${TEST_ID}"
  !define MUTEX "Local\HallJoyInstallerTest-${TEST_ID}"
  InstallDir "${TEST_ROOT}\Install"
!else
  !define REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\HallJoy"
  !define MUTEX "Local\HallJoyInstaller"
  InstallDir "$LOCALAPPDATA\Programs\HallJoy"
!endif
InstallDirRegKey HKCU "${REGKEY}" "InstallLocation"
Name "HallJoy"
OutFile "${OUTPUT}"
BrandingText "HallJoy by PashOK7"
Icon "${PAYLOAD}\HallJoy.ico"
UninstallIcon "${PAYLOAD}\HallJoy.ico"
VIProductVersion "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductName" "HallJoy"
VIAddVersionKey /LANG=1033 "FileDescription" "HallJoy Windows installer"
VIAddVersionKey /LANG=1033 "LegalCopyright" "HallJoy by PashOK7"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
ShowInstDetails show
ShowUninstDetails show
Var MenuFolder
Var DesktopFolder
!define MUI_ABORTWARNING
!define MUI_ICON "${PAYLOAD}\HallJoy.ico"
!define MUI_UNICON "${PAYLOAD}\HallJoy.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

!macro InitFolders
  SetShellVarContext current
  SetRegView 64
  !ifdef TEST_ROOT
    StrCpy $MenuFolder "${TEST_ROOT}\Start Menu\HallJoy"
    StrCpy $DesktopFolder "${TEST_ROOT}\Desktop"
  !else
    StrCpy $MenuFolder "$SMPROGRAMS\HallJoy"
    StrCpy $DesktopFolder "$DESKTOP"
  !endif
  System::Call 'kernel32::CreateMutexW(p 0, i 0, w "${MUTEX}") p .r0 ?e'
  Pop $1
  ${If} $1 = 183
    SetErrorLevel 1
    Abort
  ${EndIf}
!macroend

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "HallJoy requires Windows x64." /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP "Windows 10 or newer is required." /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
  !insertmacro InitFolders
FunctionEnd

Function ValidateDestination
  ReadRegStr $0 HKCU "${REGKEY}" "InstallLocation"
  ${If} $0 != ""
  ${AndIf} $0 != $INSTDIR
    MessageBox MB_ICONSTOP "Upgrade the existing installation in its current folder, or uninstall it first." /SD IDOK
    SetErrorLevel 5
    Abort
  ${EndIf}
  ReadINIStr $0 "$INSTDIR\installation.ini" "HallJoy" "AppId"
  ${If} $0 != "${APPID}"
    FindFirst $0 $1 "$INSTDIR\*"
    loop:
      StrCmp $1 "" empty
      StrCmp $1 "." next
      StrCmp $1 ".." next
      FindClose $0
      MessageBox MB_ICONSTOP "Choose an empty folder or an existing HallJoy installation. No files were changed." /SD IDOK
      SetErrorLevel 2
      Abort
      next:
      FindNext $0 $1
      Goto loop
    empty:
    FindClose $0
  ${EndIf}
FunctionEnd

!macro CheckExecutableUnlocked PREFIX
  IfFileExists "$INSTDIR\HallJoy.exe" 0 ${PREFIX}unlocked
  System::Call 'kernel32::CreateFileW(w "$INSTDIR\HallJoy.exe", i 0x40000000, i 0, p 0, i 3, i 0, p 0) p .r0'
  ${If} $0 == -1
    MessageBox MB_ICONSTOP "Close HallJoy and check folder permissions, then retry." /SD IDOK
    SetErrorLevel 3
    Abort
  ${EndIf}
  System::Call 'kernel32::CloseHandle(p r0)'
  ${PREFIX}unlocked:
!macroend

Section "HallJoy" MainSection
  SectionIn RO
  Call ValidateDestination
  !insertmacro CheckExecutableUnlocked install_
  SetOutPath "$INSTDIR"
  ClearErrors
  File "${PAYLOAD}\HallJoy.exe"
  File "${PAYLOAD}\HallJoy.ico"
  File "${PAYLOAD}\LICENSE"
  File "${PAYLOAD}\THIRD_PARTY_NOTICES.md"
  File "${PAYLOAD}\dependency-lock.json"
  File "${PAYLOAD}\INSTALLATION.txt"
  ${If} ${Errors}
    MessageBox MB_ICONSTOP "Could not copy all files. Check disk space and folder permissions." /SD IDOK
    SetErrorLevel 4
    Abort
  ${EndIf}
  WriteINIStr "$INSTDIR\installation.ini" "HallJoy" "AppId" "${APPID}"
  WriteINIStr "$INSTDIR\installation.ini" "HallJoy" "Version" "${VERSION}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$MenuFolder"
  CreateShortCut "$MenuFolder\HallJoy.lnk" "$INSTDIR\HallJoy.exe" "" "$INSTDIR\HallJoy.ico"
  CreateShortCut "$MenuFolder\Installation help.lnk" "$WINDIR\System32\notepad.exe" '$\"$INSTDIR\INSTALLATION.txt$\"' "$INSTDIR\HallJoy.ico"
  CreateShortCut "$MenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${REGKEY}" "DisplayName" "HallJoy"
  WriteRegStr HKCU "${REGKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${REGKEY}" "Publisher" "PashOK7"
  WriteRegStr HKCU "${REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "DisplayIcon" '$\"$INSTDIR\HallJoy.ico$\"'
  WriteRegStr HKCU "${REGKEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "${REGKEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegStr HKCU "${REGKEY}" "URLInfoAbout" "https://github.com/PashOK7/HallJoy"
  WriteRegDWORD HKCU "${REGKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${REGKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD HKCU "${REGKEY}" "EstimatedSize" $0
  ${If} ${Errors}
    MessageBox MB_ICONSTOP "Could not create shortcuts or registration. Check permissions and rerun setup." /SD IDOK
    SetErrorLevel 4
    Abort
  ${EndIf}
SectionEnd

!ifdef TEST_DESKTOP
Section "Desktop shortcut" DesktopSection
!else
Section /o "Desktop shortcut" DesktopSection
!endif
  ReadINIStr $0 "$INSTDIR\installation.ini" "HallJoy" "DesktopShortcut"
  ${If} $0 != "1"
    ${If} ${FileExists} "$DesktopFolder\HallJoy.lnk"
      Goto desktop_done
    ${EndIf}
  ${EndIf}
  CreateDirectory "$DesktopFolder"
  ClearErrors
  CreateShortCut "$DesktopFolder\HallJoy.lnk" "$INSTDIR\HallJoy.exe" "" "$INSTDIR\HallJoy.ico"
  ${If} ${Errors}
    SetErrorLevel 4
    Abort
  ${EndIf}
  WriteINIStr "$INSTDIR\installation.ini" "HallJoy" "DesktopShortcut" "1"
  desktop_done:
SectionEnd

Function un.onInit
  !insertmacro InitFolders
  ReadINIStr $0 "$INSTDIR\installation.ini" "HallJoy" "AppId"
  ${If} $0 != "${APPID}"
    SetErrorLevel 2
    Abort
  ${EndIf}
  !insertmacro CheckExecutableUnlocked uninstall_
FunctionEnd

Section "Uninstall"
  ; Enumerate owned files; never recursively remove a user's directories.
  Delete "$INSTDIR\HallJoy.exe"
  Delete "$INSTDIR\HallJoy.ico"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.md"
  Delete "$INSTDIR\dependency-lock.json"
  Delete "$INSTDIR\INSTALLATION.txt"
  Delete "$MenuFolder\HallJoy.lnk"
  Delete "$MenuFolder\Installation help.lnk"
  Delete "$MenuFolder\Uninstall.lnk"
  ReadINIStr $0 "$INSTDIR\installation.ini" "HallJoy" "DesktopShortcut"
  ${If} $0 == "1"
    Delete "$DesktopFolder\HallJoy.lnk"
  ${EndIf}
  DeleteRegKey HKCU "${REGKEY}"
  Delete "$INSTDIR\installation.ini"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$MenuFolder"
  RMDir "$INSTDIR"
SectionEnd
