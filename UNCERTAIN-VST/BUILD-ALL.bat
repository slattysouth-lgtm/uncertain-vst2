@echo off
net session >nul 2>nul
if %errorlevel% neq 0 (
    echo Demande des droits administrateur...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
setlocal enabledelayedexpansion
cd /d "%~dp0"
echo ============================================================
echo   UNCERTAIN VST SUITE - BUILD (3 plugins)
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
if not defined CMAKE ( echo [ERREUR] CMake/Visual Studio 2022 introuvable. & goto :end )
echo CMake : "!CMAKE!"
echo.

call :build "BLOODMONEY-Master-Plugin" "BLOODMONEY Master"  || goto :end
call :build "UNCERTAIN-Polish-Plugin"  "UNCERTAIN Polish"   || goto :end
call :build "UNCERTAIN-Spatial-Plugin" "UNCERTAIN Spatial"  || goto :end

echo [INSTALLEUR] Generation de l'installeur unique...
set "ISCC="
where iscc >nul 2>nul && set "ISCC=iscc"
if not defined ISCC (
    for %%P in ("C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "C:\Program Files\Inno Setup 6\ISCC.exe") do if exist %%P set "ISCC=%%~P"
)
if not defined ISCC ( echo [INFO] Inno Setup absent : les 3 VST sont installes, mais pas d'installeur unique. & goto :end )
"!ISCC!" installer-all.iss
if errorlevel 1 ( echo [ERREUR] Inno Setup. & goto :end )
echo.
echo ============================================================
echo   PRET A PARTAGER : Installer\UNCERTAIN-VST-Suite-Setup.exe
echo   ^(installe les 3 plugins en un clic, sur tout PC Windows^)
echo ============================================================
goto :end

:build
echo [BUILD] %~2 ...
"!CMAKE!" -S "%~1" -B "%~1\build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 ( echo [ERREUR] config %~2. & exit /b 1 )
"!CMAKE!" --build "%~1\build" --config Release
if errorlevel 1 ( echo [ERREUR] build %~2. & exit /b 1 )
echo.
exit /b 0

:end
echo.
pause
endlocal
