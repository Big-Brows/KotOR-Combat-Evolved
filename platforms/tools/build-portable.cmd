REM Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
REM Updated for KotOR:CE v0.2 on 2026-11-24; see docs/LOCAL_CHANGES.md.
REM Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
@echo off
setlocal
pushd "%~dp0..\.."
set "VSCMD_SKIP_SENDTELEMETRY=1"
set "K12_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%K12_VSWHERE%" goto :missing
for /f "usebackq tokens=*" %%i in (`"%K12_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "K12_VSROOT=%%i"
if not defined K12_VSROOT goto :missing
call "%K12_VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto :failed
cmake -S platforms/launcher -B platforms/out/launcher-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_CXX_FLAGS_DEBUG="/Od /Ob0 /RTC1" -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=
if errorlevel 1 goto :failed
cmake --build platforms/out/launcher-build
if errorlevel 1 goto :failed
if /i "%~1"=="--test-discovery" goto :test_discovery
if /i "%~1"=="--build-lazy-check" goto :build_lazy_check
rem Use cached sources only. No downloads, installs or shared dependency builds.
set "K12_DEPS=%CD%/platforms/out/k1-host-pc/_deps"
for /f "tokens=1,* delims==" %%a in ('findstr /b "Python3_EXECUTABLE:" platforms\out\k1-host-pc\CMakeCache.txt') do set "K12_PYTHON=%%b"
if not exist "%K12_PYTHON%" goto :failed
cmake --preset k1-host-pc -B platforms/out/k1-private-pc -DPython3_EXECUTABLE="%K12_PYTHON%" -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT= -DK2P_PRIVATE_PATHS=ON "-DK2P_PRODUCT_NAME=KotOR:CE" -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_K2SDL2="%K12_DEPS%/k2sdl2-src" -DFETCHCONTENT_SOURCE_DIR_K2FREETYPE="%K12_DEPS%/k2freetype-src" -DFETCHCONTENT_SOURCE_DIR_K2ZLIB="%K12_DEPS%/k2zlib-src"
if errorlevel 1 goto :failed
cmake --build platforms/out/k1-private-pc --parallel 8
if errorlevel 1 goto :failed
popd
exit /b 0
:test_discovery
cmake --build platforms/out/launcher-build --target discovery-tests
if errorlevel 1 goto :failed
platforms\out\launcher-build\discovery-tests.exe "platforms/out/launcher-audit/discovery-%RANDOM%"
if errorlevel 1 goto :failed
popd
exit /b 0
:build_lazy_check
cmake --build platforms/out/launcher-build --target lazy-model-check
if errorlevel 1 goto :failed
popd
exit /b 0
:missing
echo Installed Visual Studio C++ tools are required. See BUILDING.md.
:failed
popd
exit /b 1
