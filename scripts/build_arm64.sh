#!/bin/bash
# ARM64 cross-compilation build script for MultipackParser C++

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-arm64"

echo "=========================================="
echo "MultipackParser C++ - ARM64 Cross Build"
echo "=========================================="

# Check for cross compiler
if ! command -v aarch64-linux-gnu-g++ &> /dev/null; then
    echo "Error: aarch64-linux-gnu-g++ not found"
    echo "Install with: sudo apt install g++-aarch64-linux-gnu"
    exit 1
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "Configuring with CMake..."
cmake "$PROJECT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/Arm64Toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
make -j$(nproc)

echo "=========================================="
echo "ARM64 Build complete!"
echo "Binary: $BUILD_DIR/bin/multipack-parser"
echo "=========================================="
