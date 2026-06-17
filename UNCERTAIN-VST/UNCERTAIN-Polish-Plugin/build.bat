@echo off
REM ============================================================================
REM  UNCERTAIN Polish - build complet en un double-clic
REM  Configure (telecharge JUCE) -> compile Release -> genere le setup.exe
REM ============================================================================

net session >nul 2>nul
if %errorlevel% neq 0 (
    echo Demande des droits administrateur...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================================
echo   UNCERTAIN Polish - BUILD
echo ============================================================
echo.

set "CMAKE="
where cmake >nul 2>nul && set "CMAKE=cmake"
if not defined CMAKE (
    for %%P in (
        "C:\Program Files\CMake\bin\cmake.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    ) do if exist %%P set "CMAKE=%%~P"
)
if not defined CMAKE (
    echo [ERREUR] CMake introuvable. Installe Visual Studio 2022 ^("Desktop C++"^) ou CMake.
    goto :end
)
echo CMake : "!CMAKE!"
echo.

echo [1/3] Configuration...
"!CMAKE!" -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 ( echo. & echo [ERREUR] Configuration echouee. & goto :end )
echo.

echo [2/3] Compilation Release...
"!CMAKE!" --build build --config Release
if errorlevel 1 ( echo. & echo [ERREUR] Compilation echouee. & goto :end )
echo.

echo [3/3] Generation de l'installeur...
set "ISCC="
where iscc >nul 2>nul && set "ISCC=iscc"
if not defined ISCC (
    for %%P in (
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    ) do if exist %%P set "ISCC=%%~P"
)
if not defined ISCC (
    echo [INFO] Inno Setup introuvable : installeur non genere.
    echo        Le VST3 est deja installe par le build.
    goto :done
)
"!ISCC!" installer.iss
if errorlevel 1 ( echo. & echo [ERREUR] Inno Setup a echoue. & goto :end )
echo.
echo Installeur cree : Installer\UNCERTAIN-Polish-Setup.exe

:done
echo.
echo ============================================================
echo   TERMINE
echo   - VST3      : C:\Program Files\Common Files\VST3
echo   - Standalone: build\UNCERTAINPolish_artefacts\Release\Standalone
echo   - Installeur: Installer\UNCERTAIN-Polish-Setup.exe
echo ============================================================

:end
echo.
pause
endlocal
