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
- `scripts/run_subset1_scalar_core_compare.sh`
- `scripts/run_subset1_suite.sh`
- `expectations/subset1_scalar_core_expected.json`
- `fixtures/subset1_scalar_core_sample.json`

Subset 1B:

- `src/cort_mx_subset1b_cfstring_core.c`
- `scripts/run_subset1b_cfstring_core.sh`
- `expectations/subset1b_cfstring_core_expected.json`
- `fixtures/subset1b_cfstring_core_sample.json`

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
- `tools/run_elixir.sh` also checks `~/.elixir-install/installs` by default;
  override that root with `ELIXIR_INSTALLS_ROOT=/path/to/installs` if needed

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

Portable checks covered by `../tools/run_elixir.sh ../tools/workflow_selfcheck.exs`:

- shell syntax for all scripts with `sh -n`
- Elixir syntax for the reporters with `elixir`
- runtime-ownership reporter behavior against
  `fixtures/subset0_runtime_ownership_sample.json`
- generic manifest reporter behavior against
  `fixtures/subset1_scalar_core_sample.json`
- generic manifest reporter behavior against
  `fixtures/subset1b_cfstring_core_sample.json`
- Subset 0 FX-vs-MX compare behavior against sample JSON fixtures
- Subset 1A FX-vs-MX compare behavior against sample JSON fixtures
- Subset 1B FX-vs-MX compare behavior against sample JSON fixtures
- MX-side Subset 1A compare artifact wrapper against preserved sample artifacts
- FX-side Subset 1A compare wrappers against preserved sample artifacts

Additional checks performed by the same selfcheck on Darwin hosts:

- actual `scripts/run_subset0_suite.sh` execution under a temporary artifact root
- actual `scripts/run_subset1_scalar_core.sh` execution under a temporary
  artifact root
- actual `scripts/run_subset1_suite.sh` execution under a temporary artifact
  root with the shared repo-local FX JSON
- actual `scripts/run_subset1b_cfstring_core.sh` execution under a temporary
  artifact root
- preservation of MX run directories, summaries, and hashes

On non-Darwin hosts, the selfcheck skips those macOS-only MX script checks.

## Subset 1A Scalar Core

This slice validates immutable scalar core semantics while explicitly deferring
`CFString` to its own follow-on slice.

Contract:

- `../docs/cort-subset1a-scalar-core-contract.md`
- `../docs/cort-subset1a-validation-workflow.md`

Run it with:

```sh
cd cort-mx
scripts/run_subset1_scalar_core.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/`

Compare the shared FX artifact against an MX scalar-core JSON with:

```sh
cd cort-mx
scripts/run_subset1_scalar_core_compare.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core-compare/`

This defaults to:

- FX JSON: `../subset1_scalar_core_fx.json`
- MX JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json`

Run the full MX Subset 1A suite with:

```sh
cd cort-mx
scripts/run_subset1_suite.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-suite/`

This reruns the MX scalar-core probe and then compares the fresh MX JSON
against the shared repo-local `subset1_scalar_core_fx.json`.

Direct compare invocation is still available with:

```sh
cd ..
tools/run_elixir.sh tools/compare_subset1_scalar_core_json.exs \
  --fx-json /path/to/subset1_scalar_core_fx.json \
  --mx-json /path/to/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json \
  --output /path/to/subset1_scalar_core_fx_vs_mx_report.md
```

## Subset 1B Minimal CFString

This slice validates the bounded immutable `CFString` core needed by future
bplist and plist-valid key paths.

Contract:

- `../docs/cort-subset1b-cfstring-contract.md`
- `../docs/cort-subset1b-validation-workflow.md`

Run it with:

```sh
cd cort-mx
scripts/run_subset1b_cfstring_core.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/`

When FX later emits a Subset 1B JSON artifact, compare it against MX with:

```sh
cd ..
tools/run_elixir.sh tools/compare_subset1b_cfstring_json.exs \
  --fx-json /path/to/subset1b_cfstring_fx.json \
  --mx-json /path/to/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json \
  --output /path/to/subset1b_cfstring_fx_vs_mx_report.md
```
