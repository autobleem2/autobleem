; AutoBleem for Windows - the NSIS installer (docs/pc-targets-plan.md, phase C2).
;
; Per user, no administrator rights: the program goes to %LOCALAPPDATA%\Programs\AutoBleem, the data tree
; (games, settings, themes, RetroArch) to a folder of the user's choosing - Documents\AutoBleem by default.
; The installer itself only puts the program folder in place and writes the registry; everything under the
; data folder is AutoBleemWinSetup.exe's (apps/installer/src/core/windows_install_job.*), run at the end
; with the options ticked on the components page, and again from the Start Menu later ("AutoBleem Setup").
;
; Silent (/S): an update by the launcher's Software Update, or a scripted install - the options come from
; the registry (what the user chose last time; a first silent install takes the defaults), the setup helper
; runs --quiet --update, and /RESTART starts the launcher when it is done. .onInit waits for a running
; launcher to leave (the mutex Global\AutoBleemLauncher, held by the launcher on AB_PLATFORM_WIN).
;
;   makensis -DVERSION=v2.0.0 -DSTAGE=<the staged program folder> -DOUT=<the exe> [-DICON=src/win/autobleem.ico] autobleem.nsi
;
; The staged folder is what tools/make_win_package.sh --product lays out (the launcher, its resources and
; DLLs, Themes/, emu/ when there is a pcsx-ab build) plus AutoBleemWinSetup.exe.

Unicode true
!ifndef VERSION
  !define VERSION "v0.0.0"
!endif
!ifndef STAGE
  !error "STAGE=<the staged program folder> is required"
!endif
!ifndef OUT
  !define OUT "AutoBleemSetup-${VERSION}.exe"
!endif

!define NAME "AutoBleem"
!define REGKEY "Software\AutoBleem"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoBleem"
!define LAUNCHER "autobleem-gui.exe"
!define SETUP "AutoBleemWinSetup.exe"
!define MUTEX "Global\AutoBleemLauncher"

Name "${NAME} ${VERSION}"
OutFile "${OUT}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\AutoBleem"
InstallDirRegKey HKCU "${REGKEY}" "InstallDir"
SetCompressor /SOLID lzma
BrandingText "${NAME} ${VERSION}"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"
${Using:StrFunc} StrStr

Var DataRoot
Var DataRootBox
Var OneDriveNote
Var Restart

!ifdef ICON
  !define MUI_ICON "${ICON}"
  !define MUI_UNICON "${ICON}"
!endif
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "AutoBleem ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "AutoBleem is a full-screen game launcher: your PlayStation games and, with RetroArch, the other systems' too.$\r$\n$\r$\nIt installs for you alone, with no administrator rights - the program under your profile, the games in a folder of your choosing.$\r$\n$\r$\nClick Next to continue."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${LAUNCHER}"
!define MUI_FINISHPAGE_RUN_TEXT "Start AutoBleem"
!define MUI_FINISHPAGE_TEXT "AutoBleem is installed. Put your games into$\r$\n$DataRoot\Games$\r$\n(one folder per game) - the launcher finds them by itself."
!define MUI_COMPONENTSPAGE_SMALLDESC

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom DataFolderPage DataFolderLeave
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

;*******************************
; the data folder page
;*******************************
Function DataFolderPage
  !insertmacro MUI_HEADER_TEXT "Data folder" "Where the games, settings and themes go"
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 24u "AutoBleem keeps your games, save states, settings and themes in one folder. Disc images are big: pick a drive with room. This folder is never removed by an uninstall."
  Pop $0
  ${NSD_CreateText} 0 30u 75% 12u "$DataRoot"
  Pop $DataRootBox
  ${NSD_CreateButton} 78% 29u 22% 14u "Browse..."
  Pop $0
  ${NSD_OnClick} $0 DataFolderBrowse
  ; a Documents folder OneDrive has taken over would sync every disc image to the cloud
  ${NSD_CreateLabel} 0 50u 100% 24u ""
  Pop $OneDriveNote
  Call DataFolderNote
  nsDialogs::Show
FunctionEnd

Function DataFolderBrowse
  nsDialogs::SelectFolderDialog "Where the games, settings and themes go:" "$DataRoot"
  Pop $0
  ${If} $0 != error
    StrCpy $DataRoot $0
    ${NSD_SetText} $DataRootBox "$DataRoot"
    Call DataFolderNote
  ${EndIf}
FunctionEnd

Function DataFolderNote
  ${NSD_GetText} $DataRootBox $DataRoot
  ${If} $DataRoot == ""
    StrCpy $DataRoot "$DOCUMENTS\AutoBleem"
    ${NSD_SetText} $DataRootBox "$DataRoot"
  ${EndIf}
  StrCpy $0 ""
  ${StrStr} $1 "$DataRoot" "OneDrive"
  ${If} $1 != ""
    StrCpy $0 "This folder is inside OneDrive: everything you put here would be uploaded. $PROFILE\AutoBleem stays on this PC."
  ${EndIf}
  ${NSD_SetText} $OneDriveNote "$0"
FunctionEnd

Function DataFolderLeave
  ${NSD_GetText} $DataRootBox $DataRoot
  ${If} $DataRoot == ""
    MessageBox MB_OK|MB_ICONEXCLAMATION "Please choose a data folder."
    Abort
  ${EndIf}
FunctionEnd

;*******************************
; the sections
;*******************************
Section "AutoBleem (required)" SecProgram
  SectionIn RO
  SetOutPath "$INSTDIR"
  ; the program folder is ours: a previous version's files go first, the data folder is untouched
  RMDir /r "$INSTDIR\lang"
  RMDir /r "$INSTDIR\Themes"
  RMDir /r "$INSTDIR\emu"
  File /r "${STAGE}\*.*"
  WriteRegStr HKCU "${REGKEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "DataRoot" "$DataRoot"
  WriteRegStr HKCU "${REGKEY}" "Version" "${VERSION}"
  ; the uninstaller and the Add/Remove entry
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "${NAME}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "AutoBleem"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\${LAUNCHER}"
  WriteRegStr HKCU "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINSTKEY}" "EstimatedSize" "$0"
  ; the shortcuts
  CreateDirectory "$SMPROGRAMS\AutoBleem"
  CreateShortcut "$SMPROGRAMS\AutoBleem\AutoBleem.lnk" "$INSTDIR\${LAUNCHER}" "" "$INSTDIR\${LAUNCHER}" 0
  CreateShortcut "$SMPROGRAMS\AutoBleem\AutoBleem Setup.lnk" "$INSTDIR\${SETUP}" "" "$INSTDIR\${SETUP}" 0
  CreateShortcut "$SMPROGRAMS\AutoBleem\Uninstall AutoBleem.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\AutoBleem.lnk" "$INSTDIR\${LAUNCHER}" "" "$INSTDIR\${LAUNCHER}" 0
SectionEnd

Section "Cover art databases (about 290 MB)" SecCovers
  WriteRegDWORD HKCU "${REGKEY}" "OptCovers" 1
SectionEnd

Section /o "RetroArch, for the other systems' games (about 1.5 GB)" SecRetroArch
  WriteRegDWORD HKCU "${REGKEY}" "OptRetroArch" 1
SectionEnd

Section /o "BIOS files the emulator cores need (about 230 MB)" SecBios
  WriteRegDWORD HKCU "${REGKEY}" "OptBios" 1
SectionEnd

Section "Sample games (free homebrew)" SecSamples
  WriteRegDWORD HKCU "${REGKEY}" "OptSamples" 1
SectionEnd

; the data folder: AutoBleemWinSetup with the options - its window in an interactive install, --quiet
; in a silent one (the lines go nowhere; the launcher's log has the rest)
Section "-Setup"
  ; the options not ticked are cleared, so a later silent update repeats the last choice exactly
  ${IfNot} ${SectionIsSelected} ${SecCovers}
    WriteRegDWORD HKCU "${REGKEY}" "OptCovers" 0
  ${EndIf}
  ${IfNot} ${SectionIsSelected} ${SecRetroArch}
    WriteRegDWORD HKCU "${REGKEY}" "OptRetroArch" 0
  ${EndIf}
  ${IfNot} ${SectionIsSelected} ${SecBios}
    WriteRegDWORD HKCU "${REGKEY}" "OptBios" 0
  ${EndIf}
  ${IfNot} ${SectionIsSelected} ${SecSamples}
    WriteRegDWORD HKCU "${REGKEY}" "OptSamples" 0
  ${EndIf}
  StrCpy $0 ' --root "$DataRoot" --program "$INSTDIR"'
  ${If} ${SectionIsSelected} ${SecCovers}
    StrCpy $0 '$0 --covers JUP'
  ${Else}
    StrCpy $0 '$0 --covers ""'
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecRetroArch}
    StrCpy $0 '$0 --retroarch'
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecBios}
    StrCpy $0 '$0 --bios'
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecSamples}
    StrCpy $0 '$0 --samples'
  ${EndIf}
  ${If} ${Silent}
    DetailPrint "AutoBleemWinSetup --quiet --update$0"
    ExecWait '"$INSTDIR\${SETUP}" --quiet --update$0' $1
  ${Else}
    DetailPrint "AutoBleemWinSetup$0"
    ExecWait '"$INSTDIR\${SETUP}"$0' $1
  ${EndIf}
  ${If} $Restart == 1
    Exec '"$INSTDIR\${LAUNCHER}"'
  ${EndIf}
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecProgram} "The launcher, its themes and resources."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCovers} "Cover art and titles for the PlayStation library, for the three regions (downloaded)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecRetroArch} "libretro's RetroArch with every core, so the launcher plays the other systems' games too (downloaded)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecBios} "The BIOS files the cores need, fetched by AutoBleem's list from RetroBIOS (needs RetroArch)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSamples} "A few freely redistributable games, so the shelf is not empty."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;*******************************
; init: the previous choices, a running launcher
;*******************************
Function .onInit
  ; one installer at a time
  System::Call 'kernel32::CreateMutex(p 0, i 0, t "Global\AutoBleemSetup") p .r1 ?e'
  Pop $0
  ${If} $0 == 183
    MessageBox MB_OK|MB_ICONEXCLAMATION "The AutoBleem installer is already running."
    Abort
  ${EndIf}
  ${GetParameters} $0
  ${GetOptions} $0 "/RESTART" $1
  ${IfNot} ${Errors}
    StrCpy $Restart 1
  ${EndIf}
  ; the launcher holds this mutex while it runs: an update waits for it to leave (the launcher's own
  ; Software Update exits after starting us), a user is asked
  StrCpy $2 0
  loop:
    System::Call 'kernel32::OpenMutex(i 0x100000, i 0, t "${MUTEX}") p .r1'
    ${If} $1 == 0
      Goto ready
    ${EndIf}
    System::Call 'kernel32::CloseHandle(p r1)'
    ${If} ${Silent}
      IntOp $2 $2 + 1
      ${If} $2 > 60
        Abort
      ${EndIf}
      Sleep 500
      Goto loop
    ${EndIf}
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "AutoBleem is running. Close it (the Power Off item in its menu), then click Retry." IDRETRY loop
    Abort
  ready:
  ; what was chosen last time, for the pages and for a silent run
  ReadRegStr $DataRoot HKCU "${REGKEY}" "DataRoot"
  ${If} $DataRoot == ""
    StrCpy $DataRoot "$DOCUMENTS\AutoBleem"
  ${EndIf}
  ReadRegDWORD $0 HKCU "${REGKEY}" "OptCovers"
  ${IfNot} ${Errors}
    ${If} $0 == 1
      !insertmacro SelectSection ${SecCovers}
    ${Else}
      !insertmacro UnselectSection ${SecCovers}
    ${EndIf}
  ${EndIf}
  ReadRegDWORD $0 HKCU "${REGKEY}" "OptRetroArch"
  ${IfNot} ${Errors}
    ${If} $0 == 1
      !insertmacro SelectSection ${SecRetroArch}
    ${Else}
      !insertmacro UnselectSection ${SecRetroArch}
    ${EndIf}
  ${EndIf}
  ReadRegDWORD $0 HKCU "${REGKEY}" "OptBios"
  ${IfNot} ${Errors}
    ${If} $0 == 1
      !insertmacro SelectSection ${SecBios}
    ${Else}
      !insertmacro UnselectSection ${SecBios}
    ${EndIf}
  ${EndIf}
  ReadRegDWORD $0 HKCU "${REGKEY}" "OptSamples"
  ${IfNot} ${Errors}
    ${If} $0 == 1
      !insertmacro SelectSection ${SecSamples}
    ${Else}
      !insertmacro UnselectSection ${SecSamples}
    ${EndIf}
  ${EndIf}
FunctionEnd

;*******************************
; the uninstaller: the program, the shortcuts, the registry - the data folder stays
;*******************************
Section "Uninstall"
  ReadRegStr $DataRoot HKCU "${REGKEY}" "DataRoot"
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\AutoBleem\AutoBleem.lnk"
  Delete "$SMPROGRAMS\AutoBleem\AutoBleem Setup.lnk"
  Delete "$SMPROGRAMS\AutoBleem\Uninstall AutoBleem.lnk"
  RMDir "$SMPROGRAMS\AutoBleem"
  Delete "$DESKTOP\AutoBleem.lnk"
  DeleteRegKey HKCU "${UNINSTKEY}"
  DeleteRegKey HKCU "${REGKEY}"
  ${IfNot} ${Silent}
    ${If} $DataRoot != ""
      MessageBox MB_OK "AutoBleem is uninstalled. Your games, settings and themes are still in$\r$\n$DataRoot$\r$\n- delete that folder yourself if you no longer want them."
    ${EndIf}
  ${EndIf}
SectionEnd
