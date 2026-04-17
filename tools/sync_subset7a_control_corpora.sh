#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SOURCE_DIR=${1:-"$REPO_ROOT/../wip-swift-gpt54x/tests/fixtures/control"}
DEST_DIR="$REPO_ROOT/fixtures/control"

mkdir -p "$DEST_DIR"

for name in \
    bplist_packet_corpus_v1.json \
    bplist_packet_corpus_v1.source.json \
    bplist_packet_rejection_corpus_v1.json \
    bplist_packet_rejection_corpus_v1.source.json
do
    cp "$SOURCE_DIR/$name" "$DEST_DIR/$name"
done

"$REPO_ROOT/tools/run_elixir.sh" "$REPO_ROOT/tools/build_subset7a_control_packet_expected.exs"
