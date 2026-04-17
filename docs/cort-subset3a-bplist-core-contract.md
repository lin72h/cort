# CORT Subset 3A Binary Plist Core Contract

Date: 2026-04-17

Status: planned; MX validation assets prepared; FX implementation has not
started.

This document defines the next narrow semantic slice after Subset 2A:
binary-plist read/write for the first property-list-valid object graph needed
by LaunchX packet and fixture work.

Purpose:

- validate the smallest useful `bplist00` surface on macOS before FX binary
  plist implementation starts
- keep the first binary-plist slice tied to the already-validated runtime,
  scalar, string, and container subsets
- avoid smuggling in XML plist, stream APIs, mutable parse modes, or broad
  property-list utilities before LaunchX needs them

## Why This Is Subset 3A, Not Full CFPropertyList

The full public property-list surface is much broader than the first binary
 packet need:

- XML plist parsing and writing
- stream-based APIs
- mutable-container and mutable-leaf parse options
- top-level-key and filtered-keypath helpers
- `CFSet`, keyed-archiver UID, and other tags not needed by LaunchX packets
- broad validation and deep-copy utilities

So the next useful execution slice is:

```text
Subset 3A:
  CFPropertyListCreateData
  CFPropertyListCreateWithData
  binary format only
  immutable mode only
  bplist00 header/trailer validation
  bool, int, real, date, data, string, array, dict with string keys

Future widening, if needed:
  stream APIs
  XML/OpenStep
  mutable parse modes
  top-level-key and filtered-keypath helpers
  UID / set / ordset tags
  broader error/detail APIs
```

This keeps the semantics-first loop intact instead of turning the next slice
into "all property lists".

## Scope

This is not "all of `CFPropertyList`".

This slice is the minimum binary-plist surface that can later support:

- LaunchX packet and control-envelope dictionaries
- corpus-backed bplist fixtures
- binary serialization of the already-validated scalar, string, array, and
  dictionary objects
- safe rejection of malformed packet payloads

It is deliberately smaller than the upstream `CFPropertyList` and
`CFBinaryPList` implementation.

## Included APIs

### Type and ownership

- `CFRetain`
- `CFRelease`
- `CFGetTypeID`

### Property list surface

- `CFPropertyListCreateData`
- `CFPropertyListCreateWithData`
- `CFPropertyListFormat`
- `kCFPropertyListBinaryFormat_v1_0`
- `kCFPropertyListImmutable`

### Error observation surface

- `CFErrorGetCode` for semantic classification only

### Included encoded object surface

- `CFBoolean`
- `CFNumber`
  - signed integer write/read through the tested `SInt64` path
  - `Float64` write/read through the tested real path
- `CFDate`
- `CFData`
- `CFString`
  - ASCII write/read path
  - UTF-16 write/read path driven by non-ASCII strings
- `CFArray`
- `CFDictionary`
  - string keys only

## Explicitly Excluded

- XML plist parsing and writing
- OpenStep plist parsing and writing
- `CFPropertyListWrite`
- `CFPropertyListCreateWithStream`
- `CFPropertyListWriteToStream`
- mutable parse modes:
  - `kCFPropertyListMutableContainers`
  - `kCFPropertyListMutableContainersAndLeaves`
- `CFPropertyListIsValid` as a required public slice
- deep-copy and top-level-key helpers
- filtered key-path helpers
- `CFSet`
- keyed-archiver UID
- `kCFBinaryPlistMarkerUTF8String`
- `kCFBinaryPlistMarkerUID`
- `kCFBinaryPlistMarkerSet`
- `kCFBinaryPlistMarkerOrdset`
- `kCFNull`
- exact `CFError` wording
- Objective-C bridge behavior

## Required MX Observations

For the tested surface, MX must preserve observations for:

- `CFPropertyListCreateData(..., kCFPropertyListBinaryFormat_v1_0, ...)`
  producing non-null data for supported immutable property-list graphs
- `CFPropertyListCreateWithData(..., kCFPropertyListImmutable, ...)`
  returning a non-null immutable graph plus
  `format == kCFPropertyListBinaryFormat_v1_0`
- written data beginning with `bplist00`
- ASCII root-string roundtrip through the ASCII string tag path
- non-ASCII root-string roundtrip through the UTF-16 string tag path
- mixed scalar array roundtrip
- string-key dictionary roundtrip
- non-string dictionary-key write rejection
- invalid binary-plist header rejection
- truncated trailer / truncated data rejection

Required interpretation rules:

- raw `CFTypeID` numbers are diagnostic only across implementations
- exact binary layout for container object ordering is not required in this
  slice unless later fixtures prove it is stable and necessary
- exact `CFError` wording is diagnostic only
- error kind is judged by:
  - returned object is `NULL`
  - error object is present
  - error code matches the expected public category
- the public format out-parameter must report
  `kCFPropertyListBinaryFormat_v1_0` for successful binary parses
- repeated-equal-object pointer identity is diagnostic only in this slice

## Required FX Behavior

### Write path

- `CFPropertyListCreateData` accepts the supported immutable property-list
  graph and returns binary plist data
- written data begins with `bplist00`
- ASCII strings use the ASCII string write path in the tested cases
- non-ASCII strings use the UTF-16 string write path in the tested cases
- arrays and dictionaries serialize the already-validated child objects
- dictionaries with non-string keys are rejected safely

### Read path

- `CFPropertyListCreateWithData` accepts valid `bplist00` data for the tested
  object graph and returns an immutable property-list object graph
- the returned `format` reports binary format
- parsed objects preserve the semantic values of the written graph for the
  tested types
- malformed input returns `NULL` with an error object and no crash
- truncated or structurally invalid data is rejected safely

## Acceptable FX Variance

- raw `CFTypeID` numbers do not need to match macOS
- exact `CFError` text does not need to match macOS
- exact container-object ordering in the emitted object table may differ when
  the semantic graph is preserved and MX/FX cross-parse remains clean
- internal presizing, buffer growth, and offset-table construction strategy may
  differ from macOS

## Rejected In This Slice

- importing upstream `CFBinaryPList.c` and `CFPropertyList.c` unchanged
- starting with XML or stream APIs before the in-memory binary path is proven
- starting with mutable parse modes before immutable semantics are clean
- treating UID/set/ordset support as required for LaunchX packet work
- widening the compare surface to exact error strings

## Subset 3A Semantic Ledger Seed

| Subset | API or behavior | MX observation artifact | FX implementation status | Match level | Known variance | Test name | Migration status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 3A | ASCII string root write/read roundtrip | `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json` | Implemented in `cort-fx` | `match` | type numerics diagnostic | `bplist_ascii_string_roundtrip` | FX/MX compare clean |
| 3A | UTF-16 string root write/read roundtrip | `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json` | Implemented in `cort-fx` | `match` | type numerics diagnostic | `bplist_unicode_string_roundtrip` | FX/MX compare clean |
| 3A | mixed scalar array write/read roundtrip | `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json` | Implemented in `cort-fx` | `match` | exact container object-table ordering and raw type numerics diagnostic | `bplist_mixed_array_roundtrip` | FX/MX compare clean |
| 3A | string-key dictionary write/read roundtrip | `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json` | Implemented in `cort-fx` | `match` | exact container object-table ordering and raw type numerics diagnostic | `bplist_string_key_dictionary_roundtrip` | FX/MX compare clean |
| 3A | non-string dictionary key write rejection | `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json` | Implemented in `cort-fx` | `match` | error text diagnostic only | `bplist_non_string_dictionary_key_write_rejected` | FX/MX compare clean |
| 3A | invalid header rejection | `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json` | Implemented in `cort-fx` | `match` | error text diagnostic only | `bplist_invalid_header_rejected` | FX/MX compare clean |
| 3A | truncated trailer rejection | `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json` | Implemented in `cort-fx` | `match` | error text diagnostic only | `bplist_truncated_trailer_rejected` | FX/MX compare clean |

## MX Probe Assets

Probe source:

- `cort-mx/src/cort_mx_subset3a_bplist_core.c`

Expectation manifest:

- `cort-mx/expectations/subset3a_bplist_core_expected.json`

Run scripts:

- `cort-mx/scripts/run_subset3a_bplist_core.sh`
- `cort-mx/scripts/run_subset3a_bplist_compare.sh`
- `cort-mx/scripts/run_subset3a_suite.sh`

FX compare-artifact wrapper:

- `cort-fx/scripts/run_subset3a_compare_artifact.sh`

Future FX-vs-MX compare tool:

- `tools/compare_subset3a_bplist_json.exs`

FX readiness note:

- `docs/cort-subset3a-source-audit-and-readiness.md`
- `docs/cort-subset3a-fx-implementation-plan.md`
- `docs/cort-subset3a-fixture-corpus.md`

## Expected MX Artifacts

The MX run should preserve:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- probe source under `src/`
- probe binary under `bin/`
- JSON observations under `out/`
- generated `.bplist` fixtures under `fixtures/`

## Exact MX Command

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset3a_bplist_core.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/`

## Exit Gate

Subset 3A is ready for FX implementation when:

- MX run artifacts exist
- the report has no blocker probe failures
- write/read roundtrip semantics are recorded explicitly for the supported
  scalar, string, array, and dictionary cases
- rejection behavior is recorded explicitly for invalid header and truncated
  input
- non-string dictionary key write rejection is recorded explicitly
- the team accepts that the first implementation is a clean rewrite, not a
  direct `CFBinaryPList.c` extraction project
