#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/common.sh"

OUT_DIR=${1:-"$CORT_ARTIFACTS_ROOT/cort-mx/runs/subset2a-mx-suite"}
OUT_DIR=$(absolute_path "$OUT_DIR")
TMP_DIR="${OUT_DIR}.tmp"
DEFAULT_FX_JSON="$CORT_REPO_ROOT/subset2a_container_fx.json"

write_compare_stub() {
    stub_dir=$1
    command_note=$2
    line_one=$3
    line_two=$4

    mkdir -p "$stub_dir/out"
    write_host_info "$stub_dir/host.txt"
    write_toolchain_info "$stub_dir/toolchain.txt"
    {
        printf 'compare: %s\n' "$command_note"
    } > "$stub_dir/commands.txt"

    generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    cat > "$stub_dir/summary.md" <<EOF
# MX Subset 2A Container Compare

Generated: \`$generated_at\`

Comparison:

- $line_one
- $line_two
EOF

    write_sha256_manifest "$stub_dir"
}

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

container_status=0
if "$SCRIPT_DIR/run_subset2a_container_core.sh" "$TMP_DIR/container-core"; then
    container_status=0
else
    container_status=$?
fi

compare_status=0
compare_status_label=0
compare_failed=0

MX_SUITE_JSON="$TMP_DIR/container-core/out/subset2a_container_core.json"
COMPARE_DIR="$TMP_DIR/container-core-compare"
if [ -f "$MX_SUITE_JSON" ]; then
    fx_json_input=
    if [ -n "${FX_JSON:-}" ]; then
        fx_json_input=$FX_JSON
    elif [ -n "${FX_CONTAINER_JSON:-}" ]; then
        fx_json_input=$FX_CONTAINER_JSON
    elif [ -f "$DEFAULT_FX_JSON" ]; then
        fx_json_input=$DEFAULT_FX_JSON
    fi

    if [ -n "$fx_json_input" ]; then
        FX_JSON_ABS=$(absolute_path "$fx_json_input")
        if [ -f "$FX_JSON_ABS" ]; then
            if FX_JSON="$FX_JSON_ABS" MX_JSON="$MX_SUITE_JSON" "$SCRIPT_DIR/run_subset2a_container_compare.sh" "$COMPARE_DIR"; then
                compare_status=0
                compare_status_label=0
            else
                compare_status=$?
                compare_status_label=$compare_status
                compare_failed=1
            fi
        else
            compare_status=1
            compare_status_label=$compare_status
            compare_failed=1
            write_compare_stub \
                "$COMPARE_DIR" \
                "failed because provided FX JSON was not found: $FX_JSON_ABS" \
                "failed because the requested FX JSON was not found: \`$FX_JSON_ABS\`" \
                "rerun with a real \`FX_JSON=/path/to/subset2a_container_fx.json\` or publish the shared in-repo \`subset2a_container_fx.json\`"
        fi
    else
        compare_status=0
        compare_status_label=skipped
        write_compare_stub \
            "$COMPARE_DIR" \
            "skipped because no FX JSON was provided and $DEFAULT_FX_JSON is not present" \
            "skipped because no FX JSON was provided and the shared in-repo \`subset2a_container_fx.json\` is not present" \
            "publish \`subset2a_container_fx.json\` or rerun with \`FX_JSON=/path/to/subset2a_container_fx.json\`"
    fi
else
    compare_status=1
    compare_status_label=$compare_status
    compare_failed=1
    write_compare_stub \
        "$COMPARE_DIR" \
        "skipped because container-core/out/subset2a_container_core.json was not produced" \
        "skipped because \`container-core/out/subset2a_container_core.json\` was not produced" \
        "rerun \`scripts/run_subset2a_container_core.sh\` and then rerun this suite"
fi

generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
cat > "$TMP_DIR/summary.md" <<EOF
# MX Subset 2A Suite

Generated: \`$generated_at\`

Sub-runs:

- \`container-core/\`
- \`container-core-compare/\`

Notes:

- container core summary is in \`container-core/summary.md\`
- container compare summary is in \`container-core-compare/summary.md\`
- the compare step defaults to the shared in-repo FX artifact \`subset2a_container_fx.json\` when present
- set \`FX_JSON=/path/to/subset2a_container_fx.json\` to compare against an explicit FX artifact
- container-core exit status: \`$container_status\`
- container-core-compare exit status: \`$compare_status_label\`
EOF

write_sha256_manifest "$TMP_DIR"

rm -rf "$OUT_DIR"
mv "$TMP_DIR" "$OUT_DIR"
printf 'Wrote MX Subset 2A suite artifacts to %s\n' "$OUT_DIR"

if [ "$container_status" -ne 0 ]; then
    exit "$container_status"
fi
if [ "$compare_failed" -ne 0 ]; then
    exit "$compare_status"
fi
exit 0
