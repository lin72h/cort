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
- MX suite now preserves a dedicated shared-artifact compare run under:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/`
- latest shared-artifact compare result: `0` blockers, `0` warnings
- latest compare report path:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/scalar-core-compare/out/subset1a_fx_vs_mx_report.md`

FX status:

- local Subset 1A implementation exists in `cort-fx`
- `make test` passes
- local FX probe emits:
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx.json`
- shared FX handoff artifact is published in repo at:
  `/Users/me/wip-launchx/wip-cort-gpt/subset1_scalar_core_fx.json`

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

The repo-local copy is the preferred MX input when coordinating a shared
compare without regenerating the FX probe locally.

Refresh that shared repo-root artifact from the latest FX build output with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make publish-subset1a-artifact
```

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

## MX Compare Run

Compare the shared FX JSON against the MX JSON with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset1_scalar_core_compare.sh
```

Default inputs:

- FX JSON: `../subset1_scalar_core_fx.json`
- MX JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json`

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core-compare/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core-compare/out/subset1a_fx_vs_mx_report.md`

## MX Suite

Run the full MX Subset 1A suite with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset1_suite.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/scalar-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/scalar-core/out/subset1_scalar_core.json`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/scalar-core-compare/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/scalar-core-compare/out/subset1a_fx_vs_mx_report.md`

This is the preferred coordination path on MX now that `subset1_scalar_core_fx.json`
is published in-repo. The suite reruns the MX probe and compares the fresh MX
JSON against the shared FX artifact by default.

## Direct And FX-Side Comparison

Manual compare surfaces still exist when needed:

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

FX wrapper:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make compare-subset1a-with-mx \
  MX_JSON=/Users/me/wip-launchx/wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json
```

To preserve a dedicated FX-owned compare handoff run:

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

Cross-type numeric equality is required for the currently tested Subset 1A
forms only:

- `kCFNumberSInt32Type`
- `kCFNumberSInt64Type`
- exact-integer `kCFNumberFloat64Type`

Do not generalize that into broad `CFNumber` canonicalization beyond this slice
without a new MX validation pass.

## Workflow Selfcheck

Before relying on the compare/report scripts after workflow changes, run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

What it covers:

- the Subset 1A manifest reporter against sample JSON
- the Subset 1A FX-vs-MX compare tool against sample JSON fixtures
- the MX Subset 1A compare artifact wrapper against sample JSON fixtures
- the FX `make clean test` target, including dependency/export audits and the
  current wrong-type abort surface
- the FX wrapper commands `make compare-subset1a-with-mx` and
  `make artifact-subset1a-compare`
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset1_scalar_core.sh` and
  `cort-mx/scripts/run_subset1_suite.sh` under a temporary artifact root

On non-Darwin hosts, the selfcheck still validates the portable compare/report
surface and skips the native MX probe run.

## Recommended Order

1. Run `make clean test` on FX.
2. Publish or refresh the shared FX JSON at `subset1_scalar_core_fx.json`.
3. Run `scripts/run_subset1_suite.sh` on MX.
4. Record any blocker or warning in the Subset 1A contract ledger.
5. Only then decide whether Subset 1A is ready to support the next slice.
