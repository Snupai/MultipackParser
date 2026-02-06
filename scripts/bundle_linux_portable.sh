#!/usr/bin/env bash
# Bundle MultipackParser runtime files for portable Linux distribution.

set -euo pipefail

print_help() {
    cat <<'EOF'
Usage: bundle_linux_portable.sh --binary <path> --output-dir <path>

Creates a portable runtime directory containing:
  - multipack-parser binary
  - lib/ shared libraries
  - plugins/ Qt plugins
  - run.sh launcher script
  - qt.conf
EOF
}

BINARY_PATH=""
OUTPUT_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --binary)
            BINARY_PATH="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            print_help >&2
            exit 1
            ;;
    esac
done

if [[ -z "$BINARY_PATH" || -z "$OUTPUT_DIR" ]]; then
    echo "Both --binary and --output-dir are required." >&2
    print_help >&2
    exit 1
fi

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Portable Linux bundling is only supported on Linux hosts." >&2
    exit 1
fi

if [[ ! -f "$BINARY_PATH" ]]; then
    echo "Binary not found: $BINARY_PATH" >&2
    exit 1
fi

if ! command -v ldd >/dev/null 2>&1; then
    echo "ldd is required to resolve runtime dependencies." >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_DIR")"
OUTPUT_DIR="$(cd "$(dirname "$OUTPUT_DIR")" && pwd)/$(basename "$OUTPUT_DIR")"
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/lib" "$OUTPUT_DIR/plugins"

# Copy main executable.
cp -a "$BINARY_PATH" "$OUTPUT_DIR/multipack-parser"
chmod +x "$OUTPUT_DIR/multipack-parser"

LIB_SEARCH_ROOTS=(
    /usr/lib
    /usr/local/lib
    /lib
    /lib64
)

copy_matching_libs() {
    local pattern="$1"
    local root
    for root in "${LIB_SEARCH_ROOTS[@]}"; do
        [[ -d "$root" ]] || continue
        while IFS= read -r -d '' lib; do
            cp -a "$lib" "$OUTPUT_DIR/lib/" 2>/dev/null || true
        done < <(find "$root" \( -type f -o -type l \) -name "$pattern" -print0 2>/dev/null)
    done
}

# Seed with the most important Qt/system libraries used by the app.
QT_LIB_BASENAMES=(
    libQt6Core
    libQt6Gui
    libQt6Widgets
    libQt6Network
    libQt6Sql
    libQt6Multimedia
    libQt6Concurrent
    libQt6DBus
    libQt6XcbQpa
    libQt6OpenGL
)

SYSTEM_LIB_BASENAMES=(
    libicuuc
    libicui18n
    libicudata
    libpcre2-16
    libdouble-conversion
    libxcb
    libxkbcommon
    libX11
    libXext
    libXcursor
    libXfixes
    libXi
    libXrandr
    libXrender
    libXinerama
    libstdc++
    libgcc_s
    libz
)

for base in "${QT_LIB_BASENAMES[@]}"; do
    copy_matching_libs "${base}.so*"
done

for base in "${SYSTEM_LIB_BASENAMES[@]}"; do
    copy_matching_libs "${base}.so*"
done

PLUGIN_ROOT=""
if command -v qtpaths6 >/dev/null 2>&1; then
    PLUGIN_ROOT="$(qtpaths6 --plugin-dir 2>/dev/null || true)"
fi

if [[ -z "$PLUGIN_ROOT" ]] && command -v qtpaths >/dev/null 2>&1; then
    PLUGIN_ROOT="$(qtpaths --plugin-dir 2>/dev/null || true)"
fi

if [[ -z "$PLUGIN_ROOT" ]]; then
    for candidate in /usr/lib/qt6/plugins /usr/lib64/qt6/plugins /usr/lib/*/qt6/plugins /usr/local/lib/qt6/plugins /usr/local/lib/*/qt6/plugins; do
        if [[ -d "$candidate" ]]; then
            PLUGIN_ROOT="$candidate"
            break
        fi
    done
fi

if [[ -n "$PLUGIN_ROOT" && -d "$PLUGIN_ROOT" ]]; then
    for plugin_dir in platforms sqldrivers multimedia imageformats iconengines styles platformthemes xcbglintegrations tls; do
        if [[ -d "$PLUGIN_ROOT/$plugin_dir" ]]; then
            cp -a "$PLUGIN_ROOT/$plugin_dir" "$OUTPUT_DIR/plugins/" 2>/dev/null || true
        fi
    done
else
    echo "Warning: Qt plugin directory not found. Portable bundle may miss platform plugins." >&2
fi

should_skip_lib() {
    local base
    base="$(basename "$1")"
    case "$base" in
        linux-vdso.so.*|ld-linux*.so*|libc.so.*|libm.so.*|libpthread.so.*|librt.so.*|libdl.so.*|libresolv.so.*|libutil.so.*|libanl.so.*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

copy_dependency() {
    local dep="$1"
    local dep_base resolved resolved_base

    [[ -n "$dep" && -e "$dep" ]] || return
    if should_skip_lib "$dep"; then
        return
    fi

    dep_base="$(basename "$dep")"
    if [[ ! -e "$OUTPUT_DIR/lib/$dep_base" ]]; then
        cp -a "$dep" "$OUTPUT_DIR/lib/" 2>/dev/null || cp -L "$dep" "$OUTPUT_DIR/lib/" 2>/dev/null || true
        DEP_CHANGED=1
    fi

    if [[ -L "$dep" ]]; then
        resolved="$(readlink -f "$dep" 2>/dev/null || true)"
        if [[ -n "$resolved" && -e "$resolved" ]]; then
            resolved_base="$(basename "$resolved")"
            if [[ ! -e "$OUTPUT_DIR/lib/$resolved_base" ]]; then
                cp -a "$resolved" "$OUTPUT_DIR/lib/" 2>/dev/null || cp -L "$resolved" "$OUTPUT_DIR/lib/" 2>/dev/null || true
                DEP_CHANGED=1
            fi
            if [[ ! -e "$OUTPUT_DIR/lib/$dep_base" ]]; then
                ln -sf "$resolved_base" "$OUTPUT_DIR/lib/$dep_base"
                DEP_CHANGED=1
            fi
        fi
    fi
}

# Resolve transitive dependencies for the main binary and copied plugins/libs.
while :; do
    DEP_CHANGED=0
    while IFS= read -r -d '' candidate; do
        while IFS= read -r dep; do
            copy_dependency "$dep"
        done < <(ldd "$candidate" 2>/dev/null | awk '{for (i = 1; i <= NF; ++i) if ($i ~ /^\//) {print $i; break}}')
    done < <(find "$OUTPUT_DIR" \( -type f -o -type l \) \( -name "multipack-parser" -o -name "*.so" -o -name "*.so.*" \) -print0)

    if [[ "$DEP_CHANGED" -eq 0 ]]; then
        break
    fi
done

cat > "$OUTPUT_DIR/run.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$SCRIPT_DIR/plugins/platforms"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_X11_NO_MITSHM=1

exec "$SCRIPT_DIR/multipack-parser" "$@"
EOF
chmod +x "$OUTPUT_DIR/run.sh"

cat > "$OUTPUT_DIR/qt.conf" <<'EOF'
[Paths]
Prefix = .
Plugins = plugins
Libraries = lib
EOF

echo "Portable runtime bundle created at: $OUTPUT_DIR"
echo "Contents:"
echo "  - $(find "$OUTPUT_DIR/lib" -maxdepth 1 -type f | wc -l | tr -d ' ') libraries"
echo "  - $(find "$OUTPUT_DIR/plugins" -type f 2>/dev/null | wc -l | tr -d ' ') plugin files"
