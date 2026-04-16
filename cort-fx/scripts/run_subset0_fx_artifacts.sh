#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FX_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$FX_DIR/.." && pwd)
REPO_PARENT=$(CDPATH= cd -- "$REPO_ROOT/.." && pwd)
REPO_NAME=$(basename "$REPO_ROOT")
ARTIFACTS_ROOT=${CORT_ARTIFACTS_ROOT:-"$REPO_PARENT/${REPO_NAME}-artifacts"}
BUILD_DIR=${BUILD_DIR:-"$ARTIFACTS_ROOT/cort-fx/build"}
STAGE_DIR=${STAGE_DIR:-"$BUILD_DIR/stage"}
OUT_DIR=${1:-"$ARTIFACTS_ROOT/cort-fx/runs/subset0-fx-local"}
case "$OUT_DIR" in
    /*) ;;
    *) OUT_DIR="$FX_DIR/$OUT_DIR" ;;
esac
TMP_DIR="${OUT_DIR}.tmp"
MAKE_CMD=${MAKE:-make}

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
mkdir -p "$TMP_DIR/out" "$TMP_DIR/bin" "$TMP_DIR/include/CoreFoundation" "$TMP_DIR/lib"

{
    printf 'cwd: %s\n' "$FX_DIR"
    printf 'artifacts_root: %s\n' "$ARTIFACTS_ROOT"
    printf 'build_dir: %s\n' "$BUILD_DIR"
    printf 'run_dir: %s\n' "$OUT_DIR"
    printf 'build: %s clean all check-deps check-exports BUILD_DIR=%s STAGE_DIR=%s\n' "$MAKE_CMD" "$BUILD_DIR" "$STAGE_DIR"
    printf 'test-installed: %s test-installed BUILD_DIR=%s STAGE_DIR=%s\n' "$MAKE_CMD" "$BUILD_DIR" "$STAGE_DIR"
    printf 'compare-fx: %s compare-fx BUILD_DIR=%s STAGE_DIR=%s\n' "$MAKE_CMD" "$BUILD_DIR" "$STAGE_DIR"
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
    if command -v python3 >/dev/null 2>&1; then
        python3 --version
    fi
} > "$TMP_DIR/toolchain.txt"

(
    cd "$FX_DIR"
    "$MAKE_CMD" clean all check-deps check-exports BUILD_DIR="$BUILD_DIR" STAGE_DIR="$STAGE_DIR" > "$TMP_DIR/out/build.stdout" 2> "$TMP_DIR/out/build.stderr"
    "$BUILD_DIR/bin/runtime_ownership_tests" > "$TMP_DIR/out/runtime_ownership_tests.stdout" 2> "$TMP_DIR/out/runtime_ownership_tests.stderr"
    "$BUILD_DIR/bin/runtime_abort_tests" > "$TMP_DIR/out/runtime_abort_tests.stdout" 2> "$TMP_DIR/out/runtime_abort_tests.stderr"
    "$BUILD_DIR/bin/c_consumer_smoke" > "$TMP_DIR/out/c_consumer_smoke.stdout" 2> "$TMP_DIR/out/c_consumer_smoke.stderr"
    "$MAKE_CMD" test-installed BUILD_DIR="$BUILD_DIR" STAGE_DIR="$STAGE_DIR" > "$TMP_DIR/out/test-installed.stdout" 2> "$TMP_DIR/out/test-installed.stderr"
    "$MAKE_CMD" compare-fx BUILD_DIR="$BUILD_DIR" STAGE_DIR="$STAGE_DIR" > "$TMP_DIR/out/compare-fx.stdout" 2> "$TMP_DIR/out/compare-fx.stderr"
)

cp "$FX_DIR/include/CoreFoundation/CFBase.h" "$TMP_DIR/include/CoreFoundation/CFBase.h"
cp "$FX_DIR/include/CoreFoundation/CFRuntime.h" "$TMP_DIR/include/CoreFoundation/CFRuntime.h"
cp "$FX_DIR/include/CoreFoundation/CoreFoundation.h" "$TMP_DIR/include/CoreFoundation/CoreFoundation.h"
cp "$BUILD_DIR/lib/libcortfx.a" "$TMP_DIR/lib/libcortfx.a"
cp "$BUILD_DIR/bin/runtime_ownership_tests" "$TMP_DIR/bin/runtime_ownership_tests"
cp "$BUILD_DIR/bin/runtime_abort_tests" "$TMP_DIR/bin/runtime_abort_tests"
cp "$BUILD_DIR/bin/c_consumer_smoke" "$TMP_DIR/bin/c_consumer_smoke"
cp "$BUILD_DIR/bin/subset0_public_compare_fx" "$TMP_DIR/bin/subset0_public_compare_fx"
cp "$BUILD_DIR/out/subset0_public_compare_fx.json" "$TMP_DIR/out/subset0_public_compare_fx.json"
cp "$BUILD_DIR/subset0-exported-symbols.txt" "$TMP_DIR/out/subset0-exported-symbols.actual.txt"
cp "$FX_DIR/exports/subset0-exported-symbols.txt" "$TMP_DIR/out/subset0-exported-symbols.expected.txt"

generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
cat > "$TMP_DIR/summary.md" <<EOF
# FX Subset 0 Local Run

Run directory: \`$OUT_DIR\`

Generated: \`$generated_at\`

Artifacts:

- \`host.txt\`
- \`toolchain.txt\`
- \`commands.txt\`
- \`out/build.stdout\`
- \`out/runtime_ownership_tests.stdout\`
- \`out/runtime_abort_tests.stdout\`
- \`out/c_consumer_smoke.stdout\`
- \`out/test-installed.stdout\`
- \`out/subset0_public_compare_fx.json\`
- \`out/subset0-exported-symbols.actual.txt\`
- \`out/subset0-exported-symbols.expected.txt\`
- \`bin/runtime_ownership_tests\`
- \`bin/runtime_abort_tests\`
- \`bin/c_consumer_smoke\`
- \`bin/subset0_public_compare_fx\`
- \`lib/libcortfx.a\`

Status:

- local build completed
- dependency and export checks completed
- ownership smoke tests passed
- abort-path tests passed
- installed-header consumer smoke passed
- FX allocator comparison JSON emitted
EOF

(
    cd "$TMP_DIR"
    find . -type f ! -name sha256.txt | sort | while IFS= read -r rel; do
        rel=${rel#./}
        hash_file "$TMP_DIR/$rel" "$rel"
    done
) > "$TMP_DIR/sha256.txt"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote FX run artifacts to %s\n' "$OUT_DIR"
