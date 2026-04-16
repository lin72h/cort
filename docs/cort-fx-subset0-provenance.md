# cort-fx Subset 0 Provenance

Date: 2026-04-16

This note records where the temporary local `cort-fx` Subset 0 proof came
from, which upstream sources informed it, and where it deliberately diverges.

## Reference Basis

Audited upstream source checkout:

- `../nx/swift-corelibs-foundation`
- commit `f9c1d4eee71957c49eb66fd8fb5dac856d0efbf4`

Primary upstream reference files:

- `Sources/CoreFoundation/include/CFBase.h`
- `Sources/CoreFoundation/include/CFRuntime.h`
- `Sources/CoreFoundation/CFBase.c`
- `Sources/CoreFoundation/CFRuntime.c`

Semantic oracle:

- native macOS CoreFoundation on the MX lane

This FX proof is informed by the upstream source shape and by the Subset 0
contract. It is not a copied or lightly trimmed import of upstream
CoreFoundation.

## Local Proof Files

Temporary Subset 0 package files in this repo:

- `cort-fx/include/CoreFoundation/CFBase.h`
- `cort-fx/include/CoreFoundation/CFRuntime.h`
- `cort-fx/include/CoreFoundation/CoreFoundation.h`
- `cort-fx/src/FXCFInternal.h`
- `cort-fx/src/CFBase.c`
- `cort-fx/src/CFRuntime.c`
- `cort-fx/tests/runtime_ownership_tests.c`
- `cort-fx/tests/c_consumer_smoke.c`
- `cort-fx/exports/subset0-exported-symbols.txt`
- `cort-fx/README.md`

## File-Level Mapping

### `cort-fx/include/CoreFoundation/CFBase.h`

Derived from the public shape of upstream `include/CFBase.h`, but trimmed to
Subset 0:

- scalar base typedefs
- `CFRange`
- allocator typedefs and callbacks
- allocator singleton declarations
- polymorphic ownership APIs

Deliberate removals:

- Objective-C bridge annotations
- broad CF type declarations outside Subset 0
- bridge/ptrauth macros
- Foundation-facing compatibility surface

### `cort-fx/include/CoreFoundation/CFRuntime.h`

Derived from upstream `include/CFRuntime.h`:

- `CFRuntimeClass`
- pure-C `CFRuntimeBase`
- runtime registration and instance APIs

Deliberate removals:

- broad header dependencies
- full upstream callback/feature surface that Subset 0 does not exercise

### `cort-fx/src/CFBase.c`

Informed mainly by upstream `CFBase.c` allocator bootstrap and allocator API
paths:

- static allocator singleton pattern
- compile-time allocator runtime class record
- pure-C `CFAllocatorCreate` shape
- allocator callback storage and finalization

Deliberate departures:

- `_cfisa` points directly to `CFRuntimeClass`
- no Swift runtime path
- no Darwin malloc-zone path
- no CFString-based debug descriptions
- `kCFAllocatorUseContext` not supported as an input allocator in Subset 0
- no lossy allocate-and-free fallback in `CFAllocatorReallocate`

### `cort-fx/src/CFRuntime.c`

Informed mainly by upstream `CFRuntime.c` pure-C ownership path:

- minimal class table concept
- runtime registration
- instance creation
- type ID lookup
- atomic retain/release/finalization flow

Deliberate departures:

- minimal class table instead of the upstream whole-CoreFoundation class table
- no dispatch initialization
- no Swift allocation branch
- no Objective-C bridge/class-table logic
- simplified `_cfinfoa` encoding for Subset 0
- fixed-capacity table with publication rules aligned to the Subset 0 contract

### `cort-fx/src/FXCFInternal.h`

This file does not have a single upstream equivalent. It exists because the
upstream internal headers are too broad for Subset 0.

It replaces:

- `CFInternal.h`
- `CFRuntime_Internal.h`
- `CoreFoundation_Prefix.h`
- `ForFoundationOnly.h` static class setup assumptions

with a minimal internal surface:

- Subset 0 type IDs
- `_cfinfoa` bit helpers
- allocator prefix constants
- fail-fast helpers
- tiny inline helpers used by the local proof only

## Deliberate Non-Adoptions

The local proof does not adopt these upstream behaviors:

- `DEPLOYMENT_RUNTIME_SWIFT=1` default behavior
- `__HAS_DISPATCH__=1` default behavior
- monolithic internal headers
- whole-library class table population
- Objective-C bridge helpers
- Swift runtime allocation
- broad constant-object machinery beyond allocator immortality
- thread-local default allocator state

## Public Surface Policy

The local proof now uses hidden visibility by default and a pinned exported
symbol list in:

- `cort-fx/exports/subset0-exported-symbols.txt`

This keeps the package surface limited to the intended Subset 0 public API and
runtime entry points instead of leaking local `_FXCF*` helpers.

## Verification Basis

Local proof verification on the FX host currently consists of:

- `runtime_ownership_tests`
- `c_consumer_smoke`
- installed-header consumer smoke via `make test-installed`
- dependency audit via `make check-deps`
- export-surface audit via `make check-exports`

These prove that the standalone proof is structurally coherent on FX.

They do not replace MX semantic validation.
