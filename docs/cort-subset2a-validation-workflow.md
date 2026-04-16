# CORT Subset 2A Validation Workflow

Date: 2026-04-17

This document records the operational workflow for validating the MX-side
minimal container probe for Subset 2A and for later comparing the FX result to
the bounded MX surface.

Scope of this slice:

- immutable `CFArray` with `kCFTypeArrayCallBacks`
- mutable `CFArray` append/remove ownership
- immutable `CFDictionary` with `kCFType` key/value callbacks
- mutable `CFDictionary` set/replace/remove ownership
- borrowed Get semantics for arrays and dictionary values
- empty container behavior
- string-key dictionary lookup

Explicitly excluded:

- custom callback modes
- `NULL` callback modes
- copy-string key callbacks
- broad mutation APIs
- enumeration and apply APIs
- container equality/hash
- plist and bplist themselves

## Current State

MX status:

- probe source, expectations, fixture, and run script exist
- bounded macOS validation exists and the probe has been hardened to avoid the
  earlier saturated-retain-count false blocker on
  `cfarray_immutable_cftype_borrowed_get`

FX status:

- bounded `CFArray` and `CFDictionary` implementation exists locally
- shared FX artifact path is `../subset2a_container_fx.json`
- Subset 1A and Subset 1B provide the runtime, scalar, and `CFString` base for
  this slice

## MX Run

From:

- `/Users/me/wip-launchx/wip-cort-gpt/cort-mx`

Run:

```sh
scripts/run_subset2a_container_core.sh
```

If the shared artifact root should match the coordination layout:

```sh
CORT_ARTIFACTS_ROOT=/Users/me/wip-launchx/wip-cort-gpt-artifacts \
  scripts/run_subset2a_container_core.sh
```

Primary MX outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json`

## MX Compare Wrapper

Compare the shared FX artifact against the preserved MX JSON with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset2a_container_compare.sh
```

Default compare outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core-compare/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core-compare/out/subset2a_container_fx_vs_mx_report.md`

Default compare inputs:

- FX JSON: `../subset2a_container_fx.json`
- MX JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json`

Override either input with:

- `FX_JSON=/path/to/subset2a_container_fx.json`
- `MX_JSON=/path/to/subset2a_container_core.json`

## MX Suite

Run the full Subset 2A MX suite with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset2a_suite.sh
```

Default suite outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-suite/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-suite/container-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-suite/container-core-compare/summary.md`

The suite reruns the MX core probe and then:

- compares against the shared in-repo `subset2a_container_fx.json` when present
- or preserves a compare stub that explicitly says the shared FX artifact is
  missing

## MX Manifest Report

The MX script reports against:

- `cort-mx/expectations/subset2a_container_core_expected.json`

through:

- `tools/report_case_manifest.exs`

The report should be treated the same way as earlier bounded slices:

- blockers gate advancement
- warnings indicate coordination or classification cleanup work
- artifacts must still be preserved even on non-zero exit

## Future FX vs MX Comparison

Once FX emits a Subset 2A JSON artifact, compare it against the MX JSON with:

- `tools/compare_subset2a_container_json.exs`

Direct invocation:

```sh
/Users/me/wip-launchx/wip-cort-gpt/tools/run_elixir.sh \
  /Users/me/wip-launchx/wip-cort-gpt/tools/compare_subset2a_container_json.exs \
  --fx-json /path/to/subset2a_container_fx.json \
  --mx-json /path/to/subset2a_container_core.json \
  --output /path/to/subset2a_container_fx_vs_mx_report.md
```

Expected FX artifact path:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset2a_container_fx.json`
- shared handoff artifact: `../subset2a_container_fx.json`

## Comparison Rules

Blocking rows:

- classifications marked `required`

Blocking mismatches:

- missing required case on either side
- different `success` values on required rows
- mismatch on shared required semantic fields:
  - `pointer_identity_same`
  - `length_value`
  - `primary_value_text`
  - `alternate_value_text`

Diagnostic-only differences:

- raw `type_id`
- `type_description`
- raw hash numerics if any later fixture includes them

Case-specific interpretation:

- retain-count numerics remain diagnostic-only across implementations
- borrowed Get semantics are judged by each side's local success criteria plus
  pointer identity fields
- copy container pointer identity is not part of the compare surface
- string-key lookup is represented through the key/value text fields and
  pointer-identity semantics, not through raw retain-count parity

## Workflow Selfcheck

Before relying on the new Subset 2A report/compare assets after workflow
changes, run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

What it should cover after the Subset 2A assets land:

- shell syntax and execute-bit presence for the new MX script
- the generic manifest reporter against the Subset 2A sample fixture
- the Subset 2A FX-vs-MX compare tool against sample JSON fixtures
- the MX Subset 2A compare wrapper against preserved sample artifacts
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset2a_container_core.sh` under a temporary artifact
  root
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset2a_suite.sh` under a temporary artifact root with
  preserved sample FX JSON

## Recommended Order

1. Run `scripts/run_subset2a_container_core.sh` on MX.
2. Review `summary.md` for blockers or warnings.
3. Compare against the shared FX artifact with
   `scripts/run_subset2a_container_compare.sh` or `scripts/run_subset2a_suite.sh`.
4. Record the artifact paths in the Subset 2A contract ledger.
5. Classify borrowed Get semantics and replacement-release semantics
   explicitly.
6. Only then widen beyond the bounded container core.
