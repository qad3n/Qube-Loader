@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "BUILD_DIR=%CD%\build"
if not defined GAME_DIR set "GAME_DIR=%CD%\..\cube_game"
if not defined GAME_EXE set "GAME_EXE=Cube.exe"
set "DLL_NAME=cube_mod.dll"
set "RETRIES=30"

call "%CD%\build.bat"
if errorlevel 1 exit /b 1

for %%F in ("%GAME_DIR%\%GAME_EXE%" "%BUILD_DIR%\%DLL_NAME%" "%BUILD_DIR%\inject.exe") do if not exist "%%~F" (
    echo run: missing %%~F
    exit /b 1
)

echo run: launching %GAME_EXE%
start "" /d "%GAME_DIR%" "%GAME_EXE%"

set /a tries=0
:waitproc
tasklist /FI "IMAGENAME eq %GAME_EXE%" 2>nul | find /I "%GAME_EXE%" >nul && goto inject
set /a tries+=1
if !tries! geq %RETRIES% (
    echo run: %GAME_EXE% never appeared
    exit /b 1
)
timeout /t 1 /nobreak >nul
goto waitproc

:inject
echo run: injecting %DLL_NAME%
set /a tries=0
:injectloop
pushd "%BUILD_DIR%"
inject.exe "%GAME_EXE%" "%DLL_NAME%"
set "INJ=!errorlevel!"
popd
if "!INJ!"=="0" goto done
set /a tries+=1
if !tries! geq %RETRIES% (
    echo run: injection failed
    exit /b 1
)
timeout /t 1 /nobreak >nul
goto injectloop

:done
endlocal
