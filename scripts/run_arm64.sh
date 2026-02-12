#!/bin/bash
# MultipackParser ARM64 Startup Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set library paths
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${SCRIPT_DIR}/plugins/platforms"

# Qt environment for Raspberry Pi
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_OPENGL="${QT_OPENGL:-software}"
export QT_X11_NO_MITSHM=1
export LIBGL_ALWAYS_SOFTWARE=1

# Virtual keyboard support
export QT_IM_MODULE="${QT_IM_MODULE:-qtvirtualkeyboard}"

echo "Starting MultipackParser..."
echo "Platform: ${QT_QPA_PLATFORM}"
exec "${SCRIPT_DIR}/bin/multipack-parser" "$@"
