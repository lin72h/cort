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

- probe source, expectations, fixture, and run script exist
- bounded macOS validation has not been run yet

FX status:

- no `CFString` implementation yet
- Subset 1A provides the runtime and scalar base this slice will depend on

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

## MX Manifest Report

The MX script reports against:

- `cort-mx/expectations/subset1b_cfstring_core_expected.json`

through:

- `tools/report_case_manifest.exs`

The report should be treated the same way as earlier bounded slices:

- blockers gate advancement
- warnings indicate coordination or classification cleanup work
- artifacts must still be preserved even on non-zero exit

## Future FX vs MX Comparison

Once FX emits a Subset 1B JSON artifact, compare it against the MX JSON with:

- `tools/compare_subset1b_cfstring_json.exs`

Direct invocation:

```sh
/Users/me/wip-launchx/wip-cort-gpt/tools/run_elixir.sh \
  /Users/me/wip-launchx/wip-cort-gpt/tools/compare_subset1b_cfstring_json.exs \
  --fx-json /path/to/subset1b_cfstring_fx.json \
  --mx-json /path/to/subset1b_cfstring_core.json \
  --output /path/to/subset1b_cfstring_fx_vs_mx_report.md
```

Expected future FX artifact path:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1b_cfstring_fx.json`

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

## Workflow Selfcheck

Before relying on the new Subset 1B report/compare assets after workflow
changes, run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

What it should cover after the Subset 1B assets land:

- shell syntax for the new MX script
- the generic manifest reporter against the Subset 1B sample fixture
- the Subset 1B FX-vs-MX compare tool against sample JSON fixtures
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset1b_cfstring_core.sh` under a temporary artifact
  root

## Recommended Order

1. Run `scripts/run_subset1b_cfstring_core.sh` on MX.
2. Review `summary.md` for blockers or warnings.
3. Record the artifact path in the Subset 1B contract ledger.
4. Classify malformed UTF-8 rejection explicitly.
5. Only then start the FX `CFString` implementation.
