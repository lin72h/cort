# CORT Subset 7F Validation Workflow

Date: 2026-04-17

This document records the workflow for the notify state-transition slice that
follows Subset 7E.

Like Subset 7A through 7E, this slice is driven by repo-local Swift-derived
corpora. MX is not the oracle.

## Inputs

- derived 7E notify outcome corpus:
  - `fixtures/control/subset7e_notify_client_outcome_expected_v1.json`
- 7F transition scenarios:
  - `fixtures/control/subset7f_notify_state_transition_cases_v1.json`
- Swift consumer source:
  - `../wip-swift-gpt54x/swift/Sources/LaunchXNotifyC/LaunchXNotifyC.swift`

## Derived Expected Corpus

Regenerate the 7F expected corpus with:

```sh
./tools/run_elixir.sh tools/build_subset7f_notify_transition_expected.exs
```

That rewrites:

- `fixtures/control/subset7f_notify_state_transition_expected_v1.json`

Regenerate the shared 7F C case header with:

```sh
./tools/run_elixir.sh tools/build_subset7f_notify_transition_cases_header.exs
```

That rewrites:

- `cort-fx/tests/generated/subset7f_notify_transition_cases.h`

## FX Probe

The FX notify transition probe emits:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7f_notify_state_transition_fx.json`

Generate it with:

```sh
cd cort-fx
make compare-subset7f-fx
```

Publish the shared repo artifact with:

```sh
cd cort-fx
make publish-subset7f-artifact
```

That updates:

- `subset7f_notify_state_transition_fx.json`

## Compare

FX compares the probe artifact against the expected corpus with:

```sh
cd cort-fx
make compare-subset7f-with-expected
```

The compare report is written to:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7f_notify_state_transition_fx_vs_expected_report.md`

The compare is blocking on:

- `validation_mode`
- `kind`
- `primary_value_text`
- `alternate_value_text`

## Acceptance Rule

Subset 7F is accepted when:

1. `make -C cort-fx test` passes
2. `make -C cort-fx compare-subset7f-with-expected` returns `0` blockers
3. `subset7f_notify_state_transition_fx.json` is published
4. `./tools/run_elixir.sh tools/workflow_selfcheck.exs` passes

## Operational Sequence

1. Regenerate the 7F expected corpus if the 7E outcome corpus or transition
   scenarios changed.
2. Regenerate the 7F generated C case header.
3. Run the FX notify transition probe.
4. Compare the FX artifact against the expected corpus.
5. Publish the shared repo artifact.
6. Only after a clean compare should downstream notify cache consumers depend
   on the new transition layer.

## Selfcheck Coverage

Subset 7F selfcheck coverage should prove:

- the expected-corpus builder regenerates the committed 7F expected corpus
- the generated-header builder regenerates the committed 7F case header
- the 7F compare tool reports `0` blockers on identical inputs
- the FX make compare wrapper emits the expected 7F compare report
- the shared 7F artifact stays in sync with the current build output
