; installer.nsi – NSIS installer script for SprintToolBox
; Build with: makensis installer.nsi
; Requires: NSIS 3.x (mingw-w64-ucrt-x86_64-nsis in MSYS2)

Unicode True

;------------------------------------------------------------------------------
; Metadata (set by package_win.sh via /D flags)
;------------------------------------------------------------------------------
!ifndef APP_VERSION
  !define APP_VERSION "1.0.0"
!endif
!ifndef BUILD_DATE
  !define BUILD_DATE "00000000"
!endif

!define APP_NAME        "SprintToolBox"
!define APP_EXE         "SprintToolBox.exe"
!define PUBLISHER       "SprintToolBox"
!define INST_DIR        "$PROGRAMFILES64\${APP_NAME}"
!define REG_UNINSTALL   "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
!define REG_RUN         "Software\Microsoft\Windows\CurrentVersion\Run"
!define OUT_FILE        "SprintToolBox-${APP_VERSION}-${BUILD_DATE}-windows-x86_64.exe"

;------------------------------------------------------------------------------
; General settings
;------------------------------------------------------------------------------
Name                "${APP_NAME} ${APP_VERSION}"
OutFile             "${OUT_FILE}"
InstallDir          "${INST_DIR}"
InstallDirRegKey    HKLM "${REG_UNINSTALL}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor       /SOLID lzma
ShowInstDetails     show
ShowUninstDetails   show

;------------------------------------------------------------------------------
; Pages
;------------------------------------------------------------------------------
!include "MUI2.nsh"

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom AutostartPage AutostartPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;------------------------------------------------------------------------------
; Autostart custom page
;------------------------------------------------------------------------------
Var AutostartCheckbox
Var DoAutostart

Function AutostartPage
  !insertmacro MUI_HEADER_TEXT "Startup Options" "Choose how ${APP_NAME} starts."
  nsDialogs::Create 1018
  Pop $0

  ${NSD_CreateLabel} 0 0 100% 24u "${APP_NAME} runs in the system tray. You can have it start automatically when you log in to Windows."
  Pop $0

  ${NSD_CreateCheckbox} 0 36u 100% 12u "Start ${APP_NAME} automatically when Windows starts"
  Pop $AutostartCheckbox
  ${NSD_SetState} $AutostartCheckbox ${BST_CHECKED}   ; checked by default

  nsDialogs::Show
FunctionEnd

Function AutostartPageLeave
  ${NSD_GetState} $AutostartCheckbox $DoAutostart
FunctionEnd

;------------------------------------------------------------------------------
; Install section
;------------------------------------------------------------------------------
Section "MainSection" SEC_MAIN
  SetOutPath "$INSTDIR"
  SetOverwrite on

  ; Application files (staged by package_win.sh into dist\win\SprintToolBox\)
  File /r "${STAGE_DIR}\*.*"

  ; Start Menu shortcut
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" \
                  "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0 \
                  SW_SHOWNORMAL "" "Sprint tracking tray utility"
  CreateShortcut  "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" \
                  "$INSTDIR\Uninstall.exe"

  ; Autostart (HKCU so no UAC needed at runtime)
  ${If} $DoAutostart == ${BST_CHECKED}
    WriteRegStr HKCU "${REG_RUN}" "${APP_NAME}" '"$INSTDIR\${APP_EXE}"'
  ${EndIf}

  ; Uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Add/Remove Programs entry
  WriteRegStr   HKLM "${REG_UNINSTALL}" "DisplayName"          "${APP_NAME}"
  WriteRegStr   HKLM "${REG_UNINSTALL}" "DisplayVersion"       "${APP_VERSION}"
  WriteRegStr   HKLM "${REG_UNINSTALL}" "Publisher"            "${PUBLISHER}"
  WriteRegStr   HKLM "${REG_UNINSTALL}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKLM "${REG_UNINSTALL}" "UninstallString"      '"$INSTDIR\Uninstall.exe"'
  WriteRegStr   HKLM "${REG_UNINSTALL}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoModify"             1
  WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoRepair"             1
SectionEnd

;------------------------------------------------------------------------------
; Uninstall section
;------------------------------------------------------------------------------
Section "Uninstall"
  ; Kill running instance
  ExecWait 'taskkill /F /IM "${APP_EXE}"' $0

  ; Remove files
  RMDir /r "$INSTDIR"

  ; Remove Start Menu
  RMDir /r "$SMPROGRAMS\${APP_NAME}"

  ; Remove autostart entry
  DeleteRegValue HKCU "${REG_RUN}" "${APP_NAME}"

  ; Remove Add/Remove Programs entry
  DeleteRegKey HKLM "${REG_UNINSTALL}"
SectionEnd
