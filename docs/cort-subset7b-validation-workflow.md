# CORT Subset 7B Validation Workflow

Date: 2026-04-17

This document records the operational workflow for the internal control-envelope
slice that follows Subset 7A.

Like Subset 7A, this slice does not use MX as the semantic oracle. It uses the
shared Swift packet corpora and the derived expected envelope corpus.

## Scope Of This Slice

- request envelope decode/accessor compatibility
- response envelope decode/accessor compatibility
- envelope JSON/summary compare against the shared corpora
- preservation of the bounded 7A rejection surface

Explicitly excluded:

- packet encode parity
- dispatcher/service execution
- transport/session behavior
- broad packet mutation APIs
- broad malformed nested-payload fuzzing

## Shared Inputs

Repo-local imported corpora:

- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_corpus_v1.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.json`

Derived expected corpora:

- `fixtures/control/subset7a_control_packet_expected_v1.json`
- `fixtures/control/subset7b_control_envelope_expected_v1.json`

Tools:

- `tools/build_subset7b_control_envelope_expected.exs`
- `tools/compare_subset7b_control_envelope_json.exs`

FX test/probe files:

- `cort-fx/tests/control_envelope_tests.c`
- `cort-fx/tests/cort_subset7b_control_envelope_fx.c`
- `cort-fx/tests/subset7b_control_envelope_support.h`

## Expected-Corpus Build

Regenerate the 7B expected envelope corpus with:

```sh
tools/run_elixir.sh tools/build_subset7b_control_envelope_expected.exs
```

Default output:

- `fixtures/control/subset7b_control_envelope_expected_v1.json`

This build step is deterministic and should be rerun whenever the shared Swift
packet corpora change.

## FX Probe Shape

The FX envelope probe emits:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7b_control_envelope_fx.json`

Shared handoff artifact:

- `../subset7b_control_envelope_fx.json`

The probe emits one result row per packet case in the format defined by:

- `fixtures/control/subset7b_control_envelope_expected_v1.json`

Local FX commands:

```sh
cd cort-fx
make compare-subset7b-fx
make compare-subset7b-with-expected
```

## FX Compare

FX compares the envelope probe artifact against the derived expected corpus
with:

```sh
tools/run_elixir.sh tools/compare_subset7b_control_envelope_json.exs \
  --fx-json /path/to/subset7b_control_envelope_fx.json \
  --expected-json fixtures/control/subset7b_control_envelope_expected_v1.json \
  --output /path/to/subset7b_control_envelope_fx_vs_expected_report.md
```

Default compare report output:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7b_control_envelope_fx_vs_expected_report.md`

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

There is no diagnostic-only field set in this slice.

## Workflow Selfcheck

Before relying on the new 7B workflow assets after workflow changes, run:

```sh
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

Subset 7B selfcheck coverage should prove:

- the expected-corpus builder regenerates the committed 7B expected corpus
- the 7B compare tool reports `0` blockers on identical inputs
- the FX make compare wrapper emits the expected 7B compare report

## Recommended Order

1. Confirm or refresh the imported packet corpora under `fixtures/control/`.
2. Regenerate `subset7b_control_envelope_expected_v1.json`.
3. Review the expected-corpus diff if the corpora changed.
4. Run the FX envelope probe.
5. Compare the FX envelope artifact against the expected corpus.
6. Publish `subset7b_control_envelope_fx.json` as the shared handoff artifact.
7. Only after a clean compare should packet consumers depend on the new
   internal envelope layer.
