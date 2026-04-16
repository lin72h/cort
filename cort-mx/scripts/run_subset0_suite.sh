#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/common.sh"

OUT_DIR=${1:-"$CORT_ARTIFACTS_ROOT/cort-mx/runs/subset0-mx-suite"}
OUT_DIR=$(absolute_path "$OUT_DIR")
TMP_DIR="${OUT_DIR}.tmp"

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

runtime_status=0
if "$SCRIPT_DIR/run_subset0_runtime_ownership.sh" "$TMP_DIR/runtime-ownership"; then
    runtime_status=0
else
    runtime_status=$?
fi

allocator_status=0
if [ -n "${FX_JSON:-}" ]; then
    FX_JSON_ABS=$(absolute_path "$FX_JSON")
    if FX_JSON="$FX_JSON_ABS" "$SCRIPT_DIR/run_subset0_public_allocator_compare.sh" "$TMP_DIR/public-allocator-compare"; then
        allocator_status=0
    else
        allocator_status=$?
    fi
else
    if "$SCRIPT_DIR/run_subset0_public_allocator_compare.sh" "$TMP_DIR/public-allocator-compare"; then
        allocator_status=0
    else
        allocator_status=$?
    fi
fi

generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
cat > "$TMP_DIR/summary.md" <<EOF
# MX Subset 0 Suite

Generated: \`$generated_at\`

Sub-runs:

- \`runtime-ownership/\`
- \`public-allocator-compare/\`

Notes:

- runtime-ownership summary is in \`runtime-ownership/summary.md\`
- public allocator comparison summary is in \`public-allocator-compare/summary.md\`
- this suite wrapper preserves both sub-run directories under one root for handoff
- runtime-ownership exit status: \`$runtime_status\`
- public-allocator-compare exit status: \`$allocator_status\`
EOF

write_sha256_manifest "$TMP_DIR"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote MX Subset 0 suite artifacts to %s\n' "$OUT_DIR"
if [ "$runtime_status" -ne 0 ] && [ "$allocator_status" -ne 0 ]; then
    exit "$runtime_status"
fi
if [ "$runtime_status" -ne 0 ]; then
    exit "$runtime_status"
fi
exit "$allocator_status"
