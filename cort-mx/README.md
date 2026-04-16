# cort-mx Execution

This directory now holds the executable MX-side assets for the current bounded
CORT validation slices.

Default run artifacts are written outside this repo to a sibling directory
named after the repo, currently `../wip-cort-gpt-artifacts/`. Override that
root with `CORT_ARTIFACTS_ROOT=/path/to/artifacts`.

Current contents:

Subset 0:

- `src/cort_mx_subset0_runtime_ownership.c`
- `src/cort_subset0_public_allocator_compare.c`
- `scripts/common.sh`
- `scripts/run_subset0_runtime_ownership.sh`
- `scripts/run_subset0_public_allocator_compare.sh`
- `scripts/run_subset0_suite.sh`
- `expectations/subset0_runtime_ownership_expected.json`
- `fixtures/subset0_runtime_ownership_sample.json`

Subset 1A:

- `src/cort_mx_subset1_scalar_core.c`
- `scripts/run_subset1_scalar_core.sh`
- `expectations/subset1_scalar_core_expected.json`
- `fixtures/subset1_scalar_core_sample.json`

## Purpose

These assets turn the MX lane from a text request into a reproducible run
workflow:

- build native macOS CoreFoundation probes
- capture run artifacts
- generate `sha256.txt`
- produce markdown summaries
- compare the shared allocator harness against FX JSON when available
- report the full runtime-ownership JSON against the current expected-case
  manifest

Elixir note:

- the workflow reporters now run through Elixir with no third-party dependencies
- if your default `elixir` points at the wrong OTP, set `ELIXIR=/path/to/elixir`
  and/or adjust `PATH` before running the scripts

## One-Off Runs

Runtime ownership probe:

```sh
cd cort-mx
scripts/run_subset0_runtime_ownership.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-runtime-ownership/`

Public allocator compare probe:

```sh
cd cort-mx
scripts/run_subset0_public_allocator_compare.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-public-allocator-compare/`

To compare against an FX allocator JSON during the MX run:

```sh
cd cort-mx
FX_JSON=/Users/me/wip-launchx/wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json \
  scripts/run_subset0_public_allocator_compare.sh
```

## Combined Run

Run both probes under one suite root:

```sh
cd cort-mx
scripts/run_subset0_suite.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/`

With FX allocator JSON available:

```sh
cd cort-mx
FX_JSON=/Users/me/wip-launchx/wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json \
  scripts/run_subset0_suite.sh
```

If the FX lane used `make artifact-run`, the same JSON is also preserved under:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset0-fx-local/out/subset0_public_compare_fx.json`

## Runtime Ownership Report

The runtime-ownership script uses:

- `../tools/report_subset0_runtime_ownership.exs`
- `expectations/subset0_runtime_ownership_expected.json`

It emits a blocker/warning summary in the run directory `summary.md`.

Exit status:

- `0` when no blocking issues are found by the runtime-ownership reporter
- non-zero when blocking issues are found, after artifacts are still written

## Local Validation Status

Validated locally on the FX host:

- shell syntax for all scripts with `sh -n`
- Elixir syntax for the reporters with `elixir`
- runtime-ownership reporter behavior against
  `fixtures/subset0_runtime_ownership_sample.json`
- generic manifest reporter behavior against
  `fixtures/subset1_scalar_core_sample.json`

Not validated locally on the FX host:

- actual compilation against native macOS CoreFoundation
- actual MX run directories and hashes

Those still require a real macOS run.

## Subset 1A Scalar Core

This slice validates immutable scalar core semantics while explicitly deferring
`CFString` to its own follow-on slice.

Contract:

- `../docs/cort-subset1a-scalar-core-contract.md`

Run it with:

```sh
cd cort-mx
scripts/run_subset1_scalar_core.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/`
