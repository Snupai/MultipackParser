#!/bin/bash
# Docker build script for MultipackParser C++ ARM64
# Builds for ARM64 Linux using Docker buildx (cross-compilation from any host)

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="multipack-parser-arm64-builder"
CONTAINER_NAME="multipack-builder"
OUTPUT_DIR="${SCRIPT_DIR}/output"
DOCKERFILE="${SCRIPT_DIR}/Dockerfile.arm64"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}==========================================${NC}"
echo -e "${GREEN}MultipackParser ARM64 Docker Build${NC}"
echo -e "${GREEN}==========================================${NC}"

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is not installed or not in PATH${NC}"
    exit 1
fi

# Check if buildx is available for cross-platform builds
if ! docker buildx version &> /dev/null; then
    echo -e "${YELLOW}Warning: Docker buildx not available, using standard build${NC}"
    USE_BUILDX=false
else
    USE_BUILDX=true
fi

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Clean up any existing container
echo "Cleaning up previous build containers..."
docker rm -f "${CONTAINER_NAME}" 2>/dev/null || true

# Check if we need to set up QEMU for ARM64 emulation (on non-ARM hosts)
HOST_ARCH=$(uname -m)
if [[ "${HOST_ARCH}" != "aarch64" && "${HOST_ARCH}" != "arm64" ]]; then
    echo -e "${YELLOW}Host architecture: ${HOST_ARCH}${NC}"
    echo "Setting up QEMU for ARM64 emulation..."
    
    # Register QEMU handlers for ARM64
    if [[ "${USE_BUILDX}" == "true" ]]; then
        docker run --rm --privileged multiarch/qemu-user-static --reset -p yes 2>/dev/null || true
    fi
fi

echo ""
echo -e "${GREEN}Building ARM64 Docker image...${NC}"
echo "This may take several minutes on first run."
echo ""

# Build the Docker image for ARM64
if [[ "${USE_BUILDX}" == "true" ]]; then
    # Use buildx for cross-platform build
    
    # Create/use a builder that supports ARM64
    docker buildx create --name arm64builder --use 2>/dev/null || docker buildx use arm64builder 2>/dev/null || true
    
    # Build for ARM64
    docker buildx build \
        --platform linux/arm64 \
        --file "${DOCKERFILE}" \
        --tag "${IMAGE_NAME}:latest" \
        --load \
        --progress=plain \
        .
else
    # Standard build (only works on ARM64 hosts)
    docker build \
        --file "${DOCKERFILE}" \
        --tag "${IMAGE_NAME}:latest" \
        --progress=plain \
        .
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}Docker build failed!${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}Extracting build artifacts...${NC}"

# Create a container from the image to extract files
docker create --name "${CONTAINER_NAME}" "${IMAGE_NAME}:latest"

# Create bundle directory
BUNDLE_DIR="${OUTPUT_DIR}/multipack-parser-arm64"
rm -rf "${BUNDLE_DIR}"
mkdir -p "${BUNDLE_DIR}/bin"
mkdir -p "${BUNDLE_DIR}/lib"
mkdir -p "${BUNDLE_DIR}/plugins/platforms"
mkdir -p "${BUNDLE_DIR}/plugins/sqldrivers"
mkdir -p "${BUNDLE_DIR}/plugins/multimedia"

# Copy the binary (Dockerfile WORKDIR is /app)
echo "Copying binary..."
docker cp "${CONTAINER_NAME}:/app/build/bin/multipack-parser" "${BUNDLE_DIR}/bin/" 2>/dev/null || \
docker cp "${CONTAINER_NAME}:/src/build/bin/multipack-parser" "${BUNDLE_DIR}/bin/" 2>/dev/null || \
{ echo -e "${RED}Failed to copy binary${NC}"; docker rm -f "${CONTAINER_NAME}"; exit 1; }
chmod +x "${BUNDLE_DIR}/bin/multipack-parser"

# Copy Qt libraries from the container
echo "Copying Qt libraries..."
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6Core.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6Gui.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6Widgets.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6Network.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6Sql.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6Multimedia.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6DBus.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6XcbQpa.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6OpenGL.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libQt6Concurrent.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true

# Copy ICU and other Qt dependencies (required for libicui18n, etc.)
# Ubuntu 22.04 has ICU 70; try common versions
echo "Copying ICU and system libraries..."
for ver in 70 72 74; do
    docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libicuuc.so.${ver}" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
    docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libicui18n.so.${ver}" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
    docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libicudata.so.${ver}" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
done
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libpcre2-16.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libdouble-conversion.so.3" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libz.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
# Do NOT bundle libstdc++.so.6 or libgcc_s.so.1 - use system versions for GLIBCXX compatibility

# Copy X11 libraries (needed for xcb platform plugin)
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libX11.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libXext.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libXcursor.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libXfixes.so.3" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libXi.so.6" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libXrandr.so.2" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libXrender.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libXinerama.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true

# Copy Qt plugins
echo "Copying Qt plugins..."
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqxcb.so" "${BUNDLE_DIR}/plugins/platforms/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqlinuxfb.so" "${BUNDLE_DIR}/plugins/platforms/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqeglfs.so" "${BUNDLE_DIR}/plugins/platforms/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqoffscreen.so" "${BUNDLE_DIR}/plugins/platforms/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/qt6/plugins/sqldrivers/libqsqlite.so" "${BUNDLE_DIR}/plugins/sqldrivers/" 2>/dev/null || true

# Copy additional required libraries
echo "Copying additional libraries..."
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-cursor.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-icccm.so.4" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-image.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-keysyms.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-render.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-render-util.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-shape.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-shm.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-sync.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-xfixes.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-xinerama.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxcb-xkb.so.1" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxkbcommon.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/usr/lib/aarch64-linux-gnu/libxkbcommon-x11.so.0" "${BUNDLE_DIR}/lib/" 2>/dev/null || true

# Clean up container
docker rm -f "${CONTAINER_NAME}"

# Create run script from template
cp "${SCRIPT_DIR}/scripts/run_arm64.sh" "${BUNDLE_DIR}/run.sh"
chmod +x "${BUNDLE_DIR}/run.sh"

# Create qt.conf
cat > "${BUNDLE_DIR}/bin/qt.conf" << 'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
EOF

# Create systemd service file
cat > "${BUNDLE_DIR}/multipack-parser.service" << 'EOF'
[Unit]
Description=MultipackParser Application
After=network.target graphical-session.target

[Service]
Type=simple
User=pi
WorkingDirectory=/opt/multipack-parser
Environment=LD_LIBRARY_PATH=/opt/multipack-parser/lib
Environment=QT_PLUGIN_PATH=/opt/multipack-parser/plugins
Environment=QT_QPA_PLATFORM=xcb
Environment=QT_OPENGL=software
Environment=DISPLAY=:0
ExecStart=/opt/multipack-parser/run.sh
Restart=on-failure
RestartSec=5

[Install]
WantedBy=graphical.target
EOF

# Create tarball
echo ""
echo "Creating deployment archive..."
cd "${OUTPUT_DIR}"
tar -czvf multipack-parser-arm64.tar.gz multipack-parser-arm64

# Show results
echo ""
echo -e "${GREEN}==========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}==========================================${NC}"
echo ""
echo "Output files:"
echo "  - $(pwd)/multipack-parser-arm64.tar.gz"
echo "  - $(pwd)/multipack-parser-arm64/ (directory)"
echo ""
echo "Bundle contents:"
ls -lh "multipack-parser-arm64.tar.gz"
echo ""
echo "To deploy to Raspberry Pi:"
echo "  1. Copy: scp $(pwd)/multipack-parser-arm64.tar.gz pi@<raspberry-pi>:~/"
echo "  2. SSH:  ssh pi@<raspberry-pi>"
echo "  3. Extract: tar -xzvf multipack-parser-arm64.tar.gz"
echo "  4. Run: ./multipack-parser-arm64/run.sh"
echo ""
echo "Or install as service:"
echo "  sudo cp -r multipack-parser-arm64 /opt/multipack-parser"
echo "  sudo cp /opt/multipack-parser/multipack-parser.service /etc/systemd/system/"
echo "  sudo systemctl enable multipack-parser"
echo "  sudo systemctl start multipack-parser"
echo -e "${GREEN}==========================================${NC}"
