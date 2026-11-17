REM Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
@echo off
setlocal
pushd "%~dp0..\.."
set "VSCMD_SKIP_SENDTELEMETRY=1"
set "K12_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%K12_VSWHERE%" (
  echo Visual Studio C++ tools were not found. See BUILDING.md.
  popd
  exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%K12_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "K12_VSROOT=%%i"
if not defined K12_VSROOT (
  echo Visual Studio C++ tools were not found. See BUILDING.md.
  popd
  exit /b 1
)
call "%K12_VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
  popd
  exit /b 1
)
cmake --build --preset k1-host-pc --parallel 8
set "K12_BUILD_RESULT=%errorlevel%"
popd
exit /b %K12_BUILD_RESULT%
