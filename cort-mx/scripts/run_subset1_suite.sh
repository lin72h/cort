#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/common.sh"

OUT_DIR=${1:-"$CORT_ARTIFACTS_ROOT/cort-mx/runs/subset1-mx-suite"}
OUT_DIR=$(absolute_path "$OUT_DIR")
TMP_DIR="${OUT_DIR}.tmp"

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

scalar_status=0
if "$SCRIPT_DIR/run_subset1_scalar_core.sh" "$TMP_DIR/scalar-core"; then
    scalar_status=0
else
    scalar_status=$?
fi

compare_status=0
MX_SUITE_JSON="$TMP_DIR/scalar-core/out/subset1_scalar_core.json"
if [ -f "$MX_SUITE_JSON" ]; then
    if [ -n "${FX_JSON:-}" ]; then
        FX_JSON_ABS=$(absolute_path "$FX_JSON")
        if FX_JSON="$FX_JSON_ABS" MX_JSON="$MX_SUITE_JSON" "$SCRIPT_DIR/run_subset1_scalar_core_compare.sh" "$TMP_DIR/scalar-core-compare"; then
            compare_status=0
        else
            compare_status=$?
        fi
    else
        if MX_JSON="$MX_SUITE_JSON" "$SCRIPT_DIR/run_subset1_scalar_core_compare.sh" "$TMP_DIR/scalar-core-compare"; then
            compare_status=0
        else
            compare_status=$?
        fi
    fi
else
    compare_status=1
    mkdir -p "$TMP_DIR/scalar-core-compare/out"
    write_host_info "$TMP_DIR/scalar-core-compare/host.txt"
    write_toolchain_info "$TMP_DIR/scalar-core-compare/toolchain.txt"
    {
        printf '%s\n' 'compare: skipped because scalar-core/out/subset1_scalar_core.json was not produced'
    } > "$TMP_DIR/scalar-core-compare/commands.txt"

    generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    cat > "$TMP_DIR/scalar-core-compare/summary.md" <<EOF
# MX Subset 1A Scalar Core Compare

Generated: \`$generated_at\`

Comparison:

- skipped because \`scalar-core/out/subset1_scalar_core.json\` was not produced
- rerun \`scripts/run_subset1_scalar_core.sh\` and then rerun this suite
EOF

    write_sha256_manifest "$TMP_DIR/scalar-core-compare"
fi

generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
cat > "$TMP_DIR/summary.md" <<EOF
# MX Subset 1A Suite

Generated: \`$generated_at\`

Sub-runs:

- \`scalar-core/\`
- \`scalar-core-compare/\`

Notes:

- scalar-core summary is in \`scalar-core/summary.md\`
- scalar-core compare summary is in \`scalar-core-compare/summary.md\`
- the compare step defaults to the shared in-repo FX artifact \`subset1_scalar_core_fx.json\`
- scalar-core exit status: \`$scalar_status\`
- scalar-core-compare exit status: \`$compare_status\`
EOF

write_sha256_manifest "$TMP_DIR"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote MX Subset 1A suite artifacts to %s\n' "$OUT_DIR"
if [ "$scalar_status" -ne 0 ] && [ "$compare_status" -ne 0 ]; then
    exit "$scalar_status"
fi
if [ "$scalar_status" -ne 0 ]; then
    exit "$scalar_status"
fi
exit "$compare_status"
