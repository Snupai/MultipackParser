#!/bin/bash
# Docker build script for MultipackParser C++
# Creates a self-contained deployment bundle with all dependencies

set -e

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-/src/build}"
OUTPUT_DIR="${OUTPUT_DIR:-/output}"

echo "=========================================="
echo "MultipackParser C++ Build"
echo "Build type: ${BUILD_TYPE}"
echo "Build dir: ${BUILD_DIR}"
echo "=========================================="

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure
echo "Configuring with CMake..."
cmake /src \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DENABLE_VTK=OFF

# Build
echo "Building..."
make -j$(nproc)

# Create deployment bundle
if [ -d "${OUTPUT_DIR}" ]; then
    echo "=========================================="
    echo "Creating deployment bundle..."
    echo "=========================================="

    DEPLOY_DIR="${OUTPUT_DIR}/multipack-parser-bundle"
    rm -rf "${DEPLOY_DIR}"
    mkdir -p "${DEPLOY_DIR}/lib"
    mkdir -p "${DEPLOY_DIR}/plugins/platforms"
    mkdir -p "${DEPLOY_DIR}/plugins/sqldrivers"
    mkdir -p "${DEPLOY_DIR}/plugins/multimedia"

    # Copy binary
    cp -v "${BUILD_DIR}/bin/multipack-parser" "${DEPLOY_DIR}/"

    # Get list of required libraries using ldd
    echo "Collecting required libraries..."

    # Copy Qt6 libraries
    for lib in Core Gui Widgets Network Sql Multimedia OpenGL DBus XcbQpa; do
        libfile="/usr/lib/aarch64-linux-gnu/libQt6${lib}.so.6"
        if [ -f "$libfile" ]; then
            cp -v "$libfile" "${DEPLOY_DIR}/lib/" 2>/dev/null || true
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
