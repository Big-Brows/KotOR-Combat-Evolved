REM Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
@echo off
setlocal
pushd "%~dp0..\.."
set "VSCMD_SKIP_SENDTELEMETRY=1"
set "CE_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%CE_VSWHERE%" goto :missing
for /f "usebackq tokens=*" %%i in (`"%CE_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CE_VSROOT=%%i"
if not defined CE_VSROOT goto :missing
call "%CE_VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto :failed
cmake -S platforms/launcher -B platforms/out/launcher-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_CXX_FLAGS_DEBUG="/Od /Ob0 /RTC1" -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=
if errorlevel 1 goto :failed
cmake --build platforms/out/launcher-build
if errorlevel 1 goto :failed
if /i "%~1"=="--test-discovery" goto :test_discovery
set "CE_PYTHON_ARG="
if defined KOTOR_CE_PYTHON set CE_PYTHON_ARG=-DPython3_EXECUTABLE="%KOTOR_CE_PYTHON%"
set "CE_CACHE_ARGS="
if defined KOTOR_CE_DEPENDENCY_CACHE set CE_CACHE_ARGS=-DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_K2SDL2="%KOTOR_CE_DEPENDENCY_CACHE%/k2sdl2-src" -DFETCHCONTENT_SOURCE_DIR_K2FREETYPE="%KOTOR_CE_DEPENDENCY_CACHE%/k2freetype-src" -DFETCHCONTENT_SOURCE_DIR_K2ZLIB="%KOTOR_CE_DEPENDENCY_CACHE%/k2zlib-src" -DFETCHCONTENT_SOURCE_DIR_K1MINIZ="%KOTOR_CE_DEPENDENCY_CACHE%/miniz-src" -DFETCHCONTENT_SOURCE_DIR_K1LZMA="%KOTOR_CE_DEPENDENCY_CACHE%/7zip-src"
cmake --preset k1-host-pc -B platforms/out/k1-private-pc %CE_PYTHON_ARG% %CE_CACHE_ARGS% -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT= -DK2P_PRIVATE_PATHS=ON "-DK2P_PRODUCT_NAME=KotOR:CE"
if errorlevel 1 goto :failed
cmake --build platforms/out/k1-private-pc --parallel 8
if errorlevel 1 goto :failed
popd
exit /b 0
:test_discovery
cmake --build platforms/out/launcher-build --target discovery-tests
if errorlevel 1 goto :failed
platforms\out\launcher-build\discovery-tests.exe "platforms/out/discovery-%RANDOM%"
if errorlevel 1 goto :failed
popd
exit /b 0
:missing
echo Installed Visual Studio C++ tools are required. See BUILDING.md.
:failed
popd
exit /b 1
