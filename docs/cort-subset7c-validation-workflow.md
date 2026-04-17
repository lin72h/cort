# CORT Subset 7C Validation Workflow

Date: 2026-04-17

This document records the operational workflow for the control request-route
slice that follows Subset 7B.

Like Subset 7A and 7B, this slice is Swift-corpus-driven and does not use MX
as the semantic oracle.

## Scope Of This Slice

- request-route classification for accepted request packets
- typed known-parameter extraction for accepted request packets
- request-side rejection preservation for the bounded malformed request corpus

Explicitly excluded:

- response-side consumer work
- packet encode parity
- dispatcher/service execution
- transport/session behavior

## Shared Inputs

- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/subset7c_control_request_route_expected_v1.json`

Tools:

- `tools/build_subset7c_control_request_route_expected.exs`
- `tools/compare_subset7c_control_request_json.exs`

FX test/probe files:

- `cort-fx/tests/control_request_route_tests.c`
- `cort-fx/tests/cort_subset7c_control_request_route_fx.c`
- `cort-fx/tests/subset7c_control_request_route_support.h`

## Expected-Corpus Build

Regenerate the 7C expected request-route corpus with:

```sh
tools/run_elixir.sh tools/build_subset7c_control_request_route_expected.exs
```

Default output:

- `fixtures/control/subset7c_control_request_route_expected_v1.json`

## FX Probe Shape

The FX request-route probe emits:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7c_control_request_route_fx.json`

Shared handoff artifact:

- `../subset7c_control_request_route_fx.json`

Local FX commands:

```sh
cd cort-fx
make compare-subset7c-fx
make compare-subset7c-with-expected
```

## FX Compare

FX compares the request-route probe artifact against the derived expected corpus
with:

```sh
tools/run_elixir.sh tools/compare_subset7c_control_request_json.exs \
  --fx-json /path/to/subset7c_control_request_route_fx.json \
  --expected-json fixtures/control/subset7c_control_request_route_expected_v1.json \
  --output /path/to/subset7c_control_request_route_fx_vs_expected_report.md
```

Default compare report output:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7c_control_request_route_fx_vs_expected_report.md`

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

## Workflow Selfcheck

Before relying on the new 7C workflow assets after workflow changes, run:

```sh
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

Subset 7C selfcheck coverage should prove:

- the expected-corpus builder regenerates the committed 7C expected corpus
- the 7C compare tool reports `0` blockers on identical inputs
- the FX make compare wrapper emits the expected 7C compare report

## Recommended Order

1. Confirm the shared request/rejection corpora.
2. Regenerate `subset7c_control_request_route_expected_v1.json`.
3. Run the FX request-route probe.
4. Compare the FX artifact against the expected request-route corpus.
5. Publish `subset7c_control_request_route_fx.json` as the shared handoff artifact.
6. Only after a clean compare should packet consumers depend on the new route layer.
