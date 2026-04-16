# CORT Subset 0 Public Allocator Compare

Date: 2026-04-16

This document records the shared comparison harness for the public allocator
portion of CORT Subset 0.

Purpose:

- validate the public allocator behavior that FX and MX can both observe
- keep the comparison surface smaller than the full MX runtime-ownership probe
- find semantic drift before any Subset 1 work starts

This harness does not replace:

- the full MX runtime-ownership probe
- FX-only runtime tests for `_CFRuntime*`
- the Subset 0 semantic ledger

## Shared Source

Shared harness source:

- `cort-mx/src/cort_subset0_public_allocator_compare.c`

Why shared:

- FX can compile it against the local `cort-fx` headers and static library
- MX can compile the same source against native CoreFoundation
- the JSON schema and case ordering stay identical across both lanes

## FX Run

Build and emit the FX artifact:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make compare-fx
```

Output artifact:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`

For a preservable local FX run directory, use:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make artifact-run
```

Preserved FX JSON path from that run:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset0-fx-local/out/subset0_public_compare_fx.json`

## MX Run

Compile the shared source on macOS with native CoreFoundation:

```sh
clang -Wall -Wextra -Werror \
  -framework CoreFoundation \
  cort_subset0_public_allocator_compare.c \
  -o bin/cort_subset0_public_allocator_compare
./bin/cort_subset0_public_allocator_compare > out/subset0_public_compare_mx.json
```

Expected companion artifacts:

- `host.txt`
- `toolchain.txt`
- `summary.md`
- `src/cort_subset0_public_allocator_compare.c`
- `bin/cort_subset0_public_allocator_compare`
- `out/subset0_public_compare_mx.json`

## Case Set

The current shared case set is:

- `allocator_default_identity`
- `allocator_type_identity`
- `allocator_singleton_retain_identity`
- `allocator_null_allocate`
- `allocator_context_roundtrip`
- `allocator_default_switch`
- `allocator_reallocate_with_callback`
- `allocator_reallocate_without_callback`

Case intent:

- `required` rows are expected to match semantically
- `semantic-match-only` rows are used to surface deliberate or acceptable
  variance without blocking the Subset 0 runtime proof

The current known variance candidate is:

- `allocator_reallocate_without_callback`

FX intentionally returns `NULL` when no reallocate callback exists. MX may do
something different. That difference is useful evidence and should be recorded,
not papered over.

## Comparison Workflow

1. Run the FX harness and preserve `subset0_public_compare_fx.json`.
2. Run the MX harness and preserve `subset0_public_compare_mx.json`.
3. Compare the results case by case.
   Use:

   ```sh
   cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
   make compare-with-mx MX_JSON=/path/to/subset0_public_compare_mx.json
   ```

4. Update the Subset 0 semantic ledger for any required rows that show a real
   mismatch.
5. Record semantic-only variance separately from blocking mismatches.

Do not upgrade a semantic-only row into a blocking runtime issue unless the
Subset 0 contract is amended.

## Current Result

The current MX rerun with a real FX JSON input produced:

- allocator comparison report:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/out/subset0_public_compare_report.md`
- MX JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/out/subset0_public_compare_mx.json`

Outcome:

- blockers: 0
- warnings: 0
- verdict: no blocking mismatches found in the shared comparison surface

Observed diagnostic-only variance:

- `allocator_singleton_retain_identity` retain-count magnitude differs between
  FX and MX
- pointer identity and required ownership semantics still match

## What This Does Not Cover

This harness does not cover:

- `_CFRuntimeRegisterClass`
- `_CFRuntimeCreateInstance`
- `_CFRuntimeInitStaticInstance`
- FX-only finalizer behavior for custom runtime classes
- broad MX object identity probing across strings, data, numbers, booleans,
  dates, arrays, dictionaries, and errors

Those remain covered by:

- `runtime_ownership_tests`
- `runtime_abort_tests`
- `cort_mx_subset0_runtime_ownership.c`
