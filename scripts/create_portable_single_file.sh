#!/usr/bin/env bash
# Create a single-file self-extracting portable launcher.

set -euo pipefail

print_help() {
    cat <<'EOF'
Usage: create_portable_single_file.sh --bundle-dir <path> --output-file <path> [--entrypoint <path>]

Creates one executable file containing the full portable bundle.
At runtime it extracts to a temporary directory and executes the entrypoint.
EOF
}

BUNDLE_DIR=""
OUTPUT_FILE=""
ENTRYPOINT="run.sh"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --bundle-dir)
            BUNDLE_DIR="$2"
            shift 2
            ;;
        --output-file)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        --entrypoint)
            ENTRYPOINT="$2"
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

if [[ -z "$BUNDLE_DIR" || -z "$OUTPUT_FILE" ]]; then
    echo "Both --bundle-dir and --output-file are required." >&2
    print_help >&2
    exit 1
fi

if [[ ! -d "$BUNDLE_DIR" ]]; then
    echo "Bundle directory not found: $BUNDLE_DIR" >&2
    exit 1
fi

if [[ ! -f "$BUNDLE_DIR/$ENTRYPOINT" ]]; then
    echo "Entrypoint not found in bundle: $BUNDLE_DIR/$ENTRYPOINT" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_FILE")"
OUTPUT_FILE="$(cd "$(dirname "$OUTPUT_FILE")" && pwd)/$(basename "$OUTPUT_FILE")"

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

PAYLOAD_PATH="$TMP_DIR/payload.tar.gz"
STUB_PATH="$TMP_DIR/stub.sh"
PATCHED_STUB_PATH="$TMP_DIR/stub.patched.sh"

tar -czf "$PAYLOAD_PATH" -C "$BUNDLE_DIR" .

cat > "$STUB_PATH" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

SELF="$0"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/multipack-portable.XXXXXX")"
cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT INT TERM

PAYLOAD_LINE="$(awk '/^__PORTABLE_PAYLOAD_BELOW__$/ {print NR + 1; exit}' "$SELF")"
if [[ -z "$PAYLOAD_LINE" ]]; then
    echo "Portable payload marker not found in launcher." >&2
    exit 1
fi

tail -n +"$PAYLOAD_LINE" "$SELF" | tar -xzf - -C "$WORK_DIR"
export MULTIPACK_PORTABLE_RUN=1
exec "$WORK_DIR/ENTRYPOINT_PLACEHOLDER" "$@"

__PORTABLE_PAYLOAD_BELOW__
EOF

sed "s|ENTRYPOINT_PLACEHOLDER|$ENTRYPOINT|g" "$STUB_PATH" > "$PATCHED_STUB_PATH"

cat "$PATCHED_STUB_PATH" "$PAYLOAD_PATH" > "$OUTPUT_FILE"
chmod +x "$OUTPUT_FILE"

echo "Single-file portable launcher created: $OUTPUT_FILE"
