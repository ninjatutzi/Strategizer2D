@echo off
setlocal
cd /d "%~dp0"

set "EXE=build\Release\Strategizer2D.exe"
if not exist "%EXE%" set "EXE=build\Strategizer2D.exe"

rem Always rebuild first (fast when nothing changed) so you never play an outdated version
echo Pruefe auf Aenderungen ...
call build.bat nopause
if errorlevel 1 exit /b 1
set "EXE=build\Release\Strategizer2D.exe"
if not exist "%EXE%" set "EXE=build\Strategizer2D.exe"

if not exist "%EXE%" (
    echo [FEHLER] Konnte das Spiel nicht finden/bauen.
    pause
    exit /b 1
)

"%EXE%"
if errorlevel 1 pause
endlocal
