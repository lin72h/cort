# CORT Subset 7D Validation Workflow

Date: 2026-04-17

This document records the workflow for the control response-profile slice that
follows Subset 7C.

Like Subset 7A/7B/7C, this slice is driven by the shared Swift packet corpus.

## Scope Of This Slice

- accepted response-profile classification
- bounded typed response-field extraction
- response-profile JSON/summary compare against the derived expected corpus

Explicitly excluded:

- malformed envelope rejection work
- packet encode parity
- dispatcher/service execution
- request/response correlation

## Shared Inputs

- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/subset7d_control_response_profile_expected_v1.json`

Tools:

- `tools/build_subset7d_control_response_profile_expected.exs`
- `tools/compare_subset7d_control_response_json.exs`

FX test/probe files:

- `cort-fx/tests/control_response_profile_tests.c`
- `cort-fx/tests/cort_subset7d_control_response_profile_fx.c`
- `cort-fx/tests/subset7d_control_response_profile_support.h`

## Expected-Corpus Build

Regenerate the 7D expected corpus with:

```sh
tools/run_elixir.sh tools/build_subset7d_control_response_profile_expected.exs
```

Default output:

- `fixtures/control/subset7d_control_response_profile_expected_v1.json`

## FX Probe Shape

The FX response-profile probe emits:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7d_control_response_profile_fx.json`

Shared handoff artifact:

- `../subset7d_control_response_profile_fx.json`

Local FX commands:

```sh
cd cort-fx
make compare-subset7d-fx
make compare-subset7d-with-expected
```

## FX Compare

FX compares the response-profile probe artifact against the derived expected
corpus with:

```sh
tools/run_elixir.sh tools/compare_subset7d_control_response_json.exs \
  --fx-json /path/to/subset7d_control_response_profile_fx.json \
  --expected-json fixtures/control/subset7d_control_response_profile_expected_v1.json \
  --output /path/to/subset7d_control_response_profile_fx_vs_expected_report.md
```

Default compare report output:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7d_control_response_profile_fx_vs_expected_report.md`

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

For `primary_value_text`, comparison is semantic JSON comparison rather than raw
key-order comparison.

## Workflow Selfcheck

Before relying on the new 7D workflow assets after workflow changes, run:

```sh
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

Subset 7D selfcheck coverage should prove:

- the expected-corpus builder regenerates the committed 7D expected corpus
- the 7D compare tool reports `0` blockers on identical inputs
- the FX make compare wrapper emits the expected 7D compare report

## Recommended Order

1. Confirm the shared response corpus.
2. Regenerate `subset7d_control_response_profile_expected_v1.json`.
3. Run the FX response-profile probe.
4. Compare the FX artifact against the expected response-profile corpus.
5. Publish `subset7d_control_response_profile_fx.json` as the shared handoff artifact.
6. Only after a clean compare should packet-facing consumers depend on the new
   response-profile layer.
