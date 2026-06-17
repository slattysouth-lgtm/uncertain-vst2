@echo off
REM ============================================================================
REM  UNCERTAIN VST - tout automatique :
REM  installe les prerequis (via winget) PUIS compile et fabrique l'installeur.
REM  Tu ne fais qu'un double-clic. (Visual Studio est volumineux : sois patient.)
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
echo   UNCERTAIN VST - INSTALLATION AUTOMATIQUE DES OUTILS
echo ============================================================
echo.

where winget >nul 2>nul
if errorlevel 1 (
    echo [ERREUR] winget introuvable.
    echo Mets a jour "App Installer" depuis le Microsoft Store, puis relance.
    echo Sinon, installe manuellement : Visual Studio 2022 (Desktop C++),
    echo Git et Inno Setup, puis lance BUILD-ALL.bat.
    goto :end
)

echo Note : Windows installe les paquets l'un apres l'autre (c'est normal).
echo.

echo [1/4] Git...
winget install -e --id Git.Git --accept-package-agreements --accept-source-agreements --silent

echo [2/4] CMake...
winget install -e --id Kitware.CMake --accept-package-agreements --accept-source-agreements --silent

echo [3/4] Inno Setup...
winget install -e --id JRSoftware.InnoSetup --accept-package-agreements --accept-source-agreements --silent

echo [4/4] Visual Studio 2022 Build Tools + outils C++ (LONG, plusieurs Go)...
winget install -e --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements ^
  --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

REM rafraichir le PATH pour cette session (les installeurs ne le propagent pas a chaud)
set "PATH=%PATH%;C:\Program Files\Git\cmd;C:\Program Files\CMake\bin"

echo.
echo ============================================================
echo   Outils installes. Lancement de la compilation...
echo ============================================================
echo.
call BUILD-ALL.bat
goto :eof

:end
echo.
pause
endlocal
