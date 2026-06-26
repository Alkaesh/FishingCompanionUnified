@echo off
setlocal
pushd "%~dp0" || exit /b 1

where cl >nul 2>nul
if errorlevel 1 (
  echo cl.exe was not found. Run this from a Visual Studio x64 Developer Command Prompt.
  popd
  exit /b 1
)

if defined VSCMD_ARG_TGT_ARCH if /I not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
  echo Warning: target architecture is %VSCMD_ARG_TGT_ARCH%, expected x64.
)

if not exist ..\build mkdir ..\build

cl /nologo /std:c++20 /EHsc /LD /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX ^
  action_module.cpp ^
  /Fo:..\build\ ^
  /Fe..\build\il2cpp_action_module.dll ^
  user32.lib gdi32.lib shell32.lib comctl32.lib

set "EXIT_CODE=%ERRORLEVEL%"
popd
endlocal & exit /b %EXIT_CODE%
