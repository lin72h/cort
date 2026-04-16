# CORT Subset 0 Validation Workflow

Date: 2026-04-16

This document records the operational workflow for validating the current FX
Subset 0 proof against MX artifacts.

It connects four pieces that now exist:

- the local FX package and tests
- the MX execution scripts and expectations
- the shared public allocator comparison harness
- the FX artifact-run packaging script
- the FX-vs-MX comparison tool

## Local FX Run Packaging

From:

- `/Users/me/wip-launchx/wip-cort-gpt/cort-fx`

Run:

```sh
make artifact-run
```

This calls:

- `scripts/run_subset0_fx_artifacts.sh`

Default output:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset0-fx-local/`

The run directory includes:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- captured stdout/stderr for build, tests, install check, and compare harness
- `subset0_public_compare_fx.json`
- pinned and actual export snapshots
- built binaries and static library

This makes the FX side answerable in artifact form:

- what code was built
- what tests ran
- what the compare harness emitted
- which header and library snapshot the run used

## Shared FX/MX Compare Harness

Shared source:

- `cort-mx/src/cort_subset0_public_allocator_compare.c`

FX can run it locally through:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make compare-fx
```

FX output:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`

MX should compile the same source against native CoreFoundation and emit:

- `out/subset0_public_compare_mx.json`

This harness is deliberately smaller than the full MX runtime-ownership probe.
It exists to catch early semantic drift on the public allocator surface.

## MX Execution Scripts

MX execution assets now live under:

- `cort-mx/README.md`
- `cort-mx/scripts/run_subset0_runtime_ownership.sh`
- `cort-mx/scripts/run_subset0_public_allocator_compare.sh`
- `cort-mx/scripts/run_subset0_suite.sh`
- `cort-mx/expectations/subset0_runtime_ownership_expected.json`

Recommended MX entry point:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset0_suite.sh
```

If the FX allocator JSON is available during the MX run:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
FX_JSON=/Users/me/wip-launchx/wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json \
  scripts/run_subset0_suite.sh
```

Stable preserved FX JSON paths:

- live build output:
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`
- preserved artifact-run copy:
  `../wip-cort-gpt-artifacts/cort-fx/runs/subset0-fx-local/out/subset0_public_compare_fx.json`

The suite creates:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/`
- `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/`
- root `summary.md`
- root `sha256.txt`

The runtime-ownership script also runs:

- `tools/report_subset0_runtime_ownership.exs`

against:

- `cort-mx/expectations/subset0_runtime_ownership_expected.json`

to produce a blocker/warning summary from the MX JSON itself.

MX script exit behavior:

- the scripts still preserve their run directories on blocker findings
- `run_subset0_runtime_ownership.sh` exits non-zero if the runtime-ownership
  reporter finds blocking issues
- `run_subset0_public_allocator_compare.sh` exits non-zero if the FX-vs-MX
  shared allocator comparison finds blockers

## FX vs MX Report

Comparison tool:

- `tools/compare_subset0_json.exs`

Wrapper command:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make compare-with-mx MX_JSON=/path/to/subset0_public_compare_mx.json
```

Output:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_fx_vs_mx_report.md`

The comparison report classifies results as:

- `match`
- `warning`
- `BLOCKER`

Blocker rules:

- a required case is missing on either side
- a required case has different `success` values
- a required case differs on a shared blocking boolean semantic field, such as
  `pointer_identity_same`

Warning rules:

- classification differs
- a semantic-only case is missing
- diagnostic-only values differ, such as raw `type_id` or retain-count output

Numeric `type_id` values are diagnostic only across implementations. The report
records them, but does not treat raw numeric mismatch as blocking.

## Current Validation State

Current recorded macOS outcome:

- runtime-ownership suite summary:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/summary.md`
- public allocator comparison summary:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/summary.md`

Current result:

- runtime-ownership: 0 blockers, 0 warnings
- public allocator comparison: 0 blockers, 0 warnings
- no confirmed FX-vs-MX semantic mismatch on the shared allocator surface
- only diagnostic variance observed so far:
  `allocator_singleton_retain_identity` retain-count magnitude differs, while
  pointer identity semantics still match

## Recommended Order

1. Run `make artifact-run` on FX and preserve the local run directory.
2. Run `scripts/run_subset0_suite.sh` on MX and preserve the MX suite run
   directory.
3. Inspect `runtime-ownership/summary.md` for missing or failed required rows.
4. Compare FX and MX allocator JSON with `make compare-with-mx`, or provide
   `FX_JSON` directly during the MX allocator compare run.
5. Update the Subset 0 semantic ledger with:
   - concrete artifact paths
   - blocking mismatches
   - semantic-only variances
6. Only then decide whether Subset 0 is ready to serve as the base for
   Subset 1.

## What Still Requires MX

The current workflow improves discipline and artifact quality, but it does not
replace the MX oracle.

Subset 0 is still not semantically complete until MX confirms:

- public type identity observations
- public retain/release observations on native CF objects
- Create/Copy/Get ownership observations
- any accepted allocator variances

FX local evidence is necessary, but not sufficient.

Current state against that gate:

- public retain/release and allocator rows are now confirmed by MX for Subset 0
- no blocking mismatch is currently recorded on the shared allocator surface
- future Subset 0 completion still depends on whether the remaining public rows
  needed by LaunchX stay within this validated semantic envelope

Local validation boundary:

- FX artifact-run and compare tooling were executed locally
- MX shell and Elixir assets were syntax-checked locally
- MX runtime-ownership reporting was exercised against a sample JSON fixture
- actual MX compilation and run capture still require a real macOS host
