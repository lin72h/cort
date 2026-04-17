# Control Packet Corpora

This directory holds the repo-local shared control-packet corpora used by the
CORT packet-compatibility lane.

Files:

- `bplist_packet_corpus_v1.source.json`
- `bplist_packet_corpus_v1.json`
- `bplist_packet_rejection_corpus_v1.source.json`
- `bplist_packet_rejection_corpus_v1.json`
- `subset7a_control_packet_expected_v1.json`
- `subset7b_control_envelope_expected_v1.json`

Provenance:

- the first four files are imported from
  `../wip-swift-gpt54x/tests/fixtures/control/`
- the source of truth for packet semantics remains the Swift lane:
  `swift/Sources/LaunchXControl/ControlProtocol.swift`
- this repo carries copies because it is the FX/MX coordination channel and the
  packet-compatibility compare surface needs a stable local artifact

Do not hand-edit the imported corpora.

Refresh them with:

```sh
tools/sync_subset7a_control_corpora.sh
```

Or explicitly:

```sh
tools/sync_subset7a_control_corpora.sh /path/to/swift/tests/fixtures/control
```

`subset7a_control_packet_expected_v1.json` is derived from the imported source
corpora by:

- `tools/build_subset7a_control_packet_expected.exs`

That file is the semantic expected-corpus used by the future FX packet probe
compare workflow. It is generated from the source corpora and should not be
hand-edited.

`subset7b_control_envelope_expected_v1.json` is derived from the same imported
source corpora by:

- `tools/build_subset7b_control_envelope_expected.exs`

That file carries the internal typed-envelope expected surface for the next
packet-facing FX slice. It is generated from the source corpora and should not
be hand-edited.
