# cort-mx Subset 0 Execution

This directory now holds the executable MX-side assets for CORT Subset 0.

Default run artifacts are written outside this repo to a sibling directory
named after the repo, currently `../wip-cort-gpt-artifacts/`. Override that
root with `CORT_ARTIFACTS_ROOT=/path/to/artifacts`.

Current contents:

- `src/cort_mx_subset0_runtime_ownership.c`
- `src/cort_subset0_public_allocator_compare.c`
- `scripts/common.sh`
- `scripts/run_subset0_runtime_ownership.sh`
- `scripts/run_subset0_public_allocator_compare.sh`
- `scripts/run_subset0_suite.sh`
- `expectations/subset0_runtime_ownership_expected.json`
- `fixtures/subset0_runtime_ownership_sample.json`

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
FX_JSON=/path/to/subset0_public_compare_fx.json \
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
FX_JSON=/path/to/subset0_public_compare_fx.json \
  scripts/run_subset0_suite.sh
```

## Runtime Ownership Report

The runtime-ownership script uses:

- `../tools/report_subset0_runtime_ownership.py`
- `expectations/subset0_runtime_ownership_expected.json`

It emits a blocker/warning summary in the run directory `summary.md`.

Exit status:

- `0` when no blocking issues are found by the runtime-ownership reporter
- non-zero when blocking issues are found, after artifacts are still written

## Local Validation Status

Validated locally on the FX host:

- shell syntax for all scripts with `sh -n`
- Python syntax for the reporters with `python3 -m py_compile`
- runtime-ownership reporter behavior against
  `fixtures/subset0_runtime_ownership_sample.json`

Not validated locally on the FX host:

- actual compilation against native macOS CoreFoundation
- actual MX run directories and hashes

Those still require a real macOS run.
