#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FX_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if [ "$#" -ne 2 ]; then
    printf '%s\n' 'usage: publish_shared_json.sh SOURCE_JSON DEST_JSON' >&2
    exit 1
fi

SOURCE_JSON=$1
DEST_JSON=$2

case "$SOURCE_JSON" in
    /*) ;;
    *) SOURCE_JSON="$FX_DIR/$SOURCE_JSON" ;;
esac

case "$DEST_JSON" in
    /*) ;;
    *) DEST_JSON="$FX_DIR/$DEST_JSON" ;;
esac

if [ ! -f "$SOURCE_JSON" ]; then
    printf 'missing source JSON: %s\n' "$SOURCE_JSON" >&2
    exit 1
fi

DEST_DIR=$(dirname "$DEST_JSON")
mkdir -p "$DEST_DIR"

if [ -f "$DEST_JSON" ] && cmp -s "$SOURCE_JSON" "$DEST_JSON"; then
    printf 'Shared handoff artifact already current: %s\n' "$DEST_JSON"
    exit 0
fi

TMP_DEST=$(mktemp "$DEST_DIR/.publish_shared_json.XXXXXX")
cp "$SOURCE_JSON" "$TMP_DEST"
mv "$TMP_DEST" "$DEST_JSON"
printf 'Published shared handoff artifact to %s\n' "$DEST_JSON"
