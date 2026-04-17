# CORT Subset 3A FX Implementation Plan

Date: 2026-04-17

Status: planning only; do not start code changes from this document until the
current MX Subset 3A binary-plist artifacts are back and classified.

This document is the FX-side cut plan for the first bounded binary-plist
implementation. It answers a practical question that the current contract and
source-audit notes only answer indirectly:

- which files should be added or changed first
- what the minimal public ABI should look like
- how write and read should be structured internally
- how the first implementation should handle errors
- which tests and compare artifacts must exist before the slice is considered
  ready for MX comparison

This is not a replacement for:

- `docs/cort-subset3a-bplist-core-contract.md`
- `docs/cort-subset3a-source-audit-and-readiness.md`
- `docs/cort-subset3a-validation-workflow.md`

It is the concrete FX execution plan that should be followed after MX returns.

## Hard Gate

Do not start the actual `cort-fx` Subset 3A implementation until:

1. MX has run `cort-mx/scripts/run_subset3a_bplist_core.sh`
2. the MX summary is clean or any blockers are explicitly reclassified
3. the returned `subset3a_bplist_core.json` has been reviewed against the
   current 3A contract

This is a semantics-first slice. The correct order is:

```text
MX probe
  -> classify
  -> implement FX
  -> compare FX vs MX
```

Not:

```text
guess implementation details
  -> write code
  -> hope MX agrees later
```

## Planned File Set

The first Subset 3A code cut should be small and explicit.

### New public headers

- `cort-fx/include/CoreFoundation/CFError.h`
- `cort-fx/include/CoreFoundation/CFPropertyList.h`

### Updated public umbrella headers

- `cort-fx/include/CoreFoundation/CFBase.h`
- `cort-fx/include/CoreFoundation/CoreFoundation.h`

### New source files

- `cort-fx/src/CFError.c`
- `cort-fx/src/CFPropertyList.c`
- `cort-fx/src/CFBinaryPList.c`

### Updated internal runtime/header files

- `cort-fx/src/FXCFInternal.h`
- `cort-fx/src/CFRuntime.c`

### New tests

- `cort-fx/tests/bplist_core_tests.c`
- `cort-fx/tests/cort_subset3a_bplist_fx.c`

### Updated build and export files

- `cort-fx/Makefile`
- `cort-fx/exports/subset3a-exported-symbols.txt`
- `cort-fx/README.md`

## Minimal Public ABI

The first 3A ABI should stay narrow.

### `CFError.h`

Required public type and functions:

- `typedef const struct __CFError *CFErrorRef;`
- `CFErrorGetTypeID`
- `CFErrorGetCode`

Deliberately excluded from the first cut unless a real consumer forces them:

- `CFErrorCreate`
- `CFErrorGetDomain`
- `CFErrorCopyDescription`
- user-info APIs

Rationale:

- the current validated surface only needs `CFErrorRef` as an out-parameter
  object plus `CFErrorGetCode`
- broad `CFError` API work would widen the slice without helping the first
  binary-plist gate

### `CFPropertyList.h`

Required public declarations:

- `CFPropertyListFormat`
- `CFPropertyListMutabilityOptions`
- `kCFPropertyListBinaryFormat_v1_0`
- `kCFPropertyListImmutable`
- `kCFPropertyListReadCorruptError`
- `kCFPropertyListWriteStreamError`
- `CFPropertyListCreateData`
- `CFPropertyListCreateWithData`

Deliberately excluded:

- XML/OpenStep enums and APIs
- stream APIs
- mutable parse modes beyond exposing the enum values if needed for signature
  compatibility
- validation helpers

## Minimal Error Strategy

The source-audit note correctly identified this as a real implementation gate:
Subset 3A needs a public `CFErrorRef` out-parameter shape, but the repo does
not yet implement `CFError`.

The first code cut should resolve that with a narrow strategy:

### Public promise for 3A

- successful operations return a non-null result and leave `errorOut` either
  unchanged or set to `NULL`
- rejected operations return `NULL`
- when `errorOut != NULL`, rejected operations store a `CFErrorRef`
- `CFErrorGetCode(error)` returns the validated public error code

### First internal shape

Use a small immutable error object with:

- `CFRuntimeBase`
- `CFIndex code`

Optional first-cut fields:

- `CFStringRef domain`

Not required for the first cut:

- user info dictionaries
- debug strings
- recovery options

### Internal constructor policy

Do not expose a broad public `CFErrorCreate` yet.

Instead add a local helper in `FXCFInternal.h`:

```c
CFErrorRef _FXCFErrorCreateCode(CFAllocatorRef allocator, CFIndex code);
```

This lets `CFPropertyListCreateData` and `CFPropertyListCreateWithData` build
the needed error objects without widening the public API prematurely.

### First-cut error codes

The first implementation only needs the validated codes:

- `kCFPropertyListWriteStreamError`
- `kCFPropertyListReadCorruptError`

If later MX evidence shows another public code is required for the bounded
slice, add it explicitly and update the contract.

## Write Path Plan

`CFPropertyListCreateData` should be implemented as a narrow pipeline:

```text
validate supported graph
  -> assign object table slots
  -> encode supported objects
  -> compute offsets
  -> emit trailer
  -> return CFData
```

### Step 1: validate the graph

The validator should recurse over the input graph and allow only:

- `CFBoolean`
- `CFNumber`
- `CFDate`
- `CFData`
- `CFString`
- `CFArray`
- `CFDictionary`

Additional rules:

- dictionary keys must be `CFString`
- unsupported object types reject the write safely
- reject `NULL` top-level input
- stay explicit about recursion depth; the first cut should use a fixed depth
  cap and reject deeper graphs instead of risking runaway recursion

Important scope choice:

- if mutable arrays or dictionaries are encountered, either:
  - serialize them only when traversal is identical to the immutable validated
    surface, or
  - reject them explicitly
- do not silently depend on unvalidated mutable-container semantics

### Step 2: assign object slots

The first implementation should build a small object table in memory.

Recommended rule:

- assign object IDs by first traversal encounter
- preserve repeated pointer references by pointer identity, not by value

Why pointer-identity interning is the right first rule:

- it matches how the binary format naturally references objects
- it avoids re-encoding the same object repeatedly
- it leaves room for future aliasing tests without forcing value-deduplication

Value-deduplicating equal-but-distinct objects is not needed in 3A.

### Step 3: encode supported objects

Supported write tags for the first cut:

- bool
- int
- real
- date
- data
- ASCII string
- UTF-16 string
- array
- dictionary

Write rules:

- use `CFStringGetLength`, `CFStringGetBytes`, and `CFStringGetCharacters`
  instead of reading string internals directly
- use the validated 1B string surface as the boundary; do not reach into
  `CFString.c` internals for the first serializer
- use the validated 2A array/dictionary surface as the boundary; do not add
  broad container helper APIs just for the serializer

Number rules for the first cut:

- write tested integer values through the signed integer path
- write tested floating values through the `Float64` path
- do not widen immediately to every `CFNumberType`

String rules for the first cut:

- if full-range ASCII extraction succeeds and byte count equals string length,
  emit the ASCII string tag
- otherwise emit the UTF-16 string tag using `CFStringGetCharacters`

### Step 4: offset table and trailer

The first cut should use simple explicit helpers:

- determine `objectRefSize` from total object count
- determine `offsetIntSize` from final object-table offsets
- emit a normal `bplist00` header
- emit the offset table
- emit the trailer

Do not optimize for pre-sized buffers in the first cut.
Clarity is more important than shaving copies here.

## Read Path Plan

`CFPropertyListCreateWithData` should be implemented as a bounded parser:

```text
validate header and trailer
  -> validate offsets and object refs
  -> materialize supported objects
  -> cache by object index
  -> return immutable graph
```

### Step 1: validate header/trailer

Reject safely when:

- data is shorter than header plus trailer
- header is not `bplist00`
- trailer fields would imply impossible object-table bounds
- any computed offset points outside the data buffer

### Step 2: parse supported tags only

The first read cut only needs:

- bool
- int
- real
- date
- data
- ASCII string
- UTF-16 string
- array
- dictionary

Reject safely for:

- unsupported marker families
- unsupported integer widths
- unsupported collection shapes
- non-string dictionary keys

### Step 3: materialize immutable FX objects

Object materialization rules:

- ASCII strings use `CFStringCreateWithBytes(..., kCFStringEncodingASCII, ...)`
- UTF-16 strings decode big-endian code units to host-endian `UniChar` data and
  use `CFStringCreateWithCharacters`
- arrays build immutable `CFArray`
- dictionaries build immutable `CFDictionary`

Caching rule:

- cache parsed objects by object index during one parse call
- return the cached object when the same index is referenced again

Why cache in the first cut:

- avoids repeated parsing work
- preserves repeated-reference semantics more faithfully
- keeps later aliasing tests from forcing a parser rewrite

## Test Plan For The First Code Cut

The first Subset 3A code change should not land without both direct unit tests
and the FX probe artifact.

### Direct unit tests

`cort-fx/tests/bplist_core_tests.c` should cover:

- encode/decode ASCII root string
- encode/decode UTF-16 root string
- encode/decode mixed scalar array
- encode/decode string-key dictionary
- reject dictionary with non-string key on write
- reject invalid header on read
- reject truncated trailer on read
- wrong-type and `NULL` misuse for the new public entry points

### FX probe artifact

`cort-fx/tests/cort_subset3a_bplist_fx.c` should emit:

- `subset3a_bplist_fx.json`

with the same semantic fields used by the MX probe:

- `length_value`
- `primary_value_text`
- `alternate_value_text`

### Build target plan

When implementation starts, add these `Makefile` targets:

- `compare-subset3a-fx`
- `compare-subset3a-with-mx`

Do not wire them into `make test` until:

- the new library code exists
- the new pinned export snapshot exists
- the FX probe emits a real JSON artifact

## First-Cut Non-Goals

The first implementation should explicitly not do these:

- XML/OpenStep plist handling
- stream APIs
- mutable parse modes
- filtered key-path extraction
- UID, set, ordset, or null tags
- broad `CFError` API
- exact binary-layout matching with macOS
- exact error text matching with macOS

## Acceptance Gate

Subset 3A is ready to merge after implementation only when all of these are
true:

1. `make -C cort-fx clean test` passes
2. `make -C cort-fx test-installed` passes
3. a real FX artifact exists at
   `../wip-cort-gpt-artifacts/cort-fx/build/out/subset3a_bplist_fx.json`
4. the shared handoff artifact `../subset3a_bplist_fx.json` is published when
   MX needs it
5. MX compare against the real `subset3a_bplist_core.json` is clean or any
   variance is explicitly classified

## Current Conclusion

The next real code milestone after MX comes back should be:

```text
CFError minimal support
  + CFPropertyList binary-only API
  + bounded CFBinaryPList read/write core
  + direct unit tests
  + FX JSON probe
```

That is the correct first 3A implementation cut. Anything broader is the wrong
slice.
