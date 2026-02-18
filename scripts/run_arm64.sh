#!/bin/bash
# MultipackParser ARM64 Startup Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set library paths
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${SCRIPT_DIR}/plugins/platforms"
export QML2_IMPORT_PATH="${SCRIPT_DIR}/qml:${QML2_IMPORT_PATH}"
export QML_IMPORT_PATH="${SCRIPT_DIR}/qml:${QML_IMPORT_PATH}"
export QT_VIRTUALKEYBOARD_HUNSPELL_DATA_PATH="${SCRIPT_DIR}/data/qtvirtualkeyboard/hunspell:${QT_VIRTUALKEYBOARD_HUNSPELL_DATA_PATH}"

# Qt environment for Raspberry Pi (force German locale by default)
export LANG="${LANG:-de_DE.UTF-8}"
export LC_ALL="${LC_ALL:-de_DE.UTF-8}"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_OPENGL="${QT_OPENGL:-software}"
export QT_QUICK_BACKEND="${QT_QUICK_BACKEND:-software}"
export QSG_RHI_BACKEND="${QSG_RHI_BACKEND:-software}"
export QT_XCB_GL_INTEGRATION="${QT_XCB_GL_INTEGRATION:-none}"
export QT_X11_NO_MITSHM=1
export LIBGL_ALWAYS_SOFTWARE=1

# Virtual keyboard support
export QT_IM_MODULE="${QT_IM_MODULE:-qtvirtualkeyboard}"

echo "Starting MultipackParser..."
echo "Platform: ${QT_QPA_PLATFORM}"
exec "${SCRIPT_DIR}/bin/multipack-parser" "$@"
