# cort-fx Subset 0 Proof

This directory holds the temporary standalone `cort-fx` proof for CORT
Subset 0: runtime and ownership core.

It is not the final repository layout. The final target remains
`../nx/cort/cort-fx/` after explicit permission.

## Current Scope

Implemented:

- `CFRuntimeBase` with `_cfisa` as `CFRuntimeClass *`
- minimal runtime class table
- allocator bootstrap with immortal static allocator singletons
- `_CFRuntimeRegisterClass`
- `_CFRuntimeGetClassWithTypeID`
- `_CFRuntimeCreateInstance`
- `_CFRuntimeInitStaticInstance`
- `CFGetTypeID`
- `CFRetain`
- `CFRelease`
- `CFGetRetainCount`
- `CFGetAllocator`
- `CFAllocatorGetDefault`
- `CFAllocatorSetDefault`
- `CFAllocatorCreate`
- `CFAllocatorAllocate`
- `CFAllocatorReallocate`
- `CFAllocatorDeallocate`
- `CFAllocatorGetPreferredSizeForSize`
- `CFAllocatorGetContext`

Explicitly not implemented here:

- `CFString`
- `CFData`
- `CFNumber`
- `CFBoolean`
- `CFDate`
- containers
- plist/bplist
- XML plist
- BlocksRuntime integration
- Objective-C bridge behavior
- Swift runtime allocation

## Provenance

This proof is based on the `CoreFoundation` source shape in
`../nx/swift-corelibs-foundation/Sources/CoreFoundation`, especially:

- `include/CFBase.h`
- `include/CFRuntime.h`
- `CFBase.c`
- `CFRuntime.c`

It is not a direct copy. The implementation here was rewritten as a small
standalone proof because the upstream translation units assume Swift-runtime
defaults, dispatch, and a much broader class table than Subset 0 allows.

Native macOS CoreFoundation remains the semantic oracle for MX validation.

Default build and run artifacts are written outside this repo to a sibling
directory named after the repo, currently `../wip-cort-gpt-artifacts/`.
Override that root with `CORT_ARTIFACTS_ROOT=/path/to/artifacts`.

## Build

```sh
cd cort-fx
make clean test
```

This builds under `../wip-cort-gpt-artifacts/cort-fx/build/` by default:

- `lib/libcortfx.a`
- `bin/runtime_ownership_tests`
- `bin/runtime_abort_tests`
- `bin/c_consumer_smoke`
- `bin/subset0_public_compare_fx`

## Verification Targets

```sh
cd cort-fx
make check-deps
make check-exports
make compare-fx
make artifact-run
make test-installed
```

What they enforce:

- no Swift, Objective-C, dispatch, ICU, or CoreFoundation link/symbol coupling
- public exported symbol set matches `exports/subset0-exported-symbols.txt`
- abort-on-misuse policy remains enforced for invalid public calls and
  ownership failures
- installed headers and static library are usable by a standalone C consumer
- shared allocator comparison harness emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`
- artifact-run packaging emits a preservable FX run directory under
  `../wip-cort-gpt-artifacts/cort-fx/runs/`

## Install

```sh
cd cort-fx
make install PREFIX=/usr/local DESTDIR=/tmp/cort-fx-stage
```

Installed files:

- `include/CoreFoundation/CFBase.h`
- `include/CoreFoundation/CFRuntime.h`
- `include/CoreFoundation/CoreFoundation.h`
- `lib/libcortfx.a`
- `share/doc/cort-fx/README.md`

## Shared Compare Harness

The shared public allocator comparison harness source lives at:

- `../cort-mx/src/cort_subset0_public_allocator_compare.c`

FX runs it with:

```sh
cd cort-fx
make compare-fx
```

That produces:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`

MX can compile the same source against native macOS CoreFoundation to produce a
matching JSON artifact for side-by-side review.

## Artifact Run

Capture a preservable local FX run directory:

```sh
cd cort-fx
make artifact-run
```

Default output:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset0-fx-local/`

Override the run directory:

```sh
cd cort-fx
make artifact-run RUN_DIR=/absolute/path/subset0-fx-20260416
```

The run directory includes:

- host and toolchain metadata
- commands used
- per-step stdout/stderr
- the pinned and actual export snapshots
- the FX allocator comparison JSON
- installed headers and built binaries
- `sha256.txt`

## Compare With MX

Once MX produces `subset0_public_compare_mx.json`, compare it against the FX
artifact with:

```sh
cd cort-fx
make compare-with-mx MX_JSON=/path/to/subset0_public_compare_mx.json
```

This uses:

- `../tools/compare_subset0_json.exs`

and writes:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_fx_vs_mx_report.md`

## Constraints

- process-global default allocator only
- fixed-capacity class table
- `_CFRuntimeUnregisterClassWithTypeID` is a no-op
- `kCFAllocatorUseContext` is not supported as an input allocator
- `CFAllocatorReallocate` returns `NULL` when the allocator does not provide a
  reallocate callback
- this proof is useful evidence, not the semantic oracle

MX validation artifacts remain the gate before broader extraction or LaunchX
use.
