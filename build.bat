@echo off
REM Build script for MultipackParser C++
REM Supports native Windows build and Docker cross-compilation
REM
REM Usage: build.bat [options]
REM
REM Options:
REM   --native      Build for Windows (default)
REM   --arm64       Build for ARM64/Raspberry Pi using Docker
REM   --debug       Build in Debug mode
REM   --clean       Clean build directories
REM   --rebuild     Force rebuild (clean + build)
REM   --run         Run after building
REM   --help        Show this help message

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
set OUTPUT_DIR=%SCRIPT_DIR%output
set BUILD_TYPE=Release
set TARGET=native
set CLEAN=0
set RUN_AFTER=0
set JOBS=%NUMBER_OF_PROCESSORS%

REM Parse arguments
:parse_args
if "%1"=="" goto :check_target
if /i "%1"=="--native" set TARGET=native& shift& goto :parse_args
if /i "%1"=="--arm64" set TARGET=arm64& shift& goto :parse_args
if /i "%1"=="--debug" set BUILD_TYPE=Debug& shift& goto :parse_args
if /i "%1"=="--clean" set CLEAN=1& shift& goto :parse_args
if /i "%1"=="--rebuild" set CLEAN=1& shift& goto :parse_args
if /i "%1"=="--run" set RUN_AFTER=1& shift& goto :parse_args
if /i "%1"=="-h" goto :help
if /i "%1"=="--help" goto :help
if /i "%1"=="-j" set JOBS=%2& shift& shift& goto :parse_args
echo Unknown option: %1
goto :help

:help
echo.
echo ==========================================
echo MultipackParser C++ Build Script
echo ==========================================
echo.
echo Usage: build.bat [options]
echo.
echo Options:
echo   --native      Build for Windows using CMake (default)
echo   --arm64       Build for ARM64/Raspberry Pi using Docker
echo   --debug       Build in Debug mode (default: Release)
echo   --clean       Clean build directories before building
echo   --rebuild     Same as --clean
echo   --run         Run the application after building
echo   -j N          Number of parallel jobs (default: %NUMBER_OF_PROCESSORS%)
echo   --help        Show this help message
echo.
echo Examples:
echo   build.bat                     Build for Windows (Release)
echo   build.bat --debug             Build for Windows (Debug)
echo   build.bat --arm64             Build for Raspberry Pi
echo   build.bat --clean --run       Clean, build, and run
echo.
echo Requirements for native Windows build:
echo   - CMake 3.16+
echo   - Qt6 (installed or set Qt6_DIR)
echo   - Visual Studio 2019+ or MinGW
echo.
echo Requirements for ARM64 Docker build:
echo   - Docker Desktop with WSL2
echo.
goto :eof

:check_target
if "%TARGET%"=="arm64" goto :build_arm64
goto :build_native

:build_native
echo.
echo ==========================================
echo MultipackParser C++ Windows Build
echo ==========================================
echo Build type: %BUILD_TYPE%
echo Build dir:  %BUILD_DIR%
echo Jobs:       %JOBS%
echo ==========================================
echo.

REM Check for CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found in PATH
    echo.
    echo Please install CMake from https://cmake.org/download/
    echo Or install via: winget install Kitware.CMake
    exit /b 1
)

REM Clean if requested
if "%CLEAN%"=="1" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo.
)

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cd /d "%BUILD_DIR%"

REM Configure with CMake
echo Configuring with CMake...
echo.

REM Try to find Qt6
set "QT_CMAKE_ARGS="
if defined Qt6_DIR (
    set "QT_CMAKE_ARGS=-DCMAKE_PREFIX_PATH=%Qt6_DIR%"
    echo Using Qt6 from: %Qt6_DIR%
) else (
    REM Try common Qt installation paths
    if exist "C:\Qt\6.7.0\msvc2019_64\lib\cmake\Qt6" (
        set "QT_CMAKE_ARGS=-DCMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64"
        echo Found Qt6 at: C:\Qt\6.7.0\msvc2019_64
    ) else if exist "C:\Qt\6.6.0\msvc2019_64\lib\cmake\Qt6" (
        set "QT_CMAKE_ARGS=-DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64"
        echo Found Qt6 at: C:\Qt\6.6.0\msvc2019_64
    ) else if exist "C:\Qt\6.5.0\msvc2019_64\lib\cmake\Qt6" (
        set "QT_CMAKE_ARGS=-DCMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2019_64"
        echo Found Qt6 at: C:\Qt\6.5.0\msvc2019_64
    ) else (
        echo Warning: Qt6 not found. CMake will try to find it automatically.
        echo Set Qt6_DIR environment variable if build fails.
    )
)
echo.

cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_CXX_STANDARD=17 ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DENABLE_VTK=OFF ^
    %QT_CMAKE_ARGS%

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    echo Make sure Qt6 is installed and Qt6_DIR is set correctly.
    echo Example: set Qt6_DIR=C:\Qt\6.7.0\msvc2019_64
    exit /b 1
)

REM Build
echo.
echo Building with %JOBS% parallel jobs...
echo.

cmake --build . --target multipack-parser --config %BUILD_TYPE% -j %JOBS%

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    exit /b 1
)

REM Success
echo.
echo ==========================================
echo Build completed successfully!
echo ==========================================
echo.
echo Binary: %BUILD_DIR%\bin\%BUILD_TYPE%\multipack-parser.exe
echo    or:  %BUILD_DIR%\bin\multipack-parser.exe
echo.

REM Find the binary
set "BINARY="
if exist "%BUILD_DIR%\bin\%BUILD_TYPE%\multipack-parser.exe" (
    set "BINARY=%BUILD_DIR%\bin\%BUILD_TYPE%\multipack-parser.exe"
) else if exist "%BUILD_DIR%\bin\multipack-parser.exe" (
    set "BINARY=%BUILD_DIR%\bin\multipack-parser.exe"
)

if defined BINARY (
    echo Size:
    for %%A in ("%BINARY%") do echo   %%~zA bytes
    echo.
)

REM Run if requested
if "%RUN_AFTER%"=="1" (
    if defined BINARY (
        echo Running MultipackParser...
        echo.
        "%BINARY%"
    ) else (
        echo ERROR: Could not find built binary to run
    )
)

goto :eof

:build_arm64
echo.
echo ==========================================
echo MultipackParser ARM64 Docker Build
echo ==========================================
echo.

REM Check for Docker
where docker >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker not found in PATH
    echo.
    echo Please install Docker Desktop from https://www.docker.com/products/docker-desktop
    exit /b 1
)

REM Check if Docker is running
docker info >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker is not running.
    echo Please start Docker Desktop and try again.
    exit /b 1
)

REM Clean if requested
if "%CLEAN%"=="1" (
    echo Cleaning output directory...
    if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
    echo.
)

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo Setting up QEMU for ARM64 emulation...
docker run --privileged --rm tonistiigi/binfmt --install arm64 >nul 2>&1

echo.
echo Building ARM64 Docker image...
echo This may take several minutes on first run.
echo.

REM Remove trailing backslash
set "CONTEXT_DIR=%SCRIPT_DIR:~0,-1%"

docker buildx build --platform linux/arm64 ^
    -t multipack-parser-arm64-builder:latest ^
    -f "%CONTEXT_DIR%\Dockerfile.arm64" ^
    --load ^
    "%CONTEXT_DIR%"

if errorlevel 1 (
    echo.
    echo ERROR: Docker build failed!
    exit /b 1
)

echo.
echo Extracting build artifacts...

REM Create container and extract files
docker create --name multipack-builder multipack-parser-arm64-builder:latest >nul 2>&1

set "BUNDLE_DIR=%OUTPUT_DIR%\multipack-parser-arm64"
if exist "%BUNDLE_DIR%" rmdir /s /q "%BUNDLE_DIR%"
mkdir "%BUNDLE_DIR%\bin"
mkdir "%BUNDLE_DIR%\lib"
mkdir "%BUNDLE_DIR%\plugins\platforms"
mkdir "%BUNDLE_DIR%\plugins\sqldrivers"

echo Copying binary...
docker cp multipack-builder:/src/build/bin/multipack-parser "%BUNDLE_DIR%\bin\" 2>nul

echo Copying Qt libraries...
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/libQt6Core.so.6 "%BUNDLE_DIR%\lib\" 2>nul
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/libQt6Gui.so.6 "%BUNDLE_DIR%\lib\" 2>nul
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/libQt6Widgets.so.6 "%BUNDLE_DIR%\lib\" 2>nul
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/libQt6Network.so.6 "%BUNDLE_DIR%\lib\" 2>nul
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/libQt6Sql.so.6 "%BUNDLE_DIR%\lib\" 2>nul
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/libQt6Multimedia.so.6 "%BUNDLE_DIR%\lib\" 2>nul

echo Copying Qt plugins...
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqxcb.so "%BUNDLE_DIR%\plugins\platforms\" 2>nul
docker cp multipack-builder:/usr/lib/aarch64-linux-gnu/qt6/plugins/sqldrivers/libqsqlite.so "%BUNDLE_DIR%\plugins\sqldrivers\" 2>nul

REM Cleanup container
docker rm -f multipack-builder >nul 2>&1

REM Create run script
echo #!/bin/bash > "%BUNDLE_DIR%\run.sh"
echo SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" ^&^& pwd)" >> "%BUNDLE_DIR%\run.sh"
echo export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}" >> "%BUNDLE_DIR%\run.sh"
echo export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins" >> "%BUNDLE_DIR%\run.sh"
echo export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}" >> "%BUNDLE_DIR%\run.sh"
echo exec "${SCRIPT_DIR}/bin/multipack-parser" "$@" >> "%BUNDLE_DIR%\run.sh"

echo.
echo ==========================================
echo ARM64 Build completed successfully!
echo ==========================================
echo.
echo Output: %BUNDLE_DIR%
echo.
echo To deploy to Raspberry Pi:
echo   1. Copy folder to Pi: scp -r %BUNDLE_DIR% pi@raspberrypi:~/
echo   2. SSH to Pi: ssh pi@raspberrypi
echo   3. Run: chmod +x ~/multipack-parser-arm64/run.sh
echo   4. Run: ~/multipack-parser-arm64/run.sh
echo.

goto :eof
