#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/common.sh"

OUT_DIR=${1:-"$CORT_ARTIFACTS_ROOT/cort-mx/runs/subset0-mx-public-allocator-compare"}
OUT_DIR=$(absolute_path "$OUT_DIR")
TMP_DIR="${OUT_DIR}.tmp"
SOURCE_FILE="$CORT_MX_DIR/src/cort_subset0_public_allocator_compare.c"
COMPARE_TOOL="$CORT_REPO_ROOT/tools/compare_subset0_json.py"
BIN_NAME="cort_subset0_public_allocator_compare"
JSON_NAME="subset0_public_compare_mx.json"

COMPILE_CMD="$(clang_command_string) -Wall -Wextra -Werror -framework CoreFoundation src/$BIN_NAME.c -o bin/$BIN_NAME"
RUN_CMD="./bin/$BIN_NAME > out/$JSON_NAME"

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/src" "$TMP_DIR/bin" "$TMP_DIR/out"
cp "$SOURCE_FILE" "$TMP_DIR/src/$BIN_NAME.c"

write_host_info "$TMP_DIR/host.txt"
write_toolchain_info "$TMP_DIR/toolchain.txt"
{
    printf 'compile: %s\n' "$COMPILE_CMD"
    printf 'run: %s\n' "$RUN_CMD"
    if [ -n "${FX_JSON:-}" ]; then
        printf 'compare: python3 %s --fx-json %s --mx-json out/%s --output out/subset0_public_compare_report.md\n' "$COMPARE_TOOL" "$FX_JSON" "$JSON_NAME"
    fi
} > "$TMP_DIR/commands.txt"

(
    cd "$TMP_DIR"
    run_clang -Wall -Wextra -Werror -framework CoreFoundation "src/$BIN_NAME.c" -o "bin/$BIN_NAME" > "out/build.stdout" 2> "out/build.stderr"
    "bin/$BIN_NAME" > "out/$JSON_NAME" 2> "out/subset0_public_compare_mx.stderr"
    cp "out/$JSON_NAME" "out/subset0_public_compare_mx.stdout"
)

if [ -n "${FX_JSON:-}" ]; then
    python3 "$COMPARE_TOOL" \
        --fx-json "$FX_JSON" \
        --mx-json "$TMP_DIR/out/$JSON_NAME" \
        --output "$TMP_DIR/out/subset0_public_compare_report.md" \
        > "$TMP_DIR/out/report.stdout" \
        2> "$TMP_DIR/out/report.stderr" || compare_status=$?
    : "${compare_status:=0}"
    cp "$TMP_DIR/out/subset0_public_compare_report.md" "$TMP_DIR/summary.md"
else
    generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    cat > "$TMP_DIR/summary.md" <<EOF
# MX Subset 0 Public Allocator Compare

Generated: \`$generated_at\`

Artifacts:

- \`host.txt\`
- \`toolchain.txt\`
- \`commands.txt\`
- \`src/$BIN_NAME.c\`
- \`bin/$BIN_NAME\`
- \`out/$JSON_NAME\`
- \`out/subset0_public_compare_mx.stdout\`
- \`out/subset0_public_compare_mx.stderr\`

Comparison:

- no FX JSON was provided to this script
- run \`python3 $COMPARE_TOOL --fx-json /path/to/fx.json --mx-json out/$JSON_NAME --output out/subset0_public_compare_report.md\` later to compare against FX
EOF
fi

write_sha256_manifest "$TMP_DIR"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote MX public allocator compare run artifacts to %s\n' "$OUT_DIR"
exit "${compare_status:=0}"
