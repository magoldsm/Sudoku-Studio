; Sudoku Studio Installer
; NSIS Script

!include "MUI2.nsh"

; Name and file
Name "Sudoku Studio"
OutFile "SudokuStudio-Installer.exe"
InstallDir "$PROGRAMFILES\Sudoku Studio"
InstallDirRegKey HKLM "Software\Sudoku Studio" "Install_Dir"

; Request application privileges
RequestExecutionLevel admin

; MUI Settings
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; Installer sections
Section "Install"
  SetOutPath "$INSTDIR"

  ; Copy executable
  File "build\sudoku_ui.exe"

  ; Copy help files
  SetOutPath "$INSTDIR\docs\help"
  File /r "docs\help\*.*"

  ; Create start menu shortcuts
  CreateDirectory "$SMPROGRAMS\Sudoku Studio"
  CreateShortcut "$SMPROGRAMS\Sudoku Studio\Sudoku Studio.lnk" "$INSTDIR\sudoku_ui.exe"
  CreateShortcut "$SMPROGRAMS\Sudoku Studio\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  ; Create desktop shortcut
  CreateShortcut "$DESKTOP\Sudoku Studio.lnk" "$INSTDIR\sudoku_ui.exe"

  ; Write uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Registry entries for uninstall
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sudoku Studio" "DisplayName" "Sudoku Studio"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sudoku Studio" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sudoku Studio" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sudoku Studio" "DisplayVersion" "1.0"
SectionEnd

; Uninstaller section
Section "Uninstall"
  ; Remove shortcuts
  Delete "$SMPROGRAMS\Sudoku Studio\Sudoku Studio.lnk"
  Delete "$SMPROGRAMS\Sudoku Studio\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Sudoku Studio"
  Delete "$DESKTOP\Sudoku Studio.lnk"

  ; Remove files
  Delete "$INSTDIR\sudoku_ui.exe"
  RMDir /r "$INSTDIR\docs"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  ; Remove registry entries
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sudoku Studio"
  DeleteRegKey HKLM "Software\Sudoku Studio"
SectionEnd
