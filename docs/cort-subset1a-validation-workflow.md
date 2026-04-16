# CORT Subset 1A Validation Workflow

Date: 2026-04-16

This document records the current operational workflow for validating the FX
Subset 1A scalar-core implementation against the bounded MX scalar-core run.

Scope of this slice:

- `CFBoolean`
- `CFData`
- `CFNumber`
- `CFDate`

Explicitly excluded:

- `CFString`
- containers
- plist and bplist
- Objective-C bridge behavior
- broad CoreFoundation archaeology

## Current State

MX status:

- `subset1_scalar_core` completed with `0` blockers and `0` warnings
- artifact path:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json`

FX status:

- local Subset 1A implementation exists in `cort-fx`
- `make test` passes
- local FX probe emits:
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx.json`

## Local FX Build And Probe

From:

- `/Users/me/wip-launchx/wip-cort-gpt/cort-fx`

Run:

```sh
make clean test
make test-installed
make compare-subset1a-fx
```

Primary FX outputs:

- `../wip-cort-gpt-artifacts/cort-fx/build/lib/libcortfx.a`
- `../wip-cort-gpt-artifacts/cort-fx/build/bin/scalar_core_tests`
- `../wip-cort-gpt-artifacts/cort-fx/build/bin/subset1_scalar_core_fx`
- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx.json`
- `../wip-cort-gpt-artifacts/cort-fx/build/subset1a-exported-symbols.txt`

## MX Run

From:

- `/Users/me/wip-launchx/wip-cort-gpt/cort-mx`

Run:

```sh
scripts/run_subset1_scalar_core.sh
```

If the shared artifact root should match the coordination layout:

```sh
CORT_ARTIFACTS_ROOT=/Users/me/wip-launchx/wip-cort-gpt-artifacts \
  scripts/run_subset1_scalar_core.sh
```

Primary MX outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json`

Observed MX result:

- blockers: `0`
- warnings: `0`
- `cfnumber_cross_type_equality` observed `equal_same_value: true`
- `cfnumber_cross_type_equality` observed `equal_different_value: true`
- `hash_primary == hash_same_value == hash_different_value` for the tested
  `42` row

## FX vs MX Comparison

Compare the emitted FX JSON against the MX JSON with:

- `tools/compare_subset1_scalar_core_json.exs`
- `cd cort-fx && make compare-subset1a-with-mx ...`

Direct invocation:

```sh
/Users/me/wip-launchx/wip-cort-gpt/tools/run_elixir.sh \
  /Users/me/wip-launchx/wip-cort-gpt/tools/compare_subset1_scalar_core_json.exs \
  --fx-json /path/to/subset1_scalar_core_fx.json \
  --mx-json /path/to/subset1_scalar_core.json \
  --output /path/to/subset1_scalar_core_fx_vs_mx_report.md
```

Current FX wrapper:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make compare-subset1a-with-mx \
  MX_JSON=/Users/me/wip-launchx/wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json
```

To preserve a dedicated compare handoff run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make artifact-subset1a-compare \
  FX_SCALAR_JSON=/path/to/subset1_scalar_core_fx.json \
  MX_JSON=/path/to/subset1_scalar_core.json
```

This writes:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx_vs_mx_report.md`

## Comparison Rules

Blocking rows:

- classifications marked `required`

Blocking mismatches:

- missing required case on either side
- different `success` values on required rows
- mismatch on shared required semantic fields:
  - `pointer_identity_same`
  - `equal_same_value`
  - `equal_different_value`
  - `length_value`
  - `primary_value_text`
  - `alternate_value_text`

Diagnostic-only differences:

- raw `type_id`
- `type_description`
- raw hash numerics:
  - `hash_primary`
  - `hash_same_value`
  - `hash_different_value`

Cross-type numeric equality remains `semantic-match-only` in the current
contract, so any drift there should show as a warning until the contract is
promoted.

## Workflow Selfcheck

Before relying on the compare/report scripts after workflow changes, run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

What it covers:

- the Subset 1A manifest reporter against sample JSON
- the Subset 1A FX-vs-MX compare tool against sample JSON fixtures
- the FX wrapper commands `make compare-subset1a-with-mx` and
  `make artifact-subset1a-compare`
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset1_scalar_core.sh` under a temporary artifact root

On non-Darwin hosts, the selfcheck still validates the portable compare/report
surface and skips the native MX probe run.

## Recommended Order

1. Run `make clean test` on FX.
2. Run `make compare-subset1a-fx` on FX.
3. Run `scripts/run_subset1_scalar_core.sh` on MX.
4. Compare FX and MX JSON with `make compare-subset1a-with-mx`.
5. Record any blocker or warning in the Subset 1A contract ledger.
6. Only then decide whether Subset 1A is ready to support the next slice.
