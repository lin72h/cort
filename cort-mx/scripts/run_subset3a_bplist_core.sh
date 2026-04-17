#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/common.sh"

OUT_DIR=${1:-"$CORT_ARTIFACTS_ROOT/cort-mx/runs/subset3a-mx-bplist-core"}
OUT_DIR=$(absolute_path "$OUT_DIR")
TMP_DIR="${OUT_DIR}.tmp"
SOURCE_FILE="$CORT_MX_DIR/src/cort_mx_subset3a_bplist_core.c"
EXPECTATIONS_FILE="$CORT_MX_DIR/expectations/subset3a_bplist_core_expected.json"
REPORT_TOOL="$CORT_REPO_ROOT/tools/report_case_manifest.exs"
ELIXIR_RUNNER="$CORT_REPO_ROOT/tools/run_elixir.sh"
BIN_NAME="cort_mx_subset3a_bplist_core"
JSON_NAME="subset3a_bplist_core.json"
REPORT_TITLE="Subset 3A Binary Plist Core Report"

COMPILE_CMD="$(clang_command_string) -Wall -Wextra -Werror -framework CoreFoundation src/$BIN_NAME.c -o bin/$BIN_NAME"
RUN_CMD="./bin/$BIN_NAME fixtures > out/$JSON_NAME"

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/src" "$TMP_DIR/bin" "$TMP_DIR/out" "$TMP_DIR/fixtures"
cp "$SOURCE_FILE" "$TMP_DIR/src/$BIN_NAME.c"

write_host_info "$TMP_DIR/host.txt"
write_toolchain_info "$TMP_DIR/toolchain.txt"
{
    printf 'compile: %s\n' "$COMPILE_CMD"
    printf 'run: %s\n' "$RUN_CMD"
    printf 'report: %s %s --title %s --json out/%s --json-label out/%s --expected %s --expected-label expectations/%s --output summary.md\n' "$ELIXIR_RUNNER" "$REPORT_TOOL" "$REPORT_TITLE" "$JSON_NAME" "$JSON_NAME" "$EXPECTATIONS_FILE" "$(basename "$EXPECTATIONS_FILE")"
} > "$TMP_DIR/commands.txt"

(
    cd "$TMP_DIR"
    run_clang -Wall -Wextra -Werror -framework CoreFoundation "src/$BIN_NAME.c" -o "bin/$BIN_NAME" > "out/build.stdout" 2> "out/build.stderr"
    "bin/$BIN_NAME" "fixtures" > "out/$JSON_NAME" 2> "out/subset3a_bplist_core.stderr"
    cp "out/$JSON_NAME" "out/subset3a_bplist_core.stdout"
)

"$ELIXIR_RUNNER" "$REPORT_TOOL" \
    --title "$REPORT_TITLE" \
    --json "$TMP_DIR/out/$JSON_NAME" \
    --json-label "out/$JSON_NAME" \
    --expected "$EXPECTATIONS_FILE" \
    --expected-label "expectations/$(basename "$EXPECTATIONS_FILE")" \
    --output "$TMP_DIR/summary.md" \
    > "$TMP_DIR/out/report.stdout" \
    2> "$TMP_DIR/out/report.stderr" || report_status=$?

: "${report_status:=0}"

write_sha256_manifest "$TMP_DIR"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote MX Subset 3A binary plist run artifacts to %s\n' "$OUT_DIR"
exit "$report_status"
