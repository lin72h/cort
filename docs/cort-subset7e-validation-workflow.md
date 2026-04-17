# CORT Subset 7E Validation Workflow

Date: 2026-04-17

This document records the workflow for the notify-client outcome slice that
follows Subset 7D.

Like Subset 7A through 7D, this slice is driven by the shared Swift packet
corpus. MX is not the oracle.

## Inputs

- shared source corpus:
  - `fixtures/control/bplist_packet_corpus_v1.source.json`
- derived expected corpus:
  - `fixtures/control/subset7e_notify_client_outcome_expected_v1.json`
- Swift consumer source:
  - `../wip-swift-gpt54x/swift/Sources/LaunchXNotifyC/LaunchXNotifyC.swift`

## Derived Expected Corpus

Regenerate the 7E expected corpus with:

```sh
./tools/run_elixir.sh tools/build_subset7e_notify_client_expected.exs
```

That rewrites:

- `fixtures/control/subset7e_notify_client_outcome_expected_v1.json`

The expected corpus filters the shared response corpus down to the bounded
notify-client rows and derives:

- outcome kind
- notify status family
- bounded extracted success fields
- canonical summary text

## FX Probe

The FX notify-client probe emits:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7e_notify_client_outcome_fx.json`

Generate it with:

```sh
cd cort-fx
make compare-subset7e-fx
```

Publish the shared repo artifact with:

```sh
cd cort-fx
make publish-subset7e-artifact
```

That updates:

- `subset7e_notify_client_outcome_fx.json`

## Compare

FX compares the probe artifact against the expected corpus with:

```sh
cd cort-fx
make compare-subset7e-with-expected
```

The compare report is written to:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7e_notify_client_outcome_fx_vs_expected_report.md`

The compare is blocking on:

- `validation_mode`
- `kind`
- `primary_value_text`
- `alternate_value_text`

## Acceptance Rule

Subset 7E is accepted when:

1. `make -C cort-fx test` passes
2. `make -C cort-fx compare-subset7e-with-expected` returns `0` blockers
3. `subset7e_notify_client_outcome_fx.json` is published
4. `./tools/run_elixir.sh tools/workflow_selfcheck.exs` passes

## Operational Sequence

1. Regenerate the expected corpus if the shared Swift corpus changed.
2. Run the FX notify outcome probe.
3. Compare the FX artifact against the expected corpus.
4. Publish the shared repo artifact.
5. Only after a clean compare should downstream notify-client consumers depend
   on the new outcome layer.

## Selfcheck Coverage

Subset 7E selfcheck coverage should prove:

- the expected-corpus builder regenerates the committed 7E expected corpus
- the 7E compare tool reports `0` blockers on identical inputs
- the FX make compare wrapper emits the expected 7E compare report
- the shared 7E artifact stays in sync with the current build output
