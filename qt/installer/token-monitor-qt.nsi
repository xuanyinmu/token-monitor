# Token Monitor (Qt) - Windows installer.
#
# Built by qt/scripts/package.ps1, which passes /DVERSION, /DVI_VERSION,
# /DSRCDIR (the staged deployment), /DOUTFILE and /DAPPICO. Running makensis on
# this file by hand needs those defines: SRCDIR is validated at compile time so
# a half-staged tree cannot produce an installer that is missing its runtime.
#
# Keep this file ASCII-only. makensis decodes the script with the system code
# page unless a BOM or /INPUTCHARSET says otherwise, and NSIS 3.0.x (the build
# electron-builder caches) aborts with "Bad text encoding" on non-ASCII UTF-8.
#
# Design notes worth keeping:
#  - per-user install (RequestExecutionLevel user, $LOCALAPPDATA\Programs): no
#    UAC, same mode as the Electron build's NSIS installer;
#  - nothing private is ever written outside $INSTDIR and the documented user
#    data directories, so the uninstaller can be exhaustive;
#  - the uninstaller deletes everything this product owns. %APPDATA%\Token
#    Monitor is shared with the Electron build, so that directory is only
#    removed wholesale when nothing else uses it, when /KEEPDATA was not
#    passed to a silent uninstall, or when the user answers Yes.

Unicode true

!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
# Four-part numeric form for the PE version resource (0.57.0 -> 0.57.0.0);
# package.ps1 passes it because NSIS cannot assemble it from VERSION.
!ifndef VI_VERSION
  !define VI_VERSION "0.0.0.0"
!endif
!ifndef OUTFILE
  !define OUTFILE "Token-Monitor-Qt-Setup-${VERSION}.exe"
!endif
!ifndef SRCDIR
  !error "SRCDIR is not defined. Build this installer with qt/scripts/package.ps1."
!endif
!if /FileExists "${SRCDIR}\TokenMonitorQt.exe"
!else
  !error "SRCDIR does not look like a staged Token Monitor (Qt) deployment: ${SRCDIR}"
!endif
!if /FileExists "${SRCDIR}\tokscale.exe"
!else
  !error "SRCDIR is missing tokscale.exe, so the installed widget could not scan usage: ${SRCDIR}"
!endif

!define APP_NAME "Token Monitor (Qt)"
!define APP_EXE "TokenMonitorQt.exe"
!define SHORTCUT_NAME "Token Monitor (Qt).lnk"
!define RUN_VALUE "TokenMonitor"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\TokenMonitorQt"
# The shared directory: same path, and largely the same file names, as the
# Electron build's userData directory.
!define USER_DATA_DIR "$APPDATA\Token Monitor"

# Icons: the .ico is also embedded in the widget exe (resources/app.rc), so the
# installer, the shortcuts and the ARPP entry all agree on one piece of artwork.
!ifdef APPICO
  !define MUI_ICON "${APPICO}"
  !define MUI_UNICON "${APPICO}"
  Icon "${APPICO}"
  UninstallIcon "${APPICO}"
!endif

!include "MUI2.nsh"

Name "${APP_NAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Token Monitor Qt"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetOverwrite on

Var sharedFlag      # 1 when the user data directory is shared with Electron
Var keepData        # 1 when /KEEPDATA was passed
Var deleteAll       # 1 to remove the shared directory wholesale
Var askUser         # 1 when the shared-directory question has to be asked
Var residue         # 1 when something we own survived the uninstall

VIProductVersion "${VI_VERSION}"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${APP_NAME} installer"
VIAddVersionKey "CompanyName" "Javis"
VIAddVersionKey "LegalCopyright" "MIT licensed"

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Run Token Monitor now"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

ShowInstDetails show
ShowUninstDetails show

Function .onInit
  # Per-user shortcuts and ARPP entry, regardless of how the installer was
  # started (SetShellVarContext is only valid inside a Section or Function).
  SetShellVarContext current
  # A 32-bit NSIS process is subject to WOW64 registry redirection, so the Run
  # value and the "Apps & features" entry would land in HKCU\Software\
  # WOW6432Node while the widget (64-bit) writes the native view. The two would
  # then disagree and an uninstall would leave the widget's autostart behind.
  # SetRegView is the stub-independent way to do this: Target amd64-unicode
  # needs the amd64 stubs, which NSIS 3.0.x (the build electron-builder caches)
  # does not ship.
  SetRegView 64
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "This build is 64-bit only (Windows x64)."
    Abort
  ${EndIf}
FunctionEnd

Function un.onInit
  SetShellVarContext current
  SetRegView 64
FunctionEnd

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

Section "${APP_NAME}" SEC_CORE
  SectionIn RO
  SetOutPath "$INSTDIR"
  # A running widget holds its exe and the QML pipeline cache open. Settings are
  # written on change, so the only loss is an edit that has not been committed.
  DetailPrint "Stopping a running ${APP_EXE}"
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM ${APP_EXE}'
  Sleep 500

  File /r "${SRCDIR}\*.*"

  CreateDirectory "$SMPROGRAMS"
  CreateShortCut "$SMPROGRAMS\${SHORTCUT_NAME}" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "Javis"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE},0"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  # Deliberately no QuietUninstallString: a silent uninstall removes the data
  # directory shared with the Electron build, and that must be an explicit act.
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
  CreateShortCut "$DESKTOP\${SHORTCUT_NAME}" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
SectionEnd

Section "Start with Windows" SEC_AUTOSTART
  # Same value name the widget writes from Settings > startAtLogin, and quoted
  # because the default install path contains a space: Windows' Run-key parsing
  # splits an unquoted path and the app silently never starts.
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${RUN_VALUE}" '"$INSTDIR\${APP_EXE}"'
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE} "The widget, the hub and the agent CLI, the Qt runtime and the bundled tokscale scanner."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Create a shortcut on the desktop."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_AUTOSTART} "Start Token Monitor when you sign in (can also be changed later in Settings)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

# ---------------------------------------------------------------------------
# Uninstall - "leave nothing behind" is the contract, so the list below is the
# complete footprint. Files that belong to the Electron build are only removed
# when the shared directory is removed as a whole.
# ---------------------------------------------------------------------------

Section "Uninstall"
  # 1) Our processes first: a running widget keeps its exe and the pipeline
  #    cache mapped, and a locked file would strand the install directory.
  #    Plain ExecWait keeps the script free of plugins, so a bare makensis
  #    (electron-builder's cached one included) can compile it.
  DetailPrint "Stopping running Token Monitor processes"
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM ${APP_EXE}'
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM TokenMonitorHub.exe'
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM TokenMonitorAgent.exe'
  Sleep 800

  # 2) Shell integration, autostart and the "Apps & features" entry.
  Delete "$SMPROGRAMS\${SHORTCUT_NAME}"
  Delete "$DESKTOP\${SHORTCUT_NAME}"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${RUN_VALUE}"
  DeleteRegKey HKCU "${UNINST_KEY}"

  # 3) Qt-owned files inside the shared user data directory. These names are
  #    written only by the Qt build (limits-snapshot.json, the standalone hub's
  #    data\devices.json, the spend stores under data\, the qt-* scratch files,
  #    and the tokscale.exe the Qt README installs at the root) - Electron keeps
  #    its own tokscale under <userData>\tokscale\ and never reads these.
  Delete "${USER_DATA_DIR}\limits-snapshot.json"
  Delete "${USER_DATA_DIR}\tokscale.exe"
  Delete "${USER_DATA_DIR}\qt-*.json"
  Delete "${USER_DATA_DIR}\qt-*.log"
  RMDir /r "${USER_DATA_DIR}\data"

  # 4) Qt caches outside userData. Both paths are Qt's (the widget sets
  #    organizationName=Javis for the pipeline cache; the QML disk cache lands
  #    under the application name) and both regenerate on the next run.
  Delete "$LOCALAPPDATA\Token Monitor\qt-*.json"
  Delete "$LOCALAPPDATA\Token Monitor\qt-*.log"
  RMDir /r "$LOCALAPPDATA\Token Monitor\cache"
  RMDir /r "$LOCALAPPDATA\Javis\Token Monitor"

  # 5) Is %APPDATA%\Token Monitor shared with the Electron build? Its own files
  #    are the Chromium profile, the archive/rate stores and the updater's
  #    tokscale\ directory; settings.json, credentials.json, history.json,
  #    collector-anchor.json, agent.pid and hub-devices.json exist in both
  #    builds, so they cannot decide the question on their own.
  StrCpy $sharedFlag "0"
  ${If} ${FileExists} "${USER_DATA_DIR}\Local State"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\Preferences"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\.updaterId"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\blob_storage\*.*"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\Local Storage\*.*"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\GPUCache\*.*"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\daily-history-archive.json"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\session-usage-archive.json"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\exchange-rates.json"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\tokscale\*.*"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "$LOCALAPPDATA\Programs\token-monitor\*.*"
    StrCpy $sharedFlag "1"
  ${EndIf}
  ${If} ${FileExists} "$LOCALAPPDATA\Programs\Token Monitor\*.*"
    StrCpy $sharedFlag "1"
  ${EndIf}
  # The Electron build's login item (its value name is the appId, not ours).
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "com.javis.tokenmonitor"
  ${If} $0 != ""
    StrCpy $sharedFlag "1"
  ${EndIf}
  # A "Token Monitor" (exact) entry in Apps & features is the Electron install.
  StrCpy $1 0
  uninst_scan_loop:
    EnumRegKey $2 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall" $1
    ${If} $2 == ""
      Goto uninst_scan_done
    ${EndIf}
    ${If} $2 != "TokenMonitorQt"
      ReadRegStr $3 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$2" "DisplayName"
      ${If} $3 == "Token Monitor"
        StrCpy $sharedFlag "1"
      ${EndIf}
    ${EndIf}
    IntOp $1 $1 + 1
    Goto uninst_scan_loop
  uninst_scan_done:

  # /KEEPDATA is parsed with a plain case-insensitive scan of the parameter
  # string. FileFunc's ${GetOptions} is not usable here: on NSIS 3.0.x it
  # returns "" even for a literal "/S /KEEPDATA" needle, and the uninstaller
  # actually runs from the temp copy NSIS re-launches, whose command line reads
  # '"<temp>\Un_A.exe"  /S /KEEPDATA' (note the double space). Measured with an
  # instrumented build: GetOptions -> "", GetParameters -> "/S /KEEPDATA".
  StrCpy $keepData "0"
  ${GetParameters} $5
  StrCpy $6 $5
  uninst_keepdata_scan:
    StrCmp $6 "" uninst_keepdata_done
    StrCpy $7 $6 9                      # length of "/KEEPDATA"
    StrCmp $7 "/KEEPDATA" 0 uninst_keepdata_next   # StrCmp is case-insensitive
    StrCpy $keepData "1"
    Goto uninst_keepdata_done
  uninst_keepdata_next:
    StrCpy $6 $6 "" 1
    Goto uninst_keepdata_scan
  uninst_keepdata_done:

  # 6) Decide what happens to the shared directory.
  StrCpy $deleteAll "0"
  StrCpy $askUser "0"
  ${If} $sharedFlag == "0"
    # Nothing else uses it, so "leave nothing behind" has one obvious meaning.
    StrCpy $deleteAll "1"
  ${ElseIf} $keepData == "1"
    DetailPrint "Keeping data shared with the Electron build (/KEEPDATA)."
  ${ElseIf} ${Silent}
    # A silent uninstall is the automation path; the deliberate opt-out is
    # /KEEPDATA, so the default stays "remove everything".
    StrCpy $deleteAll "1"
  ${Else}
    StrCpy $askUser "1"
  ${EndIf}

  StrCmp $askUser "1" 0 uninst_choice_done
  MessageBox MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON1 \
    "${USER_DATA_DIR} is shared with the Electron build of Token Monitor.$\r$\n$\r$\nYes: delete the whole folder. settings.json, credentials.json and history.json belong to the Electron build as well, so its configuration and API credentials are erased too.$\r$\n$\r$\nNo: keep those shared files and remove only the Qt build's own files.$\r$\n$\r$\nDelete all shared user data?" \
    IDNO uninst_keep_shared
  StrCpy $deleteAll "1"
  Goto uninst_choice_done
  uninst_keep_shared:
  StrCpy $deleteAll "0"
  uninst_choice_done:

  ${If} $deleteAll == "1"
    ${If} $APPDATA != ""
      DetailPrint "Removing shared user data: ${USER_DATA_DIR}"
      RMDir /r "${USER_DATA_DIR}"
    ${EndIf}
  ${EndIf}

  # 7) Prune what is now empty, so an uninstall leaves no empty husks behind.
  ${If} $LOCALAPPDATA != ""
    RMDir "$LOCALAPPDATA\Token Monitor"
    RMDir "$LOCALAPPDATA\Javis"
  ${EndIf}
  DeleteRegKey /ifempty HKCU "Software\Javis"

  # 8) Program files. The recursive delete is gated on our own marker: the
  #    directory page lets the user install anywhere, and RMDir /r on a
  #    directory that is not ours would be destructive.
  ${If} ${FileExists} "$INSTDIR\${APP_EXE}"
    RMDir /r "$INSTDIR"
  ${Else}
    DetailPrint "WARNING: $INSTDIR\${APP_EXE} is gone; deleting only known file names."
    Delete "$INSTDIR\${APP_EXE}"
    Delete "$INSTDIR\TokenMonitorHub.exe"
    Delete "$INSTDIR\TokenMonitorAgent.exe"
    Delete "$INSTDIR\tokscale.exe"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
  ${EndIf}

  # 9) Audit the result instead of trusting it: any residue we own is reported
  #    in the details pane, and interactively it is also surfaced in a dialog.
  StrCpy $residue "0"
  ${If} ${FileExists} "${USER_DATA_DIR}\limits-snapshot.json"
    DetailPrint "leftover: ${USER_DATA_DIR}\limits-snapshot.json"
    StrCpy $residue "1"
  ${EndIf}
  ${If} ${FileExists} "${USER_DATA_DIR}\data\*.*"
    DetailPrint "leftover: ${USER_DATA_DIR}\data"
    StrCpy $residue "1"
  ${EndIf}
  ${If} ${FileExists} "$LOCALAPPDATA\Javis\Token Monitor\*.*"
    DetailPrint "leftover: $LOCALAPPDATA\Javis\Token Monitor"
    StrCpy $residue "1"
  ${EndIf}
  ${If} ${FileExists} "$SMPROGRAMS\${SHORTCUT_NAME}"
    DetailPrint "leftover: $SMPROGRAMS\${SHORTCUT_NAME}"
    StrCpy $residue "1"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\${APP_EXE}"
    DetailPrint "leftover: $INSTDIR\${APP_EXE}"
    StrCpy $residue "1"
  ${EndIf}
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${RUN_VALUE}"
  ${If} $0 != ""
    DetailPrint "leftover: HKCU Run ${RUN_VALUE}"
    StrCpy $residue "1"
  ${EndIf}
  ReadRegStr $0 HKCU "${UNINST_KEY}" "DisplayName"
  ${If} $0 != ""
    DetailPrint "leftover: HKCU ${UNINST_KEY}"
    StrCpy $residue "1"
  ${EndIf}

  ${If} $residue == "1"
    ${IfNot} ${Silent}
      MessageBox MB_OK|MB_ICONEXCLAMATION "The uninstaller could not remove everything it installed. The details pane lists what is left; please report it."
    ${EndIf}
  ${Else}
    DetailPrint "Token Monitor (Qt) removed: no known residue."
  ${EndIf}
SectionEnd
