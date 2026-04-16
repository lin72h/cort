# CORT Subset 1B Validation Workflow

Date: 2026-04-16

This document records the operational workflow for validating the MX-side
minimal `CFString` probe for Subset 1B and for later comparing the FX result to
the bounded MX surface.

Scope of this slice:

- immutable `CFString`
- ASCII create paths
- UTF-8 create/export paths
- UTF-16 code-unit create/export paths
- equality and hash coherence
- zero-length string behavior
- malformed UTF-8 rejection

Explicitly excluded:

- mutable strings
- locale-sensitive comparison
- normalization
- pointer-return optimizations
- broad encoding matrices
- plist and bplist themselves

## Current State

MX status:

- probe source, expectations, fixture, run script, compare wrapper, and suite
  wrapper exist
- bounded macOS validation is clean on the tightened 1B surface: `0` blockers,
  `0` warnings
- MX artifact path:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json`

FX status:

- bounded 1B `CFString` implementation exists locally in `cort-fx`
- local FX tests, installed-consumer test, and FX JSON probe are green
- no shared `subset1b_cfstring_fx.json` is published in-repo yet
- MX comparison against the tightened 1B surface is still pending

## MX Run

From:

- `/Users/me/wip-launchx/wip-cort-gpt/cort-mx`

Run:

```sh
scripts/run_subset1b_cfstring_core.sh
```

If the shared artifact root should match the coordination layout:

```sh
CORT_ARTIFACTS_ROOT=/Users/me/wip-launchx/wip-cort-gpt-artifacts \
  scripts/run_subset1b_cfstring_core.sh
```

Primary MX outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json`

Current MX result:

- blockers: `0`
- warnings: `0`

## MX Manifest Report

The MX script reports against:

- `cort-mx/expectations/subset1b_cfstring_core_expected.json`

through:

- `tools/report_case_manifest.exs`

The report should be treated the same way as earlier bounded slices:

- blockers gate advancement
- warnings indicate coordination or classification cleanup work
- artifacts must still be preserved even on non-zero exit

## MX Compare Run

Compare an FX Subset 1B JSON artifact against the MX JSON with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset1b_cfstring_compare.sh
```

Default inputs:

- FX JSON: `../subset1b_cfstring_fx.json`
- MX JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json`

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core-compare/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core-compare/out/subset1b_cfstring_fx_vs_mx_report.md`

## MX Suite

Run the full MX Subset 1B suite with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset1b_suite.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-suite/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-suite/cfstring-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-suite/cfstring-core/out/subset1b_cfstring_core.json`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-suite/cfstring-core-compare/summary.md`

This is the preferred coordination path on MX once FX publishes a real
`subset1b_cfstring_fx.json`. Before that artifact exists, the suite still
preserves a compare summary that says the FX input is not available yet.

## FX vs MX Direct Comparison

Compare the FX JSON artifact against the MX JSON with:

- `tools/compare_subset1b_cfstring_json.exs`

Direct invocation:

```sh
/Users/me/wip-launchx/wip-cort-gpt/tools/run_elixir.sh \
  /Users/me/wip-launchx/wip-cort-gpt/tools/compare_subset1b_cfstring_json.exs \
  --fx-json /path/to/subset1b_cfstring_fx.json \
  --mx-json /path/to/subset1b_cfstring_core.json \
  --output /path/to/subset1b_cfstring_fx_vs_mx_report.md
```

FX artifact path:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1b_cfstring_fx.json`

Convenience make target:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make compare-subset1b-with-mx MX_JSON=/path/to/subset1b_cfstring_core.json
```

## Comparison Rules

Blocking rows:

- classifications marked `required`
- classifications marked `required-but-error-text-flexible`

Blocking mismatches:

- missing required case on either side
- different `success` values on required rows
- mismatch on shared required semantic fields:
  - `equal_same_value`
  - `equal_different_value`
  - `length_value`
  - `primary_value_text`
  - `alternate_value_text`
- independent hash-coherence invariant on each side:
  - when `equal_same_value` is `true`, `hash_primary` must equal
    `hash_same_value`

Diagnostic-only differences:

- raw `type_id`
- `type_description`
- raw hash numerics:
  - `hash_primary`
  - `hash_same_value`
  - `hash_different_value`

Case-specific interpretation:

- string text fields should be emitted in ASCII-safe encoded form
- in this workflow they use hex-encoded UTF-8 and UTF-16 representations so
  the compare surface stays stable across terminals and source encodings
- malformed UTF-8 rejection is judged by `success` and object absence semantics,
  not by error wording
- the `cfstring_getcstring_small_buffer` row now covers both exact-fit success
  and undersized-buffer failure

## Workflow Selfcheck

Before relying on the new Subset 1B report/compare assets after workflow
changes, run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

What it covers:

- shell syntax for the new MX scripts
- the generic manifest reporter against the Subset 1B sample fixture
- the Subset 1B FX-vs-MX compare tool against sample JSON fixtures
- the MX Subset 1B compare artifact wrapper against sample JSON fixtures
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset1b_cfstring_core.sh` and
  `cort-mx/scripts/run_subset1b_suite.sh` under a temporary artifact root

## Recommended Order

1. Run `scripts/run_subset1b_cfstring_core.sh` on MX.
2. Record the MX artifact path and current clean result in the Subset 1B contract ledger.
3. Publish or provide `subset1b_cfstring_fx.json` when FX is ready to compare.
4. Run `scripts/run_subset1b_suite.sh` on MX.
5. Only then decide whether FX `CFString` is semantically aligned for this slice.
