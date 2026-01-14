@echo off
REM Build script for MultipackParser C++ using Docker (ARM64 Linux target)
REM
REM Usage: build.bat [options]
REM
REM Options:
REM   --arm64       Build for ARM64/Raspberry Pi using QEMU emulation (default)
REM   --native      Build for native Linux x86_64
REM   --setup       Setup Docker buildx for ARM64 emulation
REM   --clean       Remove build directories and output
REM   --rebuild     Force rebuild of Docker image
REM   --help        Show this help message

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_NAME=multipack-cpp
set DOCKER_IMAGE_X86=multipack-cpp-builder
set DOCKER_IMAGE_ARM64=multipack-cpp-builder-arm64
set OUTPUT_DIR=%SCRIPT_DIR%output

REM Parse arguments
set BUILD_TYPE=arm64
set FORCE_REBUILD=0
set USER_REBUILD=0

:parse_args
if "%1"=="" goto :start_build
if "%1"=="--native" set BUILD_TYPE=native
if "%1"=="--arm64" set BUILD_TYPE=arm64
if "%1"=="--setup" goto :setup_buildx
if "%1"=="--clean" goto :clean
if "%1"=="--rebuild" set FORCE_REBUILD=1& set USER_REBUILD=1
if "%1"=="--help" goto :help
shift
goto :parse_args

:help
echo.
echo MultipackParser C++ Docker Build Script
echo.
echo Usage: build.bat [options]
echo.
echo Options:
echo   --arm64       Build for ARM64/Raspberry Pi (default)
echo   --native      Build for Linux x86_64
echo   --setup       Setup Docker buildx for ARM64 emulation
echo   --clean       Remove build directories
echo   --rebuild     Force rebuild Docker image
echo   --help        Show this help
echo.
echo Examples:
echo   build.bat                    Build for ARM64
echo   build.bat --native           Build for x86_64
echo   build.bat --setup            Setup QEMU for ARM64
echo.
goto :eof

:setup_buildx
echo Setting up Docker buildx for ARM64 emulation...
echo.

REM Enable experimental features and setup QEMU
docker run --privileged --rm tonistiigi/binfmt --install arm64
if errorlevel 1 (
    echo ERROR: Failed to setup QEMU. Make sure Docker Desktop is running.
    exit /b 1
)

REM Create buildx builder if not exists
docker buildx create --name multipack-builder --use 2>nul
docker buildx inspect --bootstrap

echo.
echo Setup complete! You can now run: build.bat --arm64
goto :eof

:start_build
echo.
echo ==========================================
echo MultipackParser C++ Docker Build
echo Target: %BUILD_TYPE%
echo ==========================================
echo.

REM Check if Docker is running
docker info >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker is not running.
    echo Please start Docker Desktop and try again.
    exit /b 1
)

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

if "%BUILD_TYPE%"=="arm64" (
    goto :build_arm64
) else (
    goto :build_native
)

:build_arm64
echo Building for ARM64 (Raspberry Pi)...
echo.

set DOCKER_IMAGE=%DOCKER_IMAGE_ARM64%

REM Remove trailing backslash from SCRIPT_DIR for Docker
set "CONTEXT_DIR=%SCRIPT_DIR:~0,-1%"

REM Check if we need to build the Docker image
docker image inspect %DOCKER_IMAGE% >nul 2>&1
if errorlevel 1 set FORCE_REBUILD=1

if "%FORCE_REBUILD%"=="1" (
    echo Building Docker image for ARM64...
    echo This may take a while on first run due to QEMU emulation.
    echo.

    REM Setup QEMU if not already done
    docker run --privileged --rm tonistiigi/binfmt --install arm64 >nul 2>&1

    set "CACHE_FLAG="
    if "!USER_REBUILD!"=="1" set "CACHE_FLAG=--no-cache"

    docker buildx build --platform linux/arm64 ^
        -t !DOCKER_IMAGE! ^
        -f "!CONTEXT_DIR!\Dockerfile.arm64" ^
        !CACHE_FLAG! ^
        --load ^
        "!CONTEXT_DIR!"

    if errorlevel 1 (
        echo ERROR: Failed to build Docker image
        exit /b 1
    )
)

echo.
echo Running ARM64 build in Docker container...
echo.

set "SRC_DIR=%SCRIPT_DIR:~0,-1%"
docker run --rm --platform linux/arm64 ^
    -v "%SRC_DIR%:/src" ^
    -v "%OUTPUT_DIR%:/output" ^
    -e BUILD_TYPE=Release ^
    -e BUILD_DIR=/src/build-arm64 ^
    -e OUTPUT_DIR=/output ^
    %DOCKER_IMAGE%

if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    exit /b 1
)

goto :build_success

:build_native
echo Building for native Linux x86_64...
echo.

set DOCKER_IMAGE=%DOCKER_IMAGE_X86%

REM Check if we need to build the Docker image
docker image inspect %DOCKER_IMAGE% >nul 2>&1
if errorlevel 1 set FORCE_REBUILD=1

set "SRC_DIR=%SCRIPT_DIR:~0,-1%"

if "%FORCE_REBUILD%"=="1" (
    echo Building Docker image...
    docker build -t %DOCKER_IMAGE% -f "%SRC_DIR%\Dockerfile" "%SRC_DIR%"
    if errorlevel 1 (
        echo ERROR: Failed to build Docker image
        exit /b 1
    )
)

echo.
echo Running build in Docker container...
echo.

docker run --rm ^
    -v "%SRC_DIR%:/src" ^
    -v "%OUTPUT_DIR%:/output" ^
    -e BUILD_TYPE=Release ^
    -e BUILD_DIR=/src/build ^
    -e OUTPUT_DIR=/output ^
    %DOCKER_IMAGE% ^
    /usr/local/bin/docker-build.sh

if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    exit /b 1
)

goto :build_success

:build_success
echo.
echo ==========================================
echo Build complete!
echo.
echo Output: %OUTPUT_DIR%\multipack-parser
echo Target: %BUILD_TYPE%
echo.
if "%BUILD_TYPE%"=="arm64" (
    echo Copy to Raspberry Pi:
    echo   scp %OUTPUT_DIR%\multipack-parser pi@raspberrypi:~/
)
echo ==========================================
goto :eof

:clean
echo Cleaning build directories...
if exist "%SCRIPT_DIR%build" rmdir /s /q "%SCRIPT_DIR%build"
if exist "%SCRIPT_DIR%build-arm64" rmdir /s /q "%SCRIPT_DIR%build-arm64"
if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
echo.
echo Clean complete.
goto :eof
