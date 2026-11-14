@echo off
setlocal
cd /d "%~dp0"

set "EXE=build\Release\Strategizer2D.exe"
if not exist "%EXE%" set "EXE=build\Strategizer2D.exe"

if not exist "%EXE%" (
    echo Spiel ist noch nicht gebaut - starte build.bat ...
    call build.bat nopause
    set "EXE=build\Release\Strategizer2D.exe"
    if not exist "%EXE%" set "EXE=build\Strategizer2D.exe"
)

if not exist "%EXE%" (
    echo [FEHLER] Konnte das Spiel nicht finden/bauen.
    pause
    exit /b 1
)

"%EXE%"
if errorlevel 1 pause
endlocal
