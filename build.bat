@echo off
setlocal
rem ============================================================
rem  Strategizer2D - one-time setup / build (Windows)
rem  Needs: CMake (https://cmake.org/download) and a C++ compiler
rem         (Visual Studio Build Tools or MinGW-w64 / g++).
rem  SFML is downloaded automatically on the first run (needs git + internet).
rem ============================================================
cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 (
    echo [FEHLER] CMake wurde nicht gefunden. Bitte installieren: https://cmake.org/download
    echo          ^(Haken bei "Add CMake to PATH" setzen^)
    pause
    exit /b 1
)
where git >nul 2>&1
if errorlevel 1 (
    echo [FEHLER] Git wurde nicht gefunden ^(wird fuer den SFML-Download gebraucht^): https://git-scm.com
    pause
    exit /b 1
)

echo === Konfiguriere ===
cmake -S . -B build -DSTRATEGIZER_BUILD_TESTS=OFF
if errorlevel 1 (
    echo [FEHLER] Konfiguration fehlgeschlagen. Ist ein C++ Compiler installiert?
    echo          Visual Studio Build Tools: https://visualstudio.microsoft.com/downloads/
    echo          oder MinGW-w64 ^(g++^) im PATH.
    pause
    exit /b 1
)

echo === Baue ===
cmake --build build --config Release
if errorlevel 1 (
    echo [FEHLER] Build fehlgeschlagen.
    pause
    exit /b 1
)

echo.
echo Fertig! Starte das Spiel jetzt mit play.bat
if /i not "%~1"=="nopause" pause
endlocal
