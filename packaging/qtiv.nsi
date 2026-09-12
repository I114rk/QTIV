; Установщик QTIV для Windows (NSIS).
; Сборка (из каталога packaging/, после windows-deploy.sh):
;   makensis -V3 -DDEPLOY_DIR=<путь к qtiv-win> -DVERSION=0.1.2 qtiv.nsi
; Результат: qtiv-<VERSION>-win64-setup.exe

!include "WordFunc.nsh"
!include "WinMessages.nsh"
!insertmacro WordReplace

!ifndef VERSION
  !define VERSION "0.1.2"
!endif
!ifndef DEPLOY_DIR
  !error "Задайте -DDEPLOY_DIR=<каталог с файлами программы>"
!endif

Name "QTIV ${VERSION}"
OutFile "qtiv-${VERSION}-win64-setup.exe"
InstallDir "$PROGRAMFILES64\QTIV"
InstallDirRegKey HKLM "Software\QTIV" "InstallDir"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma

Page license
Page components
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

LicenseData "${DEPLOY_DIR}\LICENSE.txt"

; Список расширений для пункта «Открыть с помощью» (берём со стека).
!macro PushExts
  Push ".qtivp"
  Push ".tif"
  Push ".gif"
  Push ".bmp"
  Push ".webp"
  Push ".png"
  Push ".jpeg"
  Push ".jpg"
!macroend

; ------------------------------------------------------------------ установка

Section "QTIV — просмотрщик (обязательно)" SEC_CORE
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${DEPLOY_DIR}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\QTIV"
  CreateShortCut "$SMPROGRAMS\QTIV\QTIV.lnk" "$INSTDIR\qtiv.exe"
  CreateShortCut "$SMPROGRAMS\QTIV\QTIV Help.lnk" "$INSTDIR\qtivh.exe"

  WriteRegStr HKLM "Software\QTIV" "InstallDir" "$INSTDIR"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV" "DisplayName" "QTIV ${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV" "DisplayIcon" '"$INSTDIR\qtiv.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV" "Publisher" "QTIV project"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV" "NoRepair" 1
SectionEnd

Section "Ярлык на рабочем столе" SEC_DESKTOP
  CreateShortCut "$DESKTOP\QTIV.lnk" "$INSTDIR\qtiv.exe"
SectionEnd

Section "qtivp и qtivh в PATH (консоль)" SEC_PATH
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
  WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$0;$INSTDIR"
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000
SectionEnd

Section "Изображения в «Открыть с помощью»" SEC_ASSOC
  WriteRegStr HKCR "QTIV.Image" "" "QTIV Image"
  WriteRegStr HKCR "QTIV.Image\DefaultIcon" "" '"$INSTDIR\qtiv.exe",0'
  WriteRegStr HKCR "QTIV.Image\shell\open\command" "" '"$INSTDIR\qtiv.exe" "%1"'
  !insertmacro PushExts
  assoc_loop:
    Pop $2
    StrCmp $2 "" assoc_done
    WriteRegStr HKCR "$2\OpenWithProgids" "QTIV.Image" ""
    Goto assoc_loop
  assoc_done:
SectionEnd

; ------------------------------------------------------------------ удаление

Section "Uninstall"
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
  ${WordReplace} "$0" ";$INSTDIR" "" "+" $1
  WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$1"
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000

  Delete "$SMPROGRAMS\QTIV\QTIV.lnk"
  Delete "$SMPROGRAMS\QTIV\QTIV Help.lnk"
  RMDir "$SMPROGRAMS\QTIV"
  Delete "$DESKTOP\QTIV.lnk"

  DeleteRegKey HKCR "QTIV.Image"
  !insertmacro PushExts
  unassoc_loop:
    Pop $2
    StrCmp $2 "" unassoc_done
    DeleteRegValue HKCR "$2\OpenWithProgids" "QTIV.Image"
    Goto unassoc_loop
  unassoc_done:

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QTIV"
  DeleteRegKey HKLM "Software\QTIV"

  Delete "$INSTDIR\uninstall.exe"
  RMDir /r "$INSTDIR"
SectionEnd
