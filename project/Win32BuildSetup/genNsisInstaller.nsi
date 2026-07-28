;Application for Windows install script
;Copyright (C) 2005-2013 Team XBMC
;http://xbmc.org

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"
  !include "nsDialogs.nsh"
  !include "LogicLib.nsh"
  !include "WinVer.nsh"
  !include "x64.nsh"

;--------------------------------
;General

  ;Name and file
  Name "${APP_NAME}"
  OutFile "${APP_NAME}Setup-${app_revision}-${app_branch}-${TARGET_ARCHITECTURE}.exe"

  ;Default installation folder
!ifdef x64
  InstallDir "$PROGRAMFILES64\${APP_NAME}"
!else
  InstallDir "$PROGRAMFILES\${APP_NAME}"
!endif

  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\${APP_NAME}" ""

  ;Request application privileges for Windows Vista
  RequestExecutionLevel admin

  InstProgressFlags smooth

  ; Installer file properties
  VIProductVersion                   ${VERSION_NUMBER}
  VIAddVersionKey "ProductName"      "${APP_NAME}"
  VIAddVersionKey "Comments"         "This application and its source code are freely distributable."
  VIAddVersionKey "LegalCopyright"   "The trademark is owned by ${COMPANY_NAME}"
  VIAddVersionKey "CompanyName"      "${COMPANY_NAME}"
  VIAddVersionKey "FileDescription"  "${APP_NAME} ${VERSION_NUMBER} Setup"
  VIAddVersionKey "FileVersion"      "${VERSION_NUMBER}"
  VIAddVersionKey "ProductVersion"   "${VERSION_NUMBER}"
  VIAddVersionKey "LegalTrademarks"  "${APP_NAME}"
  ;VIAddVersionKey "OriginalFilename" "${APP_NAME}Setup-${app_revision}-${app_branch}.exe"

;--------------------------------
;Variables

  Var StartMenuFolder
  Var PageProfileState
  Var VSRedistSetupError
  Var /GLOBAL CleanDestDir

;--------------------------------
;Interface Settings

  !define MUI_HEADERIMAGE
  !define MUI_ICON "..\..\tools\windows\packaging\media\application.ico"
  !define MUI_UNICON "..\..\tools\windows\packaging\media\application.ico"
  !define MUI_HEADERIMAGE_BITMAP "..\..\tools\windows\packaging\media\installer\header.bmp"
  !define MUI_HEADERIMAGE_UNBITMAP "..\..\tools\windows\packaging\media\installer\header.bmp"
  !define MUI_WELCOMEFINISHPAGE_BITMAP "..\..\tools\windows\packaging\media\installer\welcome-left.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "..\..\tools\windows\packaging\media\installer\welcome-left.bmp"
  !define MUI_COMPONENTSPAGE_SMALLDESC
  !define MUI_FINISHPAGE_LINK "Please visit ${WEBSITE} for more information."
  !define MUI_FINISHPAGE_LINK_LOCATION "${WEBSITE}"
  !define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_NAME}.exe"
  !define MUI_FINISHPAGE_RUN_NOTCHECKED
  !define MUI_ABORTWARNING
;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "..\..\LICENSE.md"
  !insertmacro MUI_PAGE_COMPONENTS
  !define MUI_PAGE_CUSTOMFUNCTION_LEAVE CallbackDirLeave
  !insertmacro MUI_PAGE_DIRECTORY

  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\${APP_NAME}"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  UninstPage custom un.UnPageProfile un.UnPageProfileLeave
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;HelperFunction

Function CallbackDirLeave
  ;deinstall kodi if it is already there in destination folder
  Call HandleKodiInDestDir
FunctionEnd

Function HandleOldKodiInstallation
  Var /GLOBAL INSTDIR_KODI
  ReadRegStr $INSTDIR_KODI HKCU "Software\${APP_NAME}" ""

  ;if former Kodi installation was detected in a different directory then the destination dir
  ;ask for uninstallation
  ;only ask about the other installation if user didn't already
  ;decide to not overwrite the installation in his originally selected destination dir
  ${IfNot}    $CleanDestDir == "0"
  ${AndIfNot} $INSTDIR_KODI == ""
  ${AndIfNot} $INSTDIR_KODI == $INSTDIR
    MessageBox MB_YESNO|MB_ICONQUESTION  "A previous ${APP_NAME} installation in a different folder was detected. Would you like to uninstall it?$\nYour current settings and library data will be kept intact." IDYES true IDNO false
    true:
      DetailPrint "Uninstalling $INSTDIR_KODI"
      SetDetailsPrint none
      ExecWait '"$INSTDIR_KODI\uninstall.exe" /S _?=$INSTDIR_KODI'
      SetDetailsPrint both
      ;this also removes the uninstall.exe which doesn't remove it self...
      Delete "$INSTDIR_KODI\uninstall.exe"
      ;if the directory is now empty we can safely remove it (rmdir won't remove non-empty dirs!)
      RmDir "$INSTDIR_KODI"
    false:
  ${EndIf}
FunctionEnd

Function HandleKodiInDestDir
  ;if former Kodi installation was detected in the destination directory - uninstall it first
  ${IfNot} $INSTDIR == ""
  ${AndIf} ${FileExists} "$INSTDIR\uninstall.exe"
    MessageBox MB_YESNO|MB_ICONQUESTION  "A previous installation was detected in the selected destination folder. Do you really want to overwrite it?$\nYour settings and library data will be kept intact." IDYES true IDNO false
    true:
      StrCpy $CleanDestDir "1"
      Goto done
    false:
      StrCpy $CleanDestDir "0"
      Abort
    done:
	MessageBox MB_OK|MB_ICONINFORMATION "All binary add-ons (e.g. pvr, visualizations, inputstream, etc) that were previously included by default in the installer have been moved to the Kodi repository. You will have to install the ones you previously used from the repository.$\nYour add-on settings are kept intact and will be used again after installing the add-on."
  ${EndIf}
FunctionEnd

Function DeinstallKodiInDestDir
  ${If} $CleanDestDir == "1"
    DetailPrint "Uninstalling former ${APP_NAME} Installation in $INSTDIR"
    SetDetailsPrint none
    ExecWait '"$INSTDIR\uninstall.exe" /S _?=$INSTDIR'
    SetDetailsPrint both
    ;this also removes the uninstall.exe which doesn't remove it self...
    Delete "$INSTDIR\uninstall.exe"
  ${EndIf}
FunctionEnd

;--------------------------------
;Install levels

InstType "Full"    ; 1.
InstType "Normal"  ; 2.
InstType "Minimal" ; 3.

;--------------------------------
;Installer Sections

Section "${APP_NAME}" SecAPP
  SetShellVarContext all
  SectionIn RO
  SectionIn 1 2 3 #section is in install type Normal/Full/Minimal

  ;deinstall kodi in destination dir if $CleanDestDir == "1" - meaning user has confirmed it
  Call DeinstallKodiInDestDir

  ;Start copying files
  SetOutPath "$INSTDIR"
  File "${app_root}\application\*.*"
  SetOutPath "$INSTDIR\addons"
  File /r "${app_root}\application\addons\*.*"
  File /nonfatal /r "${app_root}\addons\peripheral.*"
  SetOutPath "$INSTDIR\media"
  File /r "${app_root}\application\media\*.*"
  SetOutPath "$INSTDIR\system"
  File /r "${app_root}\application\system\*.*"
  SetOutPath "$INSTDIR\userdata"
  File /r "${app_root}\application\userdata\*.*"

  ;Everything from here to the matching "SetShellVarContext all" below is about the *user's*
  ;profile, so the context has to be switched. This installer sets "SetShellVarContext all"
  ;further up, which makes $APPDATA resolve to C:\ProgramData rather than the person's
  ;roaming folder. Both of the steps below silently did the wrong thing because of it: the
  ;keymap was written to C:\ProgramData\<app>\userdata\keymaps where nothing reads it, and
  ;the profile copy looked for C:\ProgramData\Kodi, did not find it, and skipped. Neither
  ;failed loudly -- the install simply came up empty and silent.
  SetShellVarContext current

  ;Bring an existing Kodi profile across on a first install.
  ;
  ;This build is a rebranded Kodi, and APP_NAME also names the profile folder under $APPDATA.
  ;So without this, installing it "over" Kodi would start with nothing: no library, no
  ;sources, no settings, no addons. That is a very unpleasant surprise for someone who set
  ;all of it up by ear.
  ;
  ;Copied, never moved, so the original Kodi profile is left completely intact and remains a
  ;working fallback if this build misbehaves.
  ;
  ;Three guards, and all three matter:
  ;  - skipped when APP_NAME is "Kodi", where source and target are the same folder;
  ;  - skipped when our profile already exists, so an upgrade never overwrites the settings
  ;    someone has since changed;
  ;  - skipped when there is no Kodi profile to copy, which is the ordinary case for someone
  ;    who has never run Kodi before -- they get a clean first run, not an error.
  ;
  ;Only userdata and addons are copied. temp and thumbnail caches rebuild themselves, and a
  ;profile can be large enough that copying the lot would look like the installer hanging.
  StrCmp "${APP_NAME}" "Kodi" skip_profile_migration 0
  IfFileExists "$APPDATA\${APP_NAME}\userdata\*.*" skip_profile_migration 0
  IfFileExists "$APPDATA\Kodi\userdata\*.*" 0 skip_profile_migration
    DetailPrint "Copying your existing Kodi settings..."
    CreateDirectory "$APPDATA\${APP_NAME}\userdata"
    CopyFiles "$APPDATA\Kodi\userdata\*.*" "$APPDATA\${APP_NAME}\userdata"
    IfFileExists "$APPDATA\Kodi\addons\*.*" 0 +3
      CreateDirectory "$APPDATA\${APP_NAME}\addons"
      CopyFiles "$APPDATA\Kodi\addons\*.*" "$APPDATA\${APP_NAME}\addons"
  skip_profile_migration:

  ;Screen reader keymap.
  ;
  ;It has to go to the *profile* rather than $INSTDIR\userdata, because the copy in the
  ;install folder is only ever read in portable mode. Without this file the reader's keys --
  ;repeat, window, item detail, stop, settings, on/off -- do nothing at all, which looks like
  ;the reader being broken rather than a missing keymap, and is very hard to work out without
  ;being able to see the screen.
  ;
  ;Done here rather than by the addon because Kodi has no way to reload keymaps: one dropped
  ;in later does nothing until the next restart. Placing it before Kodi has ever run is what
  ;makes the keys work the first time. The addon installs it too, for anyone who installs the
  ;addon by hand from a zip.
  ;
  ;Never overwritten. If the file is already there it may have been edited, and silently
  ;replacing someone's edited keymap would be its own bug.
  ;
  ;Taken from the staged userdata rather than from the bundled addon, and that is not
  ;arbitrary. File is a *compile-time* command: makensis has to find it while building the
  ;installer. BuildSetup.bat stages and packages in one go, so the addon is not in the staged
  ;tree yet at that moment -- referencing it there fails the whole package with "Error in
  ;script ... on line N", which is a confusing way to learn about build ordering. The fork's
  ;own userdata is staged before packaging, so this path always exists.
  IfFileExists "$APPDATA\${APP_NAME}\userdata\keymaps\zzz.screenreader.xml" +4 0
    CreateDirectory "$APPDATA\${APP_NAME}\userdata\keymaps"
    SetOutPath "$APPDATA\${APP_NAME}\userdata\keymaps"
    File "${app_root}\application\userdata\keymaps\zzz.screenreader.xml"

  ;Back to the all-users context the rest of this section expects.
  SetShellVarContext all

  SetOutPath "$INSTDIR"

  ;Store installation folder
  WriteRegStr HKCU "Software\${APP_NAME}" "" $INSTDIR

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  ;Create shortcuts
  SetOutPath "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
  CreateShortCut "$SMPROGRAMS\$StartMenuFolder\${APP_NAME}.lnk" "$INSTDIR\${APP_NAME}.exe" \
    "" "$INSTDIR\${APP_NAME}.exe" 0 SW_SHOWNORMAL \
    "" "Start ${APP_NAME}."
  CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe" \
    "" "$INSTDIR\Uninstall.exe" 0 SW_SHOWNORMAL \
    "" "Uninstall ${APP_NAME}."

  WriteINIStr "$SMPROGRAMS\$StartMenuFolder\Visit ${APP_NAME} Online.url" "InternetShortcut" "URL" "${WEBSITE}"
  !insertmacro MUI_STARTMENU_WRITE_END

  ;add entry to add/remove programs
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "NoRepair" 1
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "DisplayIcon" "$INSTDIR\${APP_NAME}.exe,0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "Publisher" "${COMPANY_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "HelpLink" "${WEBSITE}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "URLInfoAbout" "${WEBSITE}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                 "DisplayVersion" "${VERSION_NUMBER}"

SectionEnd

;*-addons.nsi are generated by genNsisIncludes.bat
!include /nonfatal "audiodecoder-addons.nsi"
!include /nonfatal "audioencoder-addons.nsi"
!include /nonfatal "game-addons.nsi"
!include /nonfatal "imagedecoder-addons.nsi"
!include /nonfatal "inputstream-addons.nsi"
!include /nonfatal "pvr-addons.nsi"
!include /nonfatal "screensaver-addons.nsi"
!include /nonfatal "vfs-addons.nsi"
!include /nonfatal "visualization-addons.nsi"

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_SecAPP ${LANG_ENGLISH} "${APP_NAME} ${VERSION_NUMBER}"

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAPP} $(DESC_SecAPP)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Var UnPageProfileDialog
Var UnPageProfileCheckbox
Var UnPageProfileCheckbox_State
Var UnPageProfileEditBox

Function un.UnPageProfile
    !insertmacro MUI_HEADER_TEXT "Uninstall ${APP_NAME}" "Remove ${APP_NAME}'s profile folder from your computer."
  nsDialogs::Create /NOUNLOAD 1018
  Pop $UnPageProfileDialog

  ${If} $UnPageProfileDialog == error
    Abort
  ${EndIf}

  ${NSD_CreateLabel} 0 0 100% 12u "Do you want to delete the profile folder which contains your ${APP_NAME} settings and library data?"
  Pop $0

  ${NSD_CreateText} 0 13u 100% 12u "$APPDATA\${APP_NAME}\"
  Pop $UnPageProfileEditBox
    SendMessage $UnPageProfileEditBox ${EM_SETREADONLY} 1 0

  ${NSD_CreateLabel} 0 30u 100% 24u "Leave the option box below unchecked to keep the profile folder which contains ${APP_NAME}'s settings and library data for later use. If you are sure you want to delete the profile folder you may check the option box.$\nWARNING: Deletion of the profile folder cannot be undone and you will lose all settings and library data."
  Pop $0

  ${NSD_CreateCheckbox} 0 71u 100% 8u "Yes, I am sure and grant permission to also delete the profile folder."
  Pop $UnPageProfileCheckbox

  nsDialogs::Show
FunctionEnd

Function un.UnPageProfileLeave
${NSD_GetState} $UnPageProfileCheckbox $UnPageProfileCheckbox_State
FunctionEnd

Section "Uninstall"

  SetShellVarContext all

  ;ADD YOUR OWN FILES HERE...
  RMDir /r "$INSTDIR\addons"
  RMDir /r "$INSTDIR\language"
  RMDir /r "$INSTDIR\media"
  RMDir /r "$INSTDIR\system"
  RMDir /r "$INSTDIR\userdata"
  Delete "$INSTDIR\*.*"

  ;Un-install User Data if option is checked, otherwise skip
  ${If} $UnPageProfileCheckbox_State == ${BST_CHECKED}
    SetShellVarContext current
    RMDir /r "$APPDATA\${APP_NAME}\"
    SetShellVarContext all
    RMDir /r "$INSTDIR\portable_data\"
  ${EndIf}
  RMDir "$INSTDIR"

  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
  Delete "$SMPROGRAMS\$StartMenuFolder\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall ${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Visit ${APP_NAME} Online.url"
  RMDir "$SMPROGRAMS\$StartMenuFolder"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
  DeleteRegKey /ifempty HKCU "Software\${APP_NAME}"

SectionEnd

;--------------------------------
;vs redist installer Section
SectionGroup "Microsoft Visual C++ packages" SEC_VCREDIST

Section "Visual C++ 2015-2022 Redistributable Package (${TARGET_ARCHITECTURE})" SEC_VCREDIST1
DetailPrint "Running Visual C++ 2015-2022 Redistributable setup..."
  SectionIn 1 2 #section is in install type Full
  SetOutPath "$TEMP\vcredist"
  File "${app_root}\..\..\BuildDependencies\downloads\vcredist\2015-2022\vcredist_${TARGET_ARCHITECTURE}.exe"
  ExecWait '"$TEMP\vcredist\vcredist_${TARGET_ARCHITECTURE}.exe" /install /quiet /norestart' $VSRedistSetupError
  RMDir /r "$TEMP\vcredist"
  DetailPrint "Finished Visual C++ 2015-2022 Redistributable setup"
  SetOutPath "$INSTDIR"
SectionEnd

SectionGroupEnd

Function .onInit
  !ifdef x64
    SetRegView 64
    ${IfNot} ${RunningX64}
      MessageBox MB_OK|MB_ICONSTOP 'This is the 64-bit ${APP_NAME} installer.$\nPlease download the 32-bit version from ${WEBSITE}.$\n$\nClick Ok to quit Setup.'
      Quit
    ${Endif}
  !else
    ${If} ${RunningX64}
      MessageBox MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2 'There is a specific 64-bit ${APP_NAME} version available for download. Please consider installing the 64-bit version instead.$\nFor details visit ${WEBSITE}.$\nProceed with 32-bit installation anyway?' /SD IDYES IDYES noprob
      Quit
      noprob:
    ${Endif}
  !endif

  ; Windows 8.1 is minimum requirement
  ${IfNot} ${AtLeastWin8.1}
    MessageBox MB_OK|MB_ICONSTOP|MB_TOPMOST|MB_SETFOREGROUND "Windows 8.1 or above required."
    Quit
  ${EndIf}

  StrCpy $CleanDestDir "-1"
FunctionEnd
