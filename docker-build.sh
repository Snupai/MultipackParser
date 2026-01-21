#!/bin/bash
# Enhanced Docker build script for MultipackParser C++
# Creates optimized cross-platform deployment bundles

set -e

# Build configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-src/build}"
OUTPUT_DIR="${OUTPUT_DIR:-output}"
TARGET_ARCH="${TARGET_ARCH:-$(uname -m)}"
ENABLE_TESTS="${ENABLE_TESTS:-OFF}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

echo "=========================================="
echo "MultipackParser C++ Build ${BUILD_TYPE}"
echo "Target architecture: ${TARGET_ARCH}"
echo "Build dir: ${BUILD_DIR}"
echo "Output dir: ${OUTPUT_DIR}"
echo "=========================================="

# Detect build environment
if [[ -n "${CI}" ]]; then
    echo "CI environment detected, optimizing for CI/CD"
fi

# Create and enter build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with optimizations
echo "Configuring with CMake..."
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
    -DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY_TYPE=ON
    -DCMAKE_SKIP_BUILD_RPATH=ON
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
    -DENABLE_TESTS="${ENABLE_TESTS}"
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_COLOR_DIAGNOSTICS=ON
    -DENABLE_VTK=OFF
)

# Add architecture-specific optimizations
case "${TARGET_ARCH}" in
    "aarch64"|"arm64")
        echo "ARM64 optimizations enabled"
        CMAKE_ARGS+=(
            -DCMAKE_CXX_FLAGS="-mcpu=cortex-a72 -mtune=cortex-a72 -O2"
            -DCMAKE_C_FLAGS="-mcpu=cortex-a72 -mtune=cortex-a72 -O2"
        )
        ;;
    "x86_64")
        echo "x86_64 optimizations enabled"
        CMAKE_ARGS+=(
            -DCMAKE_CXX_FLAGS="-march=native -O2"
            -DCMAKE_C_FLAGS="-march=native -O2"
        )
        ;;
    *)
        echo "Using generic optimizations"
        ;;
esac

cmake .. "${CMAKE_ARGS[@]}"

# Build with proper error handling
echo "Building with $(nproc) cores..."
if ! cmake --build . --target multipack-parser -- -j$(nproc); then
    echo "CMake configuration failed"
    exit 1
fi

if ! cmake --build . --target multipack-parser; then
    echo "Build failed"
    exit 1
fi

# Create deployment bundle
if [ "${BUILD_TYPE}" = "Release" ] && [ -d "${OUTPUT_DIR}" ]; then
    echo "=========================================="
    echo "Creating optimized deployment bundle..."
    echo "=========================================="
    
    DEPLOY_DIR="${OUTPUT_DIR}/multipack-parser-bundle-${TARGET_ARCH}"
    rm -rf "${DEPLOY_DIR}"
    
    # Create directory structure
    mkdir -p "${DEPLOY_DIR}/bin"
    mkdir -p "${DEPLOY_DIR}/lib"
    mkdir -p "${DEPLOY_DIR}/plugins/platforms"
    mkdir -p "${DEPLOY_DIR}/plugins/sqldrivers"
    mkdir -p "${DEPLOY_DIR}/plugins/multimedia"
    mkdir -p "${DEPLOY_DIR}/resources"
    
    # Copy built files
    echo "Copying built files..."
    cp -v "bin/multipack-parser"* "${DEPLOY_DIR}/bin/" 2>/dev/null || true
    cp -v "lib/"*.so* "${DEPLOY_DIR}/lib/" 2>/dev/null || true
    cp -v "plugins/"*.so* "${DEPLOY_DIR}/plugins/" 2>/dev/null || true
    
    # Copy resources if they exist
    if [ -d "resources" ]; then
        cp -r resources/* "${DEPLOY_DIR}/resources/" 2>/dev/null || true
    fi
    
    # Copy UI files if they exist
    if [ -d "ui" ]; then
        mkdir -p "${DEPLOY_DIR}/ui"
        cp -r ui/*.ui "${DEPLOY_DIR}/ui/" 2>/dev/null || true
    fi
    
    # Get list of required libraries for bundling
    echo "Analyzing library dependencies..."
    
    # Create optimized startup script
    cat > "${DEPLOY_DIR}/run.sh" << 'EOF'
#!/bin/bash
# MultipackParser Startup Script
# Auto-generated deployment script

set -e

# Get script directory
SCRIPT_DIR="$(dirname "$0")"
DEPLOY_DIR="${SCRIPT_DIR}"

# Setup environment
export LD_LIBRARY_PATH="${DEPLOY_DIR}/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="${DEPLOY_DIR}/plugins"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export PATH="${DEPLOY_DIR}/bin:$PATH"

# Set optimized Qt environment
export QT_AUTO_SCREEN_SCALE_FACTOR="${QT_AUTO_SCREEN_SCALE_FACTOR:-1}"
export QT_OPENGL="${QT_OPENGL:-software}"
export QT_X11_NO_MITSHM="${QT_X11_NO_MITSHM:-1}"
export QT_IM_MODULE="${QT_IM_MODULE:-qtvirtualkeyboard}"

# Hardware acceleration settings
if [ -c /sys/class/drm ]; then
    export QT_QPA_PLATFORM="eglfs"
fi

echo "Starting MultipackParser C++..."
echo "Architecture: $(uname -m)"
echo "Qt Platform: ${QT_QPA_PLATFORM}"
echo "OpenGL: ${QT_OPENGL}"
exec "${DEPLOY_DIR}/bin/multipack-parser" "$@"
EOF
    
    chmod +x "${DEPLOY_DIR}/run.sh"
    
    # Create environment setup script
    cat > "${DEPLOY_DIR}/env.sh" << 'EOF'
#!/bin/bash
# Environment setup for MultipackParser C++
# Source this script to set up proper environment

export PATH="$(dirname "$0")/bin:$PATH"
export LD_LIBRARY_PATH="$(dirname "$0")/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$(dirname "$0")/plugins"
export QT_QPA_PLATFORM="xcb"
export QT_OPENGL="software"
export QT_AUTO_SCREEN_SCALE_FACTOR="1"
EOF
    
    chmod +x "${DEPLOY_DIR}/env.sh"
    
    # Create service file for systemd
    if command -v systemctl >/dev/null 2>&1; then
        cat > "${DEPLOY_DIR}/multipack-parser.service" << 'EOF'
[Unit]
Description=MultipackParser C++ Application
After=network.target graphical-session.target

[Service]
Type=simple
User=multipack
WorkingDirectory=/opt/multipack-parser
Environment=PATH=/opt/multipack-parser/bin:/usr/local/bin
Environment=LD_LIBRARY_PATH=/opt/multipack-parser/lib
Environment=QT_PLUGIN_PATH=/opt/multipack-parser/plugins
Environment=QT_QPA_PLATFORM=eglfs
ExecStart=/opt/multipack-parser/bin/multipack-parser
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF
        
        echo "Systemd service file created"
    fi
    
    # Create version info
    echo "Version: $(./bin/multipack-parser --version)" > "${DEPLOY_DIR}/VERSION"
    
    echo "Deployment bundle created successfully!"
    echo "Location: ${DEPLOY_DIR}"
    echo "Architecture: ${TARGET_ARCH}"
    echo "Run with: ./${DEPLOY_DIR}/run.sh"
    
    if [ -n "${CI}" ]; then
        echo "::set-output name=bundle_path::${DEPLOY_DIR}"
        echo "::set-output name=arch::${TARGET_ARCH}"
    fi
else
    echo "Build completed, no deployment bundle created"
fi
    done

    # Copy Qt6 private/internal libraries
    for lib in /usr/lib/aarch64-linux-gnu/libQt6*.so.6; do
        if [ -f "$lib" ]; then
            cp -v "$lib" "${DEPLOY_DIR}/lib/" 2>/dev/null || true
        fi
    done

    # Copy Qt plugins
    QT_PLUGIN_PATH="/usr/lib/aarch64-linux-gnu/qt6/plugins"
    if [ -d "$QT_PLUGIN_PATH/platforms" ]; then
        cp -v "$QT_PLUGIN_PATH/platforms/libqxcb.so" "${DEPLOY_DIR}/plugins/platforms/" 2>/dev/null || true
        cp -v "$QT_PLUGIN_PATH/platforms/libqlinuxfb.so" "${DEPLOY_DIR}/plugins/platforms/" 2>/dev/null || true
        cp -v "$QT_PLUGIN_PATH/platforms/libqeglfs.so" "${DEPLOY_DIR}/plugins/platforms/" 2>/dev/null || true
        cp -v "$QT_PLUGIN_PATH/platforms/libqoffscreen.so" "${DEPLOY_DIR}/plugins/platforms/" 2>/dev/null || true
    fi

    if [ -d "$QT_PLUGIN_PATH/sqldrivers" ]; then
        cp -v "$QT_PLUGIN_PATH/sqldrivers/"*.so "${DEPLOY_DIR}/plugins/sqldrivers/" 2>/dev/null || true
    fi

    if [ -d "$QT_PLUGIN_PATH/multimedia" ]; then
        cp -v "$QT_PLUGIN_PATH/multimedia/"*.so "${DEPLOY_DIR}/plugins/multimedia/" 2>/dev/null || true
    fi

    # Copy xcb and other required system libraries
    echo "Copying system dependencies..."

    # Use ldd to find all dependencies and copy them
    ldd "${DEPLOY_DIR}/multipack-parser" | grep "=> /" | awk '{print $3}' | while read lib; do
        # Skip system libraries that should be on target
        case "$lib" in
            /lib/aarch64-linux-gnu/libc.so*) continue ;;
            /lib/aarch64-linux-gnu/libm.so*) continue ;;
            /lib/aarch64-linux-gnu/libpthread.so*) continue ;;
            /lib/aarch64-linux-gnu/libdl.so*) continue ;;
            /lib/aarch64-linux-gnu/librt.so*) continue ;;
            /lib/aarch64-linux-gnu/ld-linux*) continue ;;
        esac

        if [ -f "$lib" ] && [ ! -f "${DEPLOY_DIR}/lib/$(basename $lib)" ]; then
            cp -v "$lib" "${DEPLOY_DIR}/lib/" 2>/dev/null || true
        fi
    done

    # Also check plugin dependencies
    for plugin in "${DEPLOY_DIR}/plugins"/**/*.so; do
        if [ -f "$plugin" ]; then
            ldd "$plugin" 2>/dev/null | grep "=> /" | awk '{print $3}' | while read lib; do
                case "$lib" in
                    /lib/aarch64-linux-gnu/libc.so*) continue ;;
                    /lib/aarch64-linux-gnu/libm.so*) continue ;;
                    /lib/aarch64-linux-gnu/libpthread.so*) continue ;;
                    /lib/aarch64-linux-gnu/libdl.so*) continue ;;
                    /lib/aarch64-linux-gnu/librt.so*) continue ;;
                    /lib/aarch64-linux-gnu/ld-linux*) continue ;;
                esac

                if [ -f "$lib" ] && [ ! -f "${DEPLOY_DIR}/lib/$(basename $lib)" ]; then
                    cp -v "$lib" "${DEPLOY_DIR}/lib/" 2>/dev/null || true
                fi
            done
        fi
    done

    # Create wrapper script
    cat > "${DEPLOY_DIR}/run.sh" << 'RUNSCRIPT'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${SCRIPT_DIR}/plugins/platforms"
exec "${SCRIPT_DIR}/multipack-parser" "$@"
RUNSCRIPT
    chmod +x "${DEPLOY_DIR}/run.sh"

    # Create qt.conf for plugin discovery
    cat > "${DEPLOY_DIR}/qt.conf" << 'QTCONF'
[Paths]
Prefix = .
Plugins = plugins
Libraries = lib
QTCONF

    # Show bundle contents
    echo ""
    echo "=========================================="
    echo "Bundle contents:"
    echo "=========================================="
    du -sh "${DEPLOY_DIR}"
    ls -la "${DEPLOY_DIR}/"
    echo ""
    echo "Libraries:"
    ls -la "${DEPLOY_DIR}/lib/" | head -20
    echo "..."
    echo "Total libraries: $(ls -1 "${DEPLOY_DIR}/lib/" | wc -l)"

    # Create tarball for easy transfer
    cd "${OUTPUT_DIR}"
    tar -czvf multipack-parser-arm64.tar.gz multipack-parser-bundle

    echo ""
    echo "=========================================="
    echo "Deployment bundle created:"
    echo "  ${OUTPUT_DIR}/multipack-parser-arm64.tar.gz"
    echo ""
    echo "To deploy:"
    echo "  1. Copy tarball to Raspberry Pi"
    echo "  2. Extract: tar -xzvf multipack-parser-arm64.tar.gz"
    echo "  3. Run: ./multipack-parser-bundle/run.sh"
    echo "=========================================="
fi

echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
