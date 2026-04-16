#!/bin/sh

set -eu

CORT_MX_SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CORT_MX_DIR=$(CDPATH= cd -- "$CORT_MX_SCRIPT_DIR/.." && pwd)
CORT_REPO_ROOT=$(CDPATH= cd -- "$CORT_MX_DIR/.." && pwd)
CORT_REPO_PARENT=$(CDPATH= cd -- "$CORT_REPO_ROOT/.." && pwd)
CORT_REPO_NAME=$(basename "$CORT_REPO_ROOT")
CORT_ARTIFACTS_ROOT=${CORT_ARTIFACTS_ROOT:-"$CORT_REPO_PARENT/${CORT_REPO_NAME}-artifacts"}

absolute_path() {
    path=$1
    case "$path" in
        /*) printf '%s\n' "$path" ;;
        *) printf '%s/%s\n' "$PWD" "$path" ;;
    esac
}

run_clang() {
    if command -v xcrun >/dev/null 2>&1; then
        xcrun --sdk macosx clang "$@"
        return
    fi
    clang "$@"
}

clang_command_string() {
    if command -v xcrun >/dev/null 2>&1; then
        printf '%s\n' 'xcrun --sdk macosx clang'
        return
    fi
    printf '%s\n' 'clang'
}

sdk_path() {
    if command -v xcrun >/dev/null 2>&1; then
        xcrun --sdk macosx --show-sdk-path
        return
    fi
    printf '%s\n' '<unknown>'
}

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

write_host_info() {
    output=$1
    {
        if command -v sw_vers >/dev/null 2>&1; then
            sw_vers
            printf '\n'
        fi
        uname -a
    } > "$output"
}

write_toolchain_info() {
    output=$1
    {
        printf 'clang command: %s\n' "$(clang_command_string)"
        printf 'sdk path: %s\n' "$(sdk_path)"
        printf '\n'
        run_clang --version
        printf '\n'
        if command -v python3 >/dev/null 2>&1; then
            python3 --version
        fi
    } > "$output"
}

write_sha256_manifest() {
    directory=$1
    (
        cd "$directory"
        find . -type f ! -name sha256.txt | sort | while IFS= read -r rel; do
            rel=${rel#./}
            hash_file "$directory/$rel" "$rel"
        done
    ) > "$directory/sha256.txt"
}
