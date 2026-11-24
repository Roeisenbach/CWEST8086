; CWEST8086 Setup -- installs the packaged release as a Windows program.
;
; Packs release\CWEST8086\ (made by tools/make_release.ps1) into
; release\CWEST8086-<version>-setup.exe. make_release.ps1 runs this itself when
; NSIS is installed; to run it by hand:
;
;   makensis tools\installer.nsi
;
; Optional defines: -DSRC=<release folder> -DVERSION=x.y.z -DOUTFILE=<setup.exe>.
; The version defaults to the ProductVersion in assets\app.rc, like the zip's.
;
; Where it installs: %LOCALAPPDATA%\Programs\CWEST8086, for the current Windows
; account -- the same place VS Code, Discord and the like put a per-user
; install. No administrator rights are needed, and the folder stays writable,
; which the app relies on: it keeps cwest.ini and corewars_layout.ini next to
; its exe, the GPU engine caches its pipeline in gpu-engine\, and the CGX
; engine writes its scores and builds its window in CGX-engine\. Program Files
; is read-only to a running program, so the directory page refuses folders the
; installer cannot write to.
;
; What it sets up: the program folder, a Start menu entry, an optional desktop
; shortcut, an entry in Settings > Apps (Apps & features) with an uninstaller,
; and an App Paths entry so Win+R "CWEST8086" starts it. Installing over an
; existing copy upgrades it and keeps its settings.

Unicode true
ManifestDPIAware true
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 64

!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh
!include FileFunc.nsh

; ---- inputs ---------------------------------------------------------------------
!define APP      "CWEST8086"
; the project root, one up from this script. ${__FILEDIR__} is the script's
; folder as given to makensis: a full path from the Windows GUI/right-click
; compile (which keeps its own working folder), possibly a relative one from a
; command line, where makensis has already moved into the script's folder.
!if /FileExists "${__FILEDIR__}/../assets/app.rc"
  !define ROOT "${__FILEDIR__}/.."
!else
  !define ROOT ".."
!endif

; everything is found from where this script sits, so it has to stay in the
; project's tools\ folder (a copy in Downloads finds nothing)
!if ! /FileExists "${ROOT}/assets/app.rc"
  !error "installer.nsi has to be in the project's tools\ folder (this copy is in ${__FILEDIR__}). Run the one in your CWEST8086 project's tools\ folder."
!endif

!ifndef SRC
  !define SRC "${ROOT}/release/${APP}"
!endif
!if ! /FileExists "${SRC}/${APP}.exe"
  !error "No release to pack: ${SRC}/${APP}.exe is missing. Run the Make Release task (tools\make_release.ps1) first -- with NSIS installed it builds the setup.exe too."
!endif

!ifndef VERSION
  ; the line is   VALUE "ProductVersion",  "0.6.0.0"   -- take what follows the
  ; comma, drop the quotes and spaces, and keep the first three numbers
  !searchparse /file "${ROOT}/assets/app.rc" `"ProductVersion",` RC_PV
  !searchreplace RC_PV `${RC_PV}` `"` ``
  !searchreplace RC_PV `${RC_PV}` ` ` ``
  !searchparse `#${RC_PV}` `#` V_MAJ `.` V_MIN `.` V_PAT `.`
  !define VERSION "${V_MAJ}.${V_MIN}.${V_PAT}"
!endif

!ifndef OUTFILE
  !define OUTFILE "${ROOT}/release/${APP}-${VERSION}-setup.exe"
!endif

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP}"
!define APPPATH_KEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${APP}.exe"
!define DESCRIPTION "Core Wars 8086 editor, simulator and tournaments"

Name "${APP}"
Caption "${APP} ${VERSION} Setup"
UninstallCaption "Uninstall ${APP}"
BrandingText "${APP} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${APP}"
InstallDirRegKey HKCU "${UNINST_KEY}" "InstallLocation"
ShowInstDetails nevershow
ShowUninstDetails nevershow

!pragma warning disable 9100   ; no LegalCopyright key
VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName"     "${APP}"
VIAddVersionKey /LANG=1033 "ProductVersion"  "${VERSION}"
VIAddVersionKey /LANG=1033 "FileVersion"     "${VERSION}"
VIAddVersionKey /LANG=1033 "FileDescription" "${APP} Setup"

; ---- pages ----------------------------------------------------------------------
!define MUI_ICON   "${ROOT}/assets/app.ico"
!define MUI_UNICON "${ROOT}/assets/app.ico"
!if /FileExists "${ROOT}/assets/setup-side.bmp"                       ; tools/makesetupart.py
  !define MUI_WELCOMEFINISHPAGE_BITMAP "${ROOT}/assets/setup-side.bmp"
!endif
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_NODESC

!define MUI_WELCOMEPAGE_TEXT "This will install ${APP} ${VERSION} for your Windows account.$\r$\n$\r$\nNo administrator rights are needed. If ${APP} is already installed, it is upgraded and keeps its settings.$\r$\n$\r$\nClick Next to continue."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!define MUI_DIRECTORYPAGE_TEXT_TOP "${APP} keeps its settings in its own folder, so it needs a folder you can write to. The suggested one is just for your account; Program Files will not work."
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${APP}"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Show the README"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---- helpers --------------------------------------------------------------------
; A running exe cannot be opened for writing, so this finds a copy of the app
; (or a program it started) still running out of the install folder.
!macro FIND_RUNNING un
Function ${un}FindRunning
  ; out: $R0 = the busy file, or "" when nothing is running
  Push $0
  Push $1
  Push $2
  StrCpy $R0 ""
  StrCpy $1 0
  ${Do}
    IntOp $1 $1 + 1
    ${Select} $1
      ${Case} 1
        StrCpy $0 "${APP}.exe"
      ${Case} 2
        StrCpy $0 "cwnode.exe"
      ${Case} 3
        StrCpy $0 "cloudflared.exe"
      ${Case} 4
        StrCpy $0 "gpu-engine\engine.exe"
      ${CaseElse}
        ${ExitDo}
    ${EndSelect}
    ${If} ${FileExists} "$INSTDIR\$0"
      ClearErrors
      FileOpen $2 "$INSTDIR\$0" a
      ${If} ${Errors}
        StrCpy $R0 $0
        ${ExitDo}
      ${EndIf}
      FileClose $2
    ${EndIf}
  ${Loop}
  Pop $2
  Pop $1
  Pop $0
FunctionEnd

Function ${un}WaitUntilClosed
  ${Do}
    Call ${un}FindRunning
    ${If} $R0 == ""
      ${ExitDo}
    ${EndIf}
    ; silent: there is nobody to ask, so give up
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "${APP} is still running ($INSTDIR\$R0).$\r$\n$\r$\nClose it, then press Retry." /SD IDCANCEL IDRETRY retry
    Abort
    retry:
  ${Loop}
FunctionEnd
!macroend
!insertmacro FIND_RUNNING ""
!insertmacro FIND_RUNNING "un."

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "${APP} needs 64-bit Windows." /SD IDOK
    Abort
  ${EndIf}
FunctionEnd

; The app writes beside its exe, so a folder the installer cannot write to
; would install but not work. Program Files is the usual one.
Function DirectoryLeave
  ClearErrors
  CreateDirectory "$INSTDIR"
  FileOpen $0 "$INSTDIR\cwest-setup-write-test.tmp" w
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONEXCLAMATION "Windows does not let programs write to$\r$\n$INSTDIR$\r$\n$\r$\n${APP} saves its settings in its own folder, so please pick a folder in your user profile (the suggested one works)."
    RMDir "$INSTDIR"
    Abort
  ${EndIf}
  FileClose $0
  Delete "$INSTDIR\cwest-setup-write-test.tmp"
  Call WaitUntilClosed
FunctionEnd

; ---- install --------------------------------------------------------------------
Section "${APP}" SecApp
  SectionIn RO
  Call WaitUntilClosed

  ; an upgrade: clear out what a new version replaces wholesale, so files a
  ; newer release dropped do not linger. Settings, the GPU pipeline cache, user
  ; fonts and the CGX survivors/zombies stay.
  RMDir /r "$INSTDIR\licenses"
  RMDir /r "$INSTDIR\gpu-engine\shaders"
  RMDir /r "$INSTDIR\CGX-engine\engine"
  RMDir /r "$INSTDIR\CGX-engine\lib"
  RMDir /r "$INSTDIR\CGX-engine\gui"
  RMDir /r "$INSTDIR\CGX-engine\lane"

  SetOutPath "$INSTDIR"
  ; everything in the release folder, minus what the app writes when it is run
  ; from there -- installing those would overwrite the user's own
  File /r /x cwest.ini /x corewars_layout.ini /x crash.log \
          /x pipeline_cache.bin /x score.csv /x scores.csv /x scores.txt /x last-run.txt \
          /x survivors /x zombies \
          /x perf.txt /x perf_open.txt /x shot.txt /x shotvp.txt /x testclip.txt /x testpaste.txt \
          "${SRC}\*.*"
  CreateDirectory "$INSTDIR\CGX-engine\survivors"
  CreateDirectory "$INSTDIR\CGX-engine\zombies"

  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Settings > Apps
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayName"          "${APP}"
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayVersion"       "${VERSION}"
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayIcon"          "$INSTDIR\${APP}.exe,0"
  WriteRegStr   HKCU "${UNINST_KEY}" "Comments"             "${DESCRIPTION}"
  WriteRegStr   HKCU "${UNINST_KEY}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKCU "${UNINST_KEY}" "UninstallString"      '"$INSTDIR\uninstall.exe"'
  WriteRegStr   HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"

  ; Win+R "CWEST8086"
  WriteRegStr HKCU "${APPPATH_KEY}" "" "$INSTDIR\${APP}.exe"
  WriteRegStr HKCU "${APPPATH_KEY}" "Path" "$INSTDIR"
SectionEnd

Section "Start menu entry" SecStartMenu
  SetOutPath "$INSTDIR"
  CreateShortcut "$SMPROGRAMS\${APP}.lnk" "$INSTDIR\${APP}.exe" "" "$INSTDIR\${APP}.exe" 0 SW_SHOWNORMAL "" "${DESCRIPTION}"
SectionEnd

Section "Desktop shortcut" SecDesktop
  SetOutPath "$INSTDIR"
  CreateShortcut "$DESKTOP\${APP}.lnk" "$INSTDIR\${APP}.exe" "" "$INSTDIR\${APP}.exe" 0 SW_SHOWNORMAL "" "${DESCRIPTION}"
SectionEnd

; ---- uninstall ------------------------------------------------------------------
; Removes the program and the settings it kept beside itself. Workspaces live
; wherever the user put them and are never touched; anything the user added to
; CGX-engine\survivors or zombies stays, and so does the folder around it.
Section "Uninstall"
  Call un.WaitUntilClosed

  Delete "$SMPROGRAMS\${APP}.lnk"
  Delete "$DESKTOP\${APP}.lnk"
  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "${APPPATH_KEY}"

  Delete "$INSTDIR\${APP}.exe"
  Delete "$INSTDIR\cwnode.exe"
  Delete "$INSTDIR\cloudflared.exe"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\cwest.ini"
  Delete "$INSTDIR\corewars_layout.ini"
  Delete "$INSTDIR\crash.log"
  RMDir /r "$INSTDIR\fonts"
  RMDir /r "$INSTDIR\licenses"
  RMDir /r "$INSTDIR\gpu-engine"

  RMDir /r "$INSTDIR\CGX-engine\engine"
  RMDir /r "$INSTDIR\CGX-engine\lib"
  RMDir /r "$INSTDIR\CGX-engine\gui"
  RMDir /r "$INSTDIR\CGX-engine\lane"
  Delete "$INSTDIR\CGX-engine\README.md"
  Delete "$INSTDIR\CGX-engine\RUN.bat"
  Delete "$INSTDIR\CGX-engine\run.sh"
  Delete "$INSTDIR\CGX-engine\scores.csv"
  Delete "$INSTDIR\CGX-engine\last-run.txt"
  RMDir "$INSTDIR\CGX-engine\survivors"
  RMDir "$INSTDIR\CGX-engine\zombies"
  RMDir "$INSTDIR\CGX-engine"

  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  ${If} ${FileExists} "$INSTDIR\*.*"
    DetailPrint "Kept $INSTDIR: it still holds files you added."
  ${EndIf}
SectionEnd
