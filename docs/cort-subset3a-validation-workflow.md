# CORT Subset 3A Validation Workflow

Date: 2026-04-17

This document records the operational workflow for validating the MX-side
minimal binary-plist probe for Subset 3A and for later comparing the FX result
to the bounded MX surface.

Scope of this slice:

- `CFPropertyListCreateData` in binary format only
- `CFPropertyListCreateWithData` in immutable mode only
- `bplist00` header/trailer validation
- bool, int, real, date, data, string, array, and dictionary with string keys
- safe rejection of malformed binary input

Explicitly excluded:

- XML/OpenStep
- stream APIs
- mutable parse modes
- filtered or top-level-key helpers
- UID / set / ordset tags
- exact error text

## Current State

MX status:

- probe source, expectations, sample fixture, compare tool, and suite wrappers
  exist
- bounded macOS validation is clean: `0` blockers, `0` warnings
- MX artifact path:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json`

FX status:

- Subset 0, 1A, 1B, and 2A provide the runtime and object surface this slice
  depends on
- real FX binary-plist implementation now exists in `cort-fx`
- real FX probe output now exists at
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset3a_bplist_fx.json`
- shared handoff artifact now exists at `../subset3a_bplist_fx.json`
- real FX-vs-MX compare is clean: `0` blockers, `0` warnings
- preserved compare artifact run now exists at:
  - `../wip-cort-gpt-artifacts/cort-fx/runs/subset3a-fx-vs-mx/summary.md`
  - `../wip-cort-gpt-artifacts/cort-fx/runs/subset3a-fx-vs-mx/out/subset3a_bplist_fx_vs_mx_report.md`
- the FX file cut and fixture contract now live in:
  - `docs/cort-subset3a-fx-implementation-plan.md`
  - `docs/cort-subset3a-fixture-corpus.md`

Refresh the shared repo-root artifact from the latest FX build output with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make publish-subset3a-artifact
```

## MX Run

From:

- `/Users/me/wip-launchx/wip-cort-gpt/cort-mx`

Run:

```sh
scripts/run_subset3a_bplist_core.sh
```

If the shared artifact root should match the coordination layout:

```sh
CORT_ARTIFACTS_ROOT=/Users/me/wip-launchx/wip-cort-gpt-artifacts \
  scripts/run_subset3a_bplist_core.sh
```

Primary MX outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json`

The run should also preserve generated `.bplist` fixtures under the run root
`fixtures/` directory.

## MX Manifest Report

The MX script reports against:

- `cort-mx/expectations/subset3a_bplist_core_expected.json`

through:

- `tools/report_case_manifest.exs`

The report should be treated the same way as earlier bounded slices:

- blockers gate advancement
- warnings indicate coordination or classification cleanup work
- artifacts must still be preserved even on non-zero exit

## MX Compare Wrapper

Compare the shared FX artifact against the preserved MX JSON with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset3a_bplist_compare.sh
```

Default compare outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core-compare/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core-compare/out/subset3a_bplist_fx_vs_mx_report.md`

Default compare inputs:

- FX JSON: `../subset3a_bplist_fx.json`
- MX JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json`

Override either input with:

- `FX_JSON=/path/to/subset3a_bplist_fx.json`
- `MX_JSON=/path/to/subset3a_bplist_core.json`

## MX Suite

Run the full Subset 3A MX suite with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset3a_suite.sh
```

Default suite outputs:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-suite/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-suite/bplist-core/summary.md`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-suite/bplist-core-compare/summary.md`

The suite reruns the MX core probe and then:

- compares against the shared in-repo `subset3a_bplist_fx.json` when present
- or preserves a compare stub that explicitly says the shared FX artifact is
  missing

## FX vs MX Comparison

Compare the FX Subset 3A JSON artifact against the MX JSON with:

- `tools/compare_subset3a_bplist_json.exs`

Direct invocation:

```sh
/Users/me/wip-launchx/wip-cort-gpt/tools/run_elixir.sh \
  /Users/me/wip-launchx/wip-cort-gpt/tools/compare_subset3a_bplist_json.exs \
  --fx-json /path/to/subset3a_bplist_fx.json \
  --mx-json /path/to/subset3a_bplist_core.json \
  --output /path/to/subset3a_bplist_fx_vs_mx_report.md
```

Expected FX artifact path:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset3a_bplist_fx.json`
- shared handoff artifact: `../subset3a_bplist_fx.json`

## FX Compare Wrapper

The FX host can compare and package the Subset 3A result with:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make compare-subset3a-with-mx \
  FX_BPLIST_JSON=/path/to/subset3a_bplist_fx.json \
  MX_JSON=/path/to/subset3a_bplist_core.json
```

To preserve a compare-only handoff artifact run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make artifact-subset3a-compare \
  FX_BPLIST_JSON=/path/to/subset3a_bplist_fx.json \
  MX_JSON=/path/to/subset3a_bplist_core.json
```

Default compare-artifact output:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset3a-fx-vs-mx/summary.md`
- `../wip-cort-gpt-artifacts/cort-fx/runs/subset3a-fx-vs-mx/out/subset3a_bplist_fx_vs_mx_report.md`

## Comparison Rules

Blocking rows:

- classifications marked `required`

Blocking mismatches:

- missing required case on either side
- different `success` values on required rows
- mismatch on shared required semantic fields:
  - `length_value`
  - `primary_value_text`
  - `alternate_value_text`

Diagnostic-only differences:

- raw `type_id`
- `type_description`
- exact binary layout details not surfaced through the agreed semantic fields

Case-specific interpretation:

- for roundtrip cases, `length_value` represents logical top-level size:
  string length or collection count
- for rejection cases, `length_value` carries the expected public error code
  category
- `primary_value_text` and `alternate_value_text` use normalized semantic
  summaries, not raw error strings

## Workflow Selfcheck

Before relying on the new Subset 3A report/compare assets after workflow
changes, run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt
tools/run_elixir.sh tools/workflow_selfcheck.exs
```

What it should cover:

- shell syntax and execute-bit presence for the new MX scripts
- the generic manifest reporter against the Subset 3A sample fixture
- the Subset 3A FX-vs-MX compare tool against sample JSON fixtures
- the MX Subset 3A compare wrapper against preserved sample artifacts
- the FX `make compare-subset3a-with-mx` wrapper against preserved sample
  fixtures
- the FX `make artifact-subset3a-compare` wrapper against preserved sample
  fixtures
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset3a_bplist_core.sh` under a temporary artifact root
- on Darwin hosts only, actual execution of
  `cort-mx/scripts/run_subset3a_suite.sh` under a temporary artifact root with
  preserved sample FX JSON

## Recommended Order

1. Run `scripts/run_subset3a_bplist_core.sh` on MX.
2. Review `summary.md` for blockers or warnings.
3. Compare against the shared FX artifact with
   `scripts/run_subset3a_bplist_compare.sh` or `scripts/run_subset3a_suite.sh`.
4. Optionally preserve an FX-host compare-only handoff run with
   `make artifact-subset3a-compare`.
5. Record the artifact paths in the Subset 3A contract ledger.
6. Classify rejection behavior and binary-format reporting explicitly.
7. Use the clean shared compare as the gate for broader slices.
