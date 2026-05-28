#!/bin/bash
# MultipackParser ARM64 Startup Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set library paths
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${SCRIPT_DIR}/plugins/platforms"

# Qt environment for Raspberry Pi.
# Prefer de_DE.UTF-8 if generated on the device. Fall back to C.UTF-8 otherwise.
if [ -z "${LANG:-}" ] || [ "${LANG}" = "C" ] || [ "${LANG}" = "C.UTF-8" ] || [ "${LANG}" = "POSIX" ]; then
    if command -v locale >/dev/null 2>&1 && locale -a 2>/dev/null | grep -qiE '^de_DE\.utf-?8$'; then
        export LANG="de_DE.UTF-8"
    else
        export LANG="C.UTF-8"
        echo "Warning: de_DE.UTF-8 locale not generated; on-screen keyboard will use the default layout." >&2
        echo "         Run: sudo sed -i 's/^# *de_DE.UTF-8 UTF-8/de_DE.UTF-8 UTF-8/' /etc/locale.gen && sudo locale-gen" >&2
    fi
fi
# Never let LC_ALL=C clobber the layout choice; unset it if it is C-ish.
case "${LC_ALL:-}" in
    C|C.UTF-8|POSIX) unset LC_ALL ;;
esac
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_OPENGL="${QT_OPENGL:-software}"
export QT_XCB_GL_INTEGRATION="${QT_XCB_GL_INTEGRATION:-none}"
unset QT_IM_MODULE
export QT_X11_NO_MITSHM=1
export LIBGL_ALWAYS_SOFTWARE=1
export MULTIPACK_FULLSCREEN="${MULTIPACK_FULLSCREEN:-1}"

# Shared QWidget virtual keyboard support
export MULTIPACK_VIRTUAL_KEYBOARD="${MULTIPACK_VIRTUAL_KEYBOARD:-1}"

echo "Starting MultipackParser..."
echo "Platform: ${QT_QPA_PLATFORM}"
exec "${SCRIPT_DIR}/bin/multipack-parser" "$@"
