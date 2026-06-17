#!/bin/bash
# Native build script for MultipackParser C++
# Builds for the current platform (macOS, Linux)

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

# Parse arguments
CLEAN=false
VERBOSE=false
RUN_AFTER=false
PORTABLE=false

print_help() {
    echo ""
    echo -e "${GREEN}MultipackParser C++ Build Script${NC}"
    echo ""
    echo "Usage: ./build.sh [options]"
    echo ""
    echo "Options:"
    echo "  -c, --clean      Clean build directory before building"
    echo "  -d, --debug      Build in Debug mode (default: Release)"
    echo "  -v, --verbose    Verbose build output"
    echo "  -r, --run        Run the application after building"
    echo "  -p, --portable   Build a single-file portable launcher (Linux only)"
    echo "  -j, --jobs N     Number of parallel jobs (default: auto)"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./build.sh                 # Build in Release mode"
    echo "  ./build.sh --debug         # Build in Debug mode"
    echo "  ./build.sh --clean --run   # Clean build and run"
    echo "  ./build.sh --portable      # Build + portable single-file launcher"
    echo ""
    echo "Requirements:"
    echo "  - CMake 3.16+"
    echo "  - Qt6 (qt6-base-dev, qt6-multimedia-dev)"
    echo "  - C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)"
    echo ""
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -r|--run)
            RUN_AFTER=true
            shift
            ;;
        -p|--portable)
            PORTABLE=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_help
            exit 1
            ;;
    esac
done

if [[ "$PORTABLE" == "true" && "$(uname -s)" != "Linux" ]]; then
    echo -e "${RED}Portable mode is currently supported on Linux only.${NC}"
    echo "Use GitHub Actions or run ./build.sh --portable on a Linux host."
    exit 1
fi

echo ""
echo -e "${GREEN}==========================================${NC}"
echo -e "${GREEN}MultipackParser C++ Build${NC}"
echo -e "${GREEN}==========================================${NC}"
echo -e "Build type:    ${BLUE}${BUILD_TYPE}${NC}"
echo -e "Build dir:     ${BLUE}${BUILD_DIR}${NC}"
echo -e "Parallel jobs: ${BLUE}${JOBS}${NC}"
echo -e "Portable mode: ${BLUE}${PORTABLE}${NC}"
echo -e "${GREEN}==========================================${NC}"
echo ""

# Check for required tools
check_requirements() {
    local missing=()
    
    if ! command -v cmake &> /dev/null; then
        missing+=("cmake")
    fi
    
    if ! command -v make &> /dev/null && ! command -v ninja &> /dev/null; then
        missing+=("make or ninja")
    fi

    if [[ "$PORTABLE" == "true" && "$(uname -s)" == "Linux" ]]; then
        if ! command -v ldd &> /dev/null; then
            missing+=("ldd")
        fi
        if ! command -v tar &> /dev/null; then
            missing+=("tar")
        fi
    fi
    
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo -e "${RED}Error: Missing required tools: ${missing[*]}${NC}"
        echo ""
        echo "Install on macOS:   brew install cmake qt@6"
        echo "Install on Ubuntu:  sudo apt install cmake qt6-base-dev qt6-multimedia-dev"
        echo "Install on Fedora:  sudo dnf install cmake qt6-qtbase-devel qt6-qtmultimedia-devel"
        exit 1
    fi
}

check_requirements

# Clean if requested
if [[ "$CLEAN" == "true" ]]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "${BUILD_DIR}"
    echo ""
fi

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Detect platform and set generator
GENERATOR=""
if command -v ninja &> /dev/null; then
    GENERATOR="-G Ninja"
    echo -e "Using ${BLUE}Ninja${NC} build system"
else
    echo -e "Using ${BLUE}Make${NC} build system"
fi

# Configure
echo ""
echo -e "${YELLOW}Configuring with CMake...${NC}"
echo ""

CMAKE_ARGS=(
    ${GENERATOR}
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DENABLE_VTK=OFF
)

if [[ -n "${MULTIPACK_APP_VERSION:-}" ]]; then
    CMAKE_ARGS+=(-DMULTIPACK_APP_VERSION="${MULTIPACK_APP_VERSION}")
fi
if [[ -n "${MULTIPACK_PROJECT_VERSION:-}" ]]; then
    CMAKE_ARGS+=(-DMULTIPACK_PROJECT_VERSION="${MULTIPACK_PROJECT_VERSION}")
fi

# Platform-specific settings
case "$(uname -s)" in
    Darwin*)
        echo -e "Platform: ${BLUE}macOS${NC}"
        # Try to find Qt6 from Homebrew
        if [[ -d "/opt/homebrew/opt/qt@6" ]]; then
            CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6")
        elif [[ -d "/usr/local/opt/qt@6" ]]; then
            CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="/usr/local/opt/qt@6")
        fi
        ;;
    Linux*)
        echo -e "Platform: ${BLUE}Linux${NC}"
        # Detect architecture for optimization flags
        ARCH=$(uname -m)
        if [[ "$ARCH" == "aarch64" || "$ARCH" == "arm64" ]]; then
            echo -e "Architecture: ${BLUE}ARM64${NC}"
            CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="-mcpu=native -O2")
        elif [[ "$ARCH" == "x86_64" ]]; then
            echo -e "Architecture: ${BLUE}x86_64${NC}"
            CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="-march=native -O2")
        fi
        ;;
    *)
        echo -e "${YELLOW}Warning: Unknown platform $(uname -s)${NC}"
        ;;
esac

cmake .. "${CMAKE_ARGS[@]}"

if [[ $? -ne 0 ]]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

# Build
echo ""
echo -e "${YELLOW}Building with ${JOBS} parallel jobs...${NC}"
echo ""

BUILD_ARGS=("--build" "." "--target" "multipack-parser" "-j" "${JOBS}")
if [[ "$VERBOSE" == "true" ]]; then
    BUILD_ARGS+=("--verbose")
fi

cmake "${BUILD_ARGS[@]}"

if [[ $? -ne 0 ]]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# Success
echo ""
echo -e "${GREEN}==========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}==========================================${NC}"
echo ""
echo -e "Binary: ${BLUE}${BUILD_DIR}/bin/multipack-parser${NC}"
echo ""

# Show binary info
if command -v file &> /dev/null; then
    echo "Binary info:"
    file "${BUILD_DIR}/bin/multipack-parser"
    echo ""
fi

# Show size
ls -lh "${BUILD_DIR}/bin/multipack-parser" | awk '{print "Size: " $5}'
echo ""

# Optional portable packaging
if [[ "$PORTABLE" == "true" ]]; then
    echo -e "${YELLOW}Creating portable runtime bundle...${NC}"
    echo ""

    PORTABLE_DIR="${BUILD_DIR}/portable"
    PORTABLE_BINARY="${BUILD_DIR}/bin/multipack-parser-portable-$(uname -m).run"

    bash "${SCRIPT_DIR}/scripts/bundle_linux_portable.sh" \
        --binary "${BUILD_DIR}/bin/multipack-parser" \
        --output-dir "${PORTABLE_DIR}"

    bash "${SCRIPT_DIR}/scripts/create_portable_single_file.sh" \
        --bundle-dir "${PORTABLE_DIR}" \
        --output-file "${PORTABLE_BINARY}"

    echo ""
    echo -e "Portable binary: ${BLUE}${PORTABLE_BINARY}${NC}"
    ls -lh "${PORTABLE_BINARY}" | awk '{print "Portable size: " $5}'
    echo ""
fi

# Run if requested
if [[ "$RUN_AFTER" == "true" ]]; then
    echo -e "${YELLOW}Running MultipackParser...${NC}"
    echo ""
    "${BUILD_DIR}/bin/multipack-parser" "$@"
fi

echo -e "${GREEN}Done!${NC}"
