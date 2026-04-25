!include "MUI2.nsh"

!define APP_NAME   "SprintToolBox"
!define APP_EXE    "SprintToolBox.exe"
!define PUBLISHER  "SprintToolBox"
!define REG_APP    "Software\${APP_NAME}"
!define REG_UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
!define REG_RUN    "Software\Microsoft\Windows\CurrentVersion\Run"

!ifndef APP_VERSION
  !define APP_VERSION "1.0.13"
!endif
!ifndef BUILD_DATE
  !define BUILD_DATE "00000000"
!endif
!ifndef STAGE_DIR
  !define STAGE_DIR "."
!endif
!ifndef OUT_DIR
  !define OUT_DIR "."
!endif

OutFile "${OUT_DIR}\SprintToolBox-${APP_VERSION}-${BUILD_DATE}-windows-x86_64.exe"
Name    "${APP_NAME} ${APP_VERSION}"

; Install to per-user local app data – no UAC elevation required.
InstallDir "$LOCALAPPDATA\${APP_NAME}"
InstallDirRegKey HKCU "${REG_APP}" "InstallDir"
RequestExecutionLevel user
ShowInstDetails show

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install ───────────────────────────────────────────────────────────────────
Section "Install" SecMain
    SetOutPath "$INSTDIR"
    File /r "${STAGE_DIR}\*"

    WriteRegStr HKCU "${REG_APP}" "InstallDir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr   HKCU "${REG_UNINST}" "DisplayName"     "${APP_NAME}"
    WriteRegStr   HKCU "${REG_UNINST}" "UninstallString"  '"$INSTDIR\Uninstall.exe"'
    WriteRegStr   HKCU "${REG_UNINST}" "DisplayVersion"  "${APP_VERSION}"
    WriteRegStr   HKCU "${REG_UNINST}" "Publisher"       "${PUBLISHER}"
    WriteRegDWORD HKCU "${REG_UNINST}" "NoModify"        1
    WriteRegDWORD HKCU "${REG_UNINST}" "NoRepair"        1

    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortCut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"  "$INSTDIR\${APP_EXE}"
    CreateShortCut  "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"    "$INSTDIR\Uninstall.exe"
SectionEnd

; ── Uninstall ─────────────────────────────────────────────────────────────────
Section "Uninstall"
    ; Remove autostart entry if the user had enabled it via the tray menu.
    DeleteRegValue HKCU "${REG_RUN}" "${APP_NAME}"

    Delete "$INSTDIR\*.*"
    RMDir /r "$INSTDIR"

    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir  "$SMPROGRAMS\${APP_NAME}"

    DeleteRegKey HKCU "${REG_UNINST}"
    DeleteRegKey HKCU "${REG_APP}"
SectionEnd
