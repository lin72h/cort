#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/common.sh"

OUT_DIR=${1:-"$CORT_ARTIFACTS_ROOT/cort-mx/runs/subset1b-mx-cfstring-core-compare"}
OUT_DIR=$(absolute_path "$OUT_DIR")
TMP_DIR="${OUT_DIR}.tmp"
COMPARE_TOOL="$CORT_REPO_ROOT/tools/compare_subset1b_cfstring_json.exs"
ELIXIR_RUNNER="$CORT_REPO_ROOT/tools/run_elixir.sh"
FX_JSON=${FX_JSON:-${FX_CFSTRING_JSON:-"$CORT_REPO_ROOT/subset1b_cfstring_fx.json"}}
MX_JSON=${MX_JSON:-${MX_CFSTRING_JSON:-"$CORT_ARTIFACTS_ROOT/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json"}}

case "$FX_JSON" in
    /*) ;;
    *) FX_JSON=$(absolute_path "$FX_JSON") ;;
esac

case "$MX_JSON" in
    /*) ;;
    *) MX_JSON=$(absolute_path "$MX_JSON") ;;
esac

if [ ! -f "$FX_JSON" ]; then
    printf 'missing FX JSON: %s\n' "$FX_JSON" >&2
    exit 1
fi

if [ ! -f "$MX_JSON" ]; then
    printf 'missing MX JSON: %s\n' "$MX_JSON" >&2
    exit 1
fi

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/out"

write_host_info "$TMP_DIR/host.txt"
write_toolchain_info "$TMP_DIR/toolchain.txt"
{
    printf 'compare: %s %s --fx-json out/subset1b_cfstring_fx.json --fx-label out/subset1b_cfstring_fx.json --mx-json out/subset1b_cfstring_mx.json --mx-label out/subset1b_cfstring_mx.json --output out/subset1b_cfstring_fx_vs_mx_report.md\n' "$ELIXIR_RUNNER" "$COMPARE_TOOL"
    printf 'source fx json: %s\n' "$FX_JSON"
    printf 'source mx json: %s\n' "$MX_JSON"
} > "$TMP_DIR/commands.txt"

cp "$FX_JSON" "$TMP_DIR/out/subset1b_cfstring_fx.json"
cp "$MX_JSON" "$TMP_DIR/out/subset1b_cfstring_mx.json"

"$ELIXIR_RUNNER" "$COMPARE_TOOL" \
    --fx-json "$TMP_DIR/out/subset1b_cfstring_fx.json" \
    --fx-label "out/subset1b_cfstring_fx.json" \
    --mx-json "$TMP_DIR/out/subset1b_cfstring_mx.json" \
    --mx-label "out/subset1b_cfstring_mx.json" \
    --output "$TMP_DIR/out/subset1b_cfstring_fx_vs_mx_report.md" \
    > "$TMP_DIR/out/compare.stdout" \
    2> "$TMP_DIR/out/compare.stderr" || compare_status=$?

: "${compare_status:=0}"

cp "$TMP_DIR/out/subset1b_cfstring_fx_vs_mx_report.md" "$TMP_DIR/summary.md"
write_sha256_manifest "$TMP_DIR"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote MX Subset 1B compare artifacts to %s\n' "$OUT_DIR"
exit "$compare_status"
