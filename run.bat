@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "MOD_DIR=%CD%"
set "BUILD_DIR=%MOD_DIR%\build"
if not defined GAME_DIR set "GAME_DIR=%MOD_DIR%\..\cube_game"
if not defined GAME_EXE set "GAME_EXE=Cube.exe"
set "DLL_NAME=cube_mod.dll"
set "LOG_FILE=%BUILD_DIR%\cube_mod.log"
set "INJECT_RETRIES=30"
for %%F in ("%GAME_EXE%") do set "GAME_PROC=%%~nF"

call "%MOD_DIR%\build.bat"
if errorlevel 1 (
    echo run: build failed
    exit /b 1
)

if not exist "%GAME_DIR%\%GAME_EXE%" (
    echo run: game not found at %GAME_DIR%\%GAME_EXE% ^(set GAME_DIR/GAME_EXE^)
    exit /b 1
)
if not exist "%BUILD_DIR%\%DLL_NAME%" (
    echo run: %DLL_NAME% missing after build
    exit /b 1
)
if not exist "%BUILD_DIR%\inject.exe" (
    echo run: inject.exe missing after build
    exit /b 1
)

if exist "%LOG_FILE%" del /q "%LOG_FILE%"

echo run: launching %GAME_EXE%
start "" /d "%GAME_DIR%" "%GAME_EXE%"

echo run: waiting for %GAME_EXE% process
set /a tries=0
:waitproc
tasklist /FI "IMAGENAME eq %GAME_EXE%" 2>nul | find /I "%GAME_EXE%" >nul
if not errorlevel 1 goto procfound
set /a tries+=1
if !tries! geq %INJECT_RETRIES% (
    echo run: %GAME_EXE% process never appeared
    exit /b 1
)
timeout /t 1 /nobreak >nul
goto waitproc
:procfound

echo run: injecting %DLL_NAME%
set /a tries=0
:injectloop
pushd "%BUILD_DIR%"
inject.exe "%GAME_EXE%" "%DLL_NAME%"
set "INJ=!errorlevel!"
popd
if "!INJ!"=="0" goto injected
set /a tries+=1
if !tries! geq %INJECT_RETRIES% (
    echo run: injection failed
    exit /b 1
)
timeout /t 1 /nobreak >nul
goto injectloop
:injected

echo run: injected. tailing %LOG_FILE% ^(Ctrl-C to stop; auto-stops when the game exits^)

set "CUBE_RUN_LOG=%LOG_FILE%"
set "CUBE_RUN_PROC=%GAME_PROC%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$log = $env:CUBE_RUN_LOG; $name = $env:CUBE_RUN_PROC;" ^
  "while (-not (Test-Path $log)) { if (-not (Get-Process -Name $name -ErrorAction SilentlyContinue)) { exit }; Start-Sleep -Milliseconds 200 }" ^
  "$fs = [System.IO.File]::Open($log, 'Open', 'Read', 'ReadWrite'); $sr = New-Object System.IO.StreamReader($fs);" ^
  "while ($true) { $line = $sr.ReadLine(); if ($null -ne $line) { Write-Output $line } else { if (-not (Get-Process -Name $name -ErrorAction SilentlyContinue)) { break }; Start-Sleep -Milliseconds 300 } }" ^
  "$sr.Close(); $fs.Close()"

echo run: %GAME_EXE% exited, stopping
endlocal
