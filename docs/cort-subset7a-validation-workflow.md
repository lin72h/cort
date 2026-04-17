# CORT Subset 7A Validation Workflow

Date: 2026-04-17

This document records the operational workflow for the first packet-facing CORT
slice.

Unlike the earlier subsets, Subset 7A does not use MX as the semantic oracle.
It uses the LaunchX Swift packet corpora that define the current
`ControlProtocol` contract.

## Scope Of This Slice

- current accepted control-packet corpus decode compatibility
- current rejected control-packet corpus rejection compatibility
- request vs response envelope shape
- exact top-level rejection family for the bounded rejection corpus

Explicitly excluded:

- packet encode parity
- transport/session behavior
- descriptor attachments
- dispatcher/service semantics after a packet has already decoded
- broad malformed nested-type fuzzing

## Shared Inputs

Repo-local imported corpora:

- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_corpus_v1.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.json`

Derived expected semantic corpus:

- `fixtures/control/subset7a_control_packet_expected_v1.json`

Tools:

- `tools/build_subset7a_control_packet_expected.exs`
- `tools/build_subset7a_control_packet_cases_header.exs`
- `tools/compare_subset7a_control_packet_json.exs`
- `tools/sync_subset7a_control_corpora.sh`

## Corpus Sync

Refresh the imported packet corpora from the Swift lane with:

```sh
tools/sync_subset7a_control_corpora.sh
```

Or explicitly:

```sh
tools/sync_subset7a_control_corpora.sh /path/to/swift/tests/fixtures/control
```

That copies the imported corpora into `fixtures/control/` and regenerates the
derived expected semantic corpus.

## Expected-Corpus Build

Regenerate the derived expected semantic corpus with:

```sh
tools/run_elixir.sh tools/build_subset7a_control_packet_expected.exs
```

Default output:

- `fixtures/control/subset7a_control_packet_expected_v1.json`

This build step is deterministic and should be rerun whenever the imported
source corpora change.

## FX Probe Shape

The FX packet probe emits:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7a_control_packet_fx.json`

Shared handoff artifact:

- `../subset7a_control_packet_fx.json`

The FX packet probe and packet tests use a generated header built from the
imported corpora and derived expected semantics:

- `tools/build_subset7a_control_packet_cases_header.exs`
- `cort-fx/tests/generated/subset7a_control_packet_cases.h`

The generated header is built from:

- `fixtures/control/bplist_packet_corpus_v1.json`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.json`
- `fixtures/control/subset7a_control_packet_expected_v1.json`

The FX packet tests and JSON probe live in:

- `cort-fx/tests/control_packet_tests.c`
- `cort-fx/tests/cort_subset7a_control_packet_fx.c`
- `cort-fx/tests/subset7a_control_packet_support.h`

The probe emits one result row per case in the format defined by:

- `fixtures/control/subset7a_control_packet_expected_v1.json`

Local FX commands:

```sh
cd cort-fx
make compare-subset7a-fx
make compare-subset7a-with-expected
```

## FX Compare

FX compares the packet probe artifact against the derived expected semantic
corpus with:

```sh
tools/run_elixir.sh tools/compare_subset7a_control_packet_json.exs \
  --fx-json /path/to/subset7a_control_packet_fx.json \
  --expected-json fixtures/control/subset7a_control_packet_expected_v1.json \
  --output /path/to/subset7a_control_packet_fx_vs_expected_report.md
```

Default compare report output:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7a_control_packet_fx_vs_expected_report.md`

## Comparison Rules

All rows are blocking in this slice.

Blocking mismatches:

- missing case on either side
- `success` mismatch
- mismatch on:
  - `validation_mode`
  - `kind`
  - `primary_value_text`
  - `alternate_value_text`

There is no diagnostic-only field set in this slice yet.

## Workflow Selfcheck

Before relying on the new Subset 7A workflow assets after workflow changes,
run:

```sh
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

Subset 7A selfcheck coverage should prove:

- the imported-source builder regenerates the committed expected corpus
- the packet-case header builder regenerates the committed generated header
- the 7A compare tool reports `0` blockers on identical inputs
- the FX make compare wrapper emits the expected packet compare report

## Recommended Order

1. Refresh or confirm the imported shared corpora in `fixtures/control/`.
2. Regenerate `subset7a_control_packet_expected_v1.json`.
3. Review the expected corpus diff if the corpora changed.
4. Regenerate `cort-fx/tests/generated/subset7a_control_packet_cases.h`.
5. Run the FX decode-only packet probe.
6. Compare the FX probe output against the expected corpus.
7. Only after a clean compare should the packet lane consider encode parity or
   transport integration work.
