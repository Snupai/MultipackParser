#!/bin/bash
# Native build script for MultipackParser C++

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

echo "=========================================="
echo "MultipackParser C++ - Native Build"
echo "=========================================="

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "Configuring with CMake..."
cmake "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ${MULTIPACK_APP_VERSION:+-DMULTIPACK_APP_VERSION=$MULTIPACK_APP_VERSION} \
    ${MULTIPACK_PROJECT_VERSION:+-DMULTIPACK_PROJECT_VERSION=$MULTIPACK_PROJECT_VERSION}

# Build
echo "Building..."
make -j$(nproc)

echo "=========================================="
echo "Build complete!"
echo "Binary: $BUILD_DIR/bin/multipack-parser"
echo "=========================================="
