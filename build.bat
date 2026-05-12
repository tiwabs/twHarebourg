@echo off
setlocal enabledelayedexpansion

echo.

:: Default values
set "BUILD_TYPE=Release"
set "POST_BUILD_RUN=true"

set "SCRIPT_DIR=%~dp0"

:: Check required tools
where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake not found. Please install CMake or add it to PATH.
    exit /b 9009
)

where g++ >nul 2>nul
if errorlevel 1 (
    echo Error: g++ not found. Please install MinGW or add it to PATH.
    exit /b 9009
)

where mingw32-make >nul 2>nul
if errorlevel 1 (
    echo Error: mingw32-make not found. Please install MinGW or add it to PATH.
    exit /b 9009
)

:: Détecter le préfixe MinGW (ex: C:\msys64\mingw64) depuis le chemin de g++
for /f "delims=" %%i in ('where g++') do (
    set "GPP_PATH=%%i"
    goto :found_gpp
)
:found_gpp
for %%i in ("%GPP_PATH%") do set "MINGW_BIN=%%~dpi"
set "MINGW_PREFIX=%MINGW_BIN%..\."
for %%i in ("%MINGW_PREFIX%") do set "MINGW_PREFIX=%%~fi"
echo MinGW prefix: %MINGW_PREFIX%

:: Create build folder and enter it
if not exist build mkdir build
cd build

:: Configure CMake
echo Configuring CMake...
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%MINGW_PREFIX%" ..
if errorlevel 1 (
    echo Error: CMake configuration failed.
    exit /b 1
)

:: Build
echo Compiling...

cmake --build . --config %BUILD_TYPE%

if errorlevel 1 (
    echo Error: Build failed.
    exit /b 1
)

echo Build successful!


:: Run target if requested (from project root so config.json resolves)
if "%POST_BUILD_RUN%"=="true" (
    echo Launching twHarebourg.exe
    ".\twHarebourg.exe"
)

cd ..
