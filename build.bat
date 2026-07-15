@echo off
rem Configure + build the 32-bit mod DLL and injector with Visual Studio / MSVC.
rem Run from a Developer Command Prompt, or just double-click if CMake + VS are on PATH.
setlocal
cd /d "%~dp0"

rem Vendored deps (MinHook, ImGui) are git submodules; check them out before configuring.
where git >nul 2>nul && git submodule update --init --recursive

set "BUILD_DIR=build"
if "%CUBE_MOD_BUILD_TYPE%"=="" set "CUBE_MOD_BUILD_TYPE=Release"

rem -A Win32 forces the 32-bit target (Cube.exe is a 32-bit PE). No mingw toolchain file here.
cmake -S . -B "%BUILD_DIR%" -A Win32 -DCMAKE_BUILD_TYPE=%CUBE_MOD_BUILD_TYPE%
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --config %CUBE_MOD_BUILD_TYPE%
if errorlevel 1 exit /b 1

echo.
echo Artifacts:
if exist "%BUILD_DIR%\cube_mod.dll" echo   %BUILD_DIR%\cube_mod.dll
if exist "%BUILD_DIR%\inject.exe" echo   %BUILD_DIR%\inject.exe
if exist "%BUILD_DIR%\mods\example_mod.dll" echo   %BUILD_DIR%\mods\example_mod.dll
endlocal
