#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FX_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$FX_DIR/.." && pwd)
REPO_PARENT=$(CDPATH= cd -- "$REPO_ROOT/.." && pwd)
REPO_NAME=$(basename "$REPO_ROOT")
ARTIFACTS_ROOT=${CORT_ARTIFACTS_ROOT:-"$REPO_PARENT/${REPO_NAME}-artifacts"}
BUILD_DIR=${BUILD_DIR:-"$ARTIFACTS_ROOT/cort-fx/build"}
OUT_DIR=${1:-"$ARTIFACTS_ROOT/cort-fx/runs/subset3a-fx-vs-mx"}
COMPARE_TOOL="$REPO_ROOT/tools/compare_subset3a_bplist_json.exs"
ELIXIR_RUN="$REPO_ROOT/tools/run_elixir.sh"
FX_JSON=${FX_BPLIST_JSON:-${BPLIST_CORE_FX_JSON:-}}
MX_JSON=${MX_JSON:-${BPLIST_CORE_MX_JSON:-}}

case "$OUT_DIR" in
    /*) ;;
    *) OUT_DIR="$FX_DIR/$OUT_DIR" ;;
esac
TMP_DIR="${OUT_DIR}.tmp"

if [ -z "$FX_JSON" ]; then
    printf '%s\n' 'set FX_BPLIST_JSON=/path/to/subset3a_bplist_fx.json' >&2
    exit 1
fi

if [ -z "$MX_JSON" ]; then
    printf '%s\n' 'set MX_JSON=/path/to/subset3a_bplist_core.json' >&2
    exit 1
fi

case "$FX_JSON" in
    /*) ;;
    *) FX_JSON="$FX_DIR/$FX_JSON" ;;
esac

case "$MX_JSON" in
    /*) ;;
    *) MX_JSON="$FX_DIR/$MX_JSON" ;;
esac

if [ ! -f "$FX_JSON" ]; then
    printf 'missing FX JSON: %s\n' "$FX_JSON" >&2
    exit 1
fi

if [ ! -f "$MX_JSON" ]; then
    printf 'missing MX JSON: %s\n' "$MX_JSON" >&2
    exit 1
fi

hash_file() {
    file=$1
    rel=$2
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | awk -v rel="$rel" '{print $1 "  " rel}'
        return
    fi
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | awk -v rel="$rel" '{print $1 "  " rel}'
        return
    fi
    if command -v sha256 >/dev/null 2>&1; then
        digest=$(sha256 -q "$file")
        printf '%s  %s\n' "$digest" "$rel"
        return
    fi
    printf 'no-sha256-tool  %s\n' "$rel"
}

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/out"

{
    printf 'cwd: %s\n' "$FX_DIR"
    printf 'artifacts_root: %s\n' "$ARTIFACTS_ROOT"
    printf 'build_dir: %s\n' "$BUILD_DIR"
    printf 'run_dir: %s\n' "$OUT_DIR"
    printf 'compare: %s %s --fx-json out/subset3a_bplist_fx.json --fx-label out/subset3a_bplist_fx.json --mx-json out/subset3a_bplist_mx.json --mx-label out/subset3a_bplist_mx.json --output out/subset3a_bplist_fx_vs_mx_report.md\n' "$ELIXIR_RUN" "$COMPARE_TOOL"
} > "$TMP_DIR/commands.txt"

uname -a > "$TMP_DIR/host.txt"
{
    if command -v cc >/dev/null 2>&1; then
        cc --version
    fi
    printf '\n'
    if command -v clang >/dev/null 2>&1; then
        clang --version
    fi
    printf '\n'
    "$ELIXIR_RUN" --version
} > "$TMP_DIR/toolchain.txt"

cp "$FX_JSON" "$TMP_DIR/out/subset3a_bplist_fx.json"
cp "$MX_JSON" "$TMP_DIR/out/subset3a_bplist_mx.json"

"$ELIXIR_RUN" "$COMPARE_TOOL" \
    --fx-json "$TMP_DIR/out/subset3a_bplist_fx.json" \
    --fx-label "out/subset3a_bplist_fx.json" \
    --mx-json "$TMP_DIR/out/subset3a_bplist_mx.json" \
    --mx-label "out/subset3a_bplist_mx.json" \
    --output "$TMP_DIR/out/subset3a_bplist_fx_vs_mx_report.md" \
    > "$TMP_DIR/out/compare.stdout" \
    2> "$TMP_DIR/out/compare.stderr" || compare_status=$?

: "${compare_status:=0}"

cp "$TMP_DIR/out/subset3a_bplist_fx_vs_mx_report.md" "$TMP_DIR/summary.md"

(
    cd "$TMP_DIR"
    find . -type f ! -name sha256.txt | sort | while IFS= read -r rel; do
        rel=${rel#./}
        hash_file "$TMP_DIR/$rel" "$rel"
    done
) > "$TMP_DIR/sha256.txt"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote FX Subset 3A compare artifacts to %s\n' "$OUT_DIR"
exit "$compare_status"
