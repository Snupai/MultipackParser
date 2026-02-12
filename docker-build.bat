@echo off
REM Docker build script for MultipackParser C++ ARM64
REM Builds for ARM64/aarch64 Linux (Raspberry Pi) using Docker buildx
REM Equivalent to docker-build.sh for Windows
REM
REM Usage: docker-build.bat [options]
REM   --portable   Create single-file portable launcher (.run)
REM   --help       Show this help

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
set IMAGE_NAME=multipack-parser-arm64-builder
set CONTAINER_NAME=multipack-builder
set OUTPUT_DIR=%SCRIPT_DIR%\output
set DOCKERFILE=%SCRIPT_DIR%\Dockerfile.arm64
set PORTABLE=0

REM Parse arguments
:parse_args
if "%1"=="" goto :after_parse
if /i "%1"=="--portable" set PORTABLE=1& shift& goto :parse_args
if /i "%1"=="-p" set PORTABLE=1& shift& goto :parse_args
if /i "%1"=="--help" goto :help
if /i "%1"=="-h" goto :help
echo Unknown option: %1
goto :help

:help
echo.
echo Usage: docker-build.bat [options]
echo.
echo Options:
echo   --portable, -p   Create single-file portable launcher ^(multipack-parser-arm64-portable.run^)
echo   --help, -h      Show this help
echo.
goto :eof

:after_parse

echo.
echo ==========================================
echo MultipackParser ARM64 Docker Build
echo ==========================================
echo.

REM Check if Docker is available
where docker >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker is not installed or not in PATH
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

REM Check if buildx is available
docker buildx version >nul 2>&1
if errorlevel 1 (
    echo WARNING: Docker buildx not available, using standard build
    set USE_BUILDX=0
) else (
    set USE_BUILDX=1
)

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Clean up any existing container
echo Cleaning up previous build containers...
docker rm -f %CONTAINER_NAME% >nul 2>&1

REM Set up QEMU for ARM64 emulation on non-ARM hosts (x86_64 Windows)
if "%USE_BUILDX%"=="1" (
    echo Setting up QEMU for ARM64 emulation...
    docker run --rm --privileged multiarch/qemu-user-static --reset -p yes >nul 2>&1
)

echo.
echo Building ARM64 Docker image...
echo This may take several minutes on first run.
echo.

REM Build the Docker image for ARM64
if "%USE_BUILDX%"=="1" (
    REM Create/use a builder that supports ARM64
    docker buildx create --name arm64builder --use >nul 2>&1
    if errorlevel 1 docker buildx use arm64builder >nul 2>&1

    REM Build for ARM64
    docker buildx build --platform linux/arm64 ^
        --file "%DOCKERFILE%" ^
        --tag %IMAGE_NAME%:latest ^
        --load ^
        --progress=plain ^
        "%SCRIPT_DIR%"
) else (
    docker build --file "%DOCKERFILE%" ^
        --tag %IMAGE_NAME%:latest ^
        --progress=plain ^
        "%SCRIPT_DIR%"
)

if errorlevel 1 (
    echo.
    echo ERROR: Docker build failed!
    exit /b 1
)

echo.
echo Extracting build artifacts...

REM Create bundle directory
set BUNDLE_DIR=%OUTPUT_DIR%\multipack-parser-arm64
if exist "%BUNDLE_DIR%" rmdir /s /q "%BUNDLE_DIR%"
mkdir "%BUNDLE_DIR%\bin"
mkdir "%BUNDLE_DIR%\lib"
mkdir "%BUNDLE_DIR%\plugins\platforms"
mkdir "%BUNDLE_DIR%\plugins\sqldrivers"
mkdir "%BUNDLE_DIR%\plugins\multimedia"

REM Copy all artifacts using docker run + volume mount (reliable on Windows cross-platform)
echo Copying binary, libraries and plugins...
set "BASH_CMD=cp /app/build/bin/multipack-parser /output/multipack-parser-arm64/bin/ && chmod +x /output/multipack-parser-arm64/bin/multipack-parser; cp /usr/lib/aarch64-linux-gnu/libQt6Core.so.6 /usr/lib/aarch64-linux-gnu/libQt6Gui.so.6 /usr/lib/aarch64-linux-gnu/libQt6Widgets.so.6 /usr/lib/aarch64-linux-gnu/libQt6Network.so.6 /usr/lib/aarch64-linux-gnu/libQt6Sql.so.6 /usr/lib/aarch64-linux-gnu/libQt6Multimedia.so.6 /usr/lib/aarch64-linux-gnu/libQt6DBus.so.6 /usr/lib/aarch64-linux-gnu/libQt6XcbQpa.so.6 /usr/lib/aarch64-linux-gnu/libQt6OpenGL.so.6 /usr/lib/aarch64-linux-gnu/libQt6Concurrent.so.6 /output/multipack-parser-arm64/lib/; cp /usr/lib/aarch64-linux-gnu/libicu*.so* /usr/lib/aarch64-linux-gnu/libpcre2-16*.so* /usr/lib/aarch64-linux-gnu/libdouble-conversion*.so* /usr/lib/aarch64-linux-gnu/libz.so* /output/multipack-parser-arm64/lib/ 2>/dev/null; cp /usr/lib/aarch64-linux-gnu/libxcb*.so* /usr/lib/aarch64-linux-gnu/libxkbcommon*.so* /usr/lib/aarch64-linux-gnu/libX11.so* /usr/lib/aarch64-linux-gnu/libXext.so* /usr/lib/aarch64-linux-gnu/libXcursor.so* /usr/lib/aarch64-linux-gnu/libXfixes.so* /usr/lib/aarch64-linux-gnu/libXi.so* /usr/lib/aarch64-linux-gnu/libXrandr.so* /usr/lib/aarch64-linux-gnu/libXrender.so* /usr/lib/aarch64-linux-gnu/libXinerama.so* /output/multipack-parser-arm64/lib/ 2>/dev/null; cp /usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqxcb.so /usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqlinuxfb.so /usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqeglfs.so /usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqoffscreen.so /output/multipack-parser-arm64/plugins/platforms/; cp /usr/lib/aarch64-linux-gnu/qt6/plugins/sqldrivers/libqsqlite.so /output/multipack-parser-arm64/plugins/sqldrivers/; true"
docker run --rm --platform linux/arm64 -v "%OUTPUT_DIR%:/output" %IMAGE_NAME%:latest bash -c "%BASH_CMD%"
if errorlevel 1 (
    echo ERROR: Failed to copy build artifacts
    exit /b 1
)
if not exist "%BUNDLE_DIR%\bin\multipack-parser" (
    echo ERROR: Binary not found after copy
    exit /b 1
)

REM Create run.sh from template (avoids batch quoting issues)
copy "%SCRIPT_DIR%\scripts\run_arm64.sh" "%BUNDLE_DIR%\run.sh" >nul

REM Create qt.conf
(
echo [Paths]
echo Prefix = ..
echo Plugins = plugins
echo Libraries = lib
) > "%BUNDLE_DIR%\bin\qt.conf"

REM Create systemd service file
(
echo [Unit]
echo Description=MultipackParser Application
echo After=network.target graphical-session.target
echo.
echo [Service]
echo Type=simple
echo User=pi
echo WorkingDirectory=/opt/multipack-parser
echo Environment=LD_LIBRARY_PATH=/opt/multipack-parser/lib
echo Environment=QT_PLUGIN_PATH=/opt/multipack-parser/plugins
echo Environment=QT_QPA_PLATFORM=xcb
echo Environment=QT_OPENGL=software
echo Environment=DISPLAY=:0
echo ExecStart=/opt/multipack-parser/run.sh
echo Restart=on-failure
echo RestartSec=5
echo.
echo [Install]
echo WantedBy=graphical.target
) > "%BUNDLE_DIR%\multipack-parser.service"

REM Set execute permissions and create tarball (inside container to preserve permissions)
echo.
echo Creating deployment archive...
set "TAR_CMD=chmod +x /output/multipack-parser-arm64/run.sh /output/multipack-parser-arm64/bin/multipack-parser; cd /output && tar -czvf multipack-parser-arm64.tar.gz multipack-parser-arm64"
docker run --rm --platform linux/arm64 -v "%OUTPUT_DIR%:/output" %IMAGE_NAME%:latest bash -c "%TAR_CMD%"
if errorlevel 1 (
    echo WARNING: Could not create tarball.
    echo Bundle is available at: %BUNDLE_DIR%
)

REM Optional: Create single-file portable launcher
if "%PORTABLE%"=="1" (
    echo.
    echo Creating portable single-file launcher...
    docker run --rm --platform linux/arm64 ^
        -v "%SCRIPT_DIR%:/work" ^
        -v "%OUTPUT_DIR%:/output" ^
        %IMAGE_NAME%:latest ^
        bash -c "bash /work/scripts/bundle_linux_portable.sh --binary /output/multipack-parser-arm64/bin/multipack-parser --output-dir /output/bundle"
    if errorlevel 1 (
        echo WARNING: Portable bundle creation failed
    ) else (
        docker run --rm --platform linux/arm64 ^
            -v "%SCRIPT_DIR%:/work" ^
            -v "%OUTPUT_DIR%:/output" ^
            %IMAGE_NAME%:latest ^
            bash -c "bash /work/scripts/create_portable_single_file.sh --bundle-dir /output/bundle --output-file /output/multipack-parser-arm64-portable.run"
        if errorlevel 1 (
            echo WARNING: Portable launcher creation failed
        ) else (
            echo Portable launcher created: %OUTPUT_DIR%\multipack-parser-arm64-portable.run
        )
    )
)

echo.
echo ==========================================
echo Build completed successfully!
echo ==========================================
echo.
echo Output: %BUNDLE_DIR%
if exist "%OUTPUT_DIR%\multipack-parser-arm64.tar.gz" (
    echo Archive: %OUTPUT_DIR%\multipack-parser-arm64.tar.gz
    for %%A in ("%OUTPUT_DIR%\multipack-parser-arm64.tar.gz") do echo Size: %%~zA bytes
)
if exist "%OUTPUT_DIR%\multipack-parser-arm64-portable.run" (
    echo Portable: %OUTPUT_DIR%\multipack-parser-arm64-portable.run
    for %%A in ("%OUTPUT_DIR%\multipack-parser-arm64-portable.run") do echo Size: %%~zA bytes
    echo   Deploy: scp output\multipack-parser-arm64-portable.run pi@^<raspberry-pi^>:~/
    echo   Run on Pi: chmod +x multipack-parser-arm64-portable.run ^&^& ./multipack-parser-arm64-portable.run
)
if exist "%OUTPUT_DIR%\multipack-parser-arm64.tar.gz" (
    echo.
    echo To deploy to Raspberry Pi:
    echo   1. Copy: scp output\multipack-parser-arm64.tar.gz pi@^<raspberry-pi^>:~/
    echo   2. SSH:  ssh pi@^<raspberry-pi^>
    echo   3. Extract: tar -xzvf multipack-parser-arm64.tar.gz
    echo   4. Run: ./multipack-parser-arm64/run.sh
    echo.
    echo Or install as service:
    echo   sudo cp -r multipack-parser-arm64 /opt/multipack-parser
    echo   sudo cp /opt/multipack-parser/multipack-parser.service /etc/systemd/system/
    echo   sudo systemctl enable multipack-parser
    echo   sudo systemctl start multipack-parser
)
echo.
goto :eof
