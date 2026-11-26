@echo off
setlocal EnableExtensions
title TZAR // build
cd /d "%~dp0"
echo.
echo   ===========================================
echo    TZAR  //  THERMONUCLEAR SEQUENCE ENGINE
echo    Compilation du VST3 (Windows x64)
echo   ===========================================
echo.

rem --- 1. trouver CMake : PATH, sinon celui fourni avec Visual Studio
set "CMAKE=cmake"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cmake >nul 2>nul
if not errorlevel 1 goto :findgit
if not exist "%VSWHERE%" goto :findgit
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do set "CMAKE=%%i"

:findgit
rem --- 2. trouver Git : PATH, sinon celui fourni avec Visual Studio
where git >nul 2>nul
if not errorlevel 1 goto :check
if not exist "%VSWHERE%" goto :check
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -find Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\**\cmd\git.exe`) do set "GITDIR=%%~dpi"
if defined GITDIR set "PATH=%GITDIR%;%PATH%"

:check
"%CMAKE%" --version >nul 2>nul
if errorlevel 1 goto :nocmake
where git >nul 2>nul
if errorlevel 1 goto :nogit

echo [TZAR] Configuration... (le premier lancement telecharge JUCE, 1-2 min)
"%CMAKE%" -S . -B build -A x64
if errorlevel 1 goto :fail
echo [TZAR] Compilation Release...
"%CMAKE%" --build build --config Release --target TZAR_VST3 TZAR_Standalone --parallel
if errorlevel 1 goto :fail

set "OUT=%~dp0build\TZAR_artefacts\Release\VST3\TZAR.vst3"
echo.
echo [TZAR] OK  --  %OUT%
echo.
set "INSTALL=n"
set /p INSTALL="Installer dans %CommonProgramFiles%\VST3 ? (o/n) "
if /i not "%INSTALL%"=="o" goto :done
xcopy /E /I /Y "%OUT%" "%CommonProgramFiles%\VST3\TZAR.vst3" >nul
if errorlevel 1 goto :copyfail
echo [TZAR] Installe. Dans FL Studio : Options / Manage plugins / Find installed plugins.
goto :done

:copyfail
echo [TZAR] Copie refusee : relance ce script en administrateur
echo        (clic droit / Executer en tant qu'administrateur), ou copie le dossier
echo        TZAR.vst3 a la main dans %CommonProgramFiles%\VST3
goto :done

:nocmake
echo [TZAR] CMake introuvable.
echo        Installe "Visual Studio 2022 Build Tools" (gratuit) avec la charge de travail
echo        "Developpement Desktop en C++" : CMake est inclus. Puis relance ce script.
goto :fail

:nogit
echo [TZAR] Git introuvable : installe Git for Windows (https://git-scm.com), puis relance.
goto :fail

:done
echo.
pause
exit /b 0

:fail
echo.
echo [TZAR] Echec.
pause
exit /b 1
