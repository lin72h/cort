# CORT Subset 2A Container Core Contract

Date: 2026-04-17

Status: MX validation clean on the tightened 2A surface; compare workflow is
packaged; FX implementation has not started.

This document defines the next narrow semantic slice after Subset 1B:
minimal array and dictionary semantics for plist-valid object graphs.

Purpose:

- validate the smallest useful container surface on macOS before FX container
  implementation starts
- support plist-valid arrays and string-key dictionaries without committing to
  the full CoreFoundation callback matrix
- keep the first container slice narrow enough that ownership and borrowed
  getter semantics stay explicit

## Why This Is Subset 2A, Not Full Subset 2

The original container subset includes:

- immutable array creation
- mutable array append/remove
- immutable dictionary creation
- mutable dictionary set/remove
- key retention
- value retention
- copied objects following Create/Copy ownership
- Get APIs returning borrowed references
- dictionary lookup working for string keys

That is still the long-term container target, but the full public surface is
broader:

- custom callback structs
- `NULL` callback modes
- copy-string dictionary key callbacks
- extra mutable operations like insert, replace, exchange, and remove-all
- enumeration, apply, and bulk getter APIs
- container `CFEqual`/`CFHash`/`CFCopyDescription`

So the next useful execution slice is:

```text
Subset 2A:
  CFArray with kCFTypeArrayCallBacks only
  CFDictionary with kCFTypeDictionaryKeyCallBacks and
  kCFTypeDictionaryValueCallBacks only
  immutable create/copy
  mutable append/set/remove
  borrowed Get semantics
  string-key dictionary lookup

Future widening, if needed:
  custom/NULL callback modes
  copy-string key callbacks
  broader mutation APIs
  enumeration and container equality/hash
```

This keeps the semantics-first loop intact instead of smuggling in the full
callback matrix before LaunchX needs it.

## Scope

This is not “all of `CFArray` and `CFDictionary`”.

This slice is the minimum container core that can later support:

- plist-valid arrays of previously validated scalar/string objects
- plist-valid dictionaries keyed by validated `CFString`
- future bplist array and dictionary read/write paths
- future LaunchX packet dictionaries with string keys

It is deliberately smaller than the public CoreFoundation headers.

## Included APIs

### Type and ownership

- `CFArrayGetTypeID`
- `CFDictionaryGetTypeID`
- `CFRetain`
- `CFRelease`
- `CFGetTypeID`

### `CFArray`

Supported callback mode in this slice:

- `kCFTypeArrayCallBacks`

APIs:

- `CFArrayCreate`
- `CFArrayCreateCopy`
- `CFArrayCreateMutable`
- `CFArrayGetCount`
- `CFArrayGetValueAtIndex`
- `CFArrayAppendValue`
- `CFArrayRemoveValueAtIndex`

### `CFDictionary`

Supported callback modes in this slice:

- `kCFTypeDictionaryKeyCallBacks`
- `kCFTypeDictionaryValueCallBacks`

APIs:

- `CFDictionaryCreate`
- `CFDictionaryCreateCopy`
- `CFDictionaryCreateMutable`
- `CFDictionaryGetCount`
- `CFDictionaryGetValue`
- `CFDictionaryGetValueIfPresent`
- `CFDictionarySetValue`
- `CFDictionaryRemoveValue`

## Explicitly Excluded

- custom `CFArrayCallBacks`
- custom `CFDictionaryKeyCallBacks`
- custom `CFDictionaryValueCallBacks`
- `NULL` callback modes
- `kCFCopyStringDictionaryKeyCallBacks`
- `CFArraySetValueAtIndex`
- `CFArrayInsertValueAtIndex`
- `CFArrayExchangeValuesAtIndices`
- `CFArrayRemoveAllValues`
- `CFDictionaryCreateMutableCopy`
- `CFDictionaryGetKeysAndValues`
- `CFDictionaryApplyFunction`
- `CFDictionaryContainsKey`
- `CFDictionaryContainsValue`
- `CFDictionaryGetCountOfKey`
- `CFDictionaryGetCountOfValue`
- `CFDictionaryGetKeyIfPresent`
- container `CFEqual`
- container `CFHash`
- container `CFCopyDescription`
- `CFSet`, `CFBag`, plist and bplist themselves
- Objective-C bridge behavior

## Required MX Observations

For the tested surface, MX must preserve observations for:

- empty immutable array creation with count `0`
- immutable array creation with `kCFTypeArrayCallBacks`
- borrowed getter semantics for `CFArrayGetValueAtIndex`
- mutable array append/remove ownership with `kCFTypeArrayCallBacks`
- immutable array Create/Copy ownership with child pointer identity preserved
- empty immutable dictionary creation with count `0`
- immutable dictionary creation with `kCFType` key/value callbacks
- string-key lookup through `CFDictionaryGetValue`
- present-hit / absent-miss semantics through `CFDictionaryGetValueIfPresent`
- borrowed getter semantics for dictionary values
- mutable dictionary set/remove ownership with `kCFType` key/value callbacks
- replacement of a value for an existing key releasing the old value
- immutable dictionary Create/Copy ownership with child pointer identity
  preserved

Required interpretation rules:

- raw `CFTypeID` numbers are diagnostic only across implementations
- raw retain-count numerics are diagnostic only across implementations
- container copy pointer identity is diagnostic only; owned-copy semantics
  matter, not whether the container pointer is reused
- borrowed getter semantics are judged by local success conditions inside the
  probe, not by comparing raw retain-count numerics across implementations
- child pointer identity returned by the tested Get APIs is required for the
  validated `kCFType` callback mode in this slice

## Required FX Behavior

### `CFArray`

- immutable arrays preserve inserted order
- mutable arrays append at the end
- removing an index compacts the remaining elements
- `CFArrayGetCount` returns the current element count
- `CFArrayGetValueAtIndex` returns a borrowed child reference
- with `kCFTypeArrayCallBacks`, insertion retains and removal releases
- container deallocation releases any remaining children
- `CFArrayCreateCopy` returns an owned container with the same semantic
  contents and borrowed child pointers for the tested elements

### `CFDictionary`

- immutable dictionaries preserve the inserted key/value associations
- mutable dictionaries set, replace, and remove values for existing string keys
- `CFDictionaryGetCount` returns the current entry count
- `CFDictionaryGetValue` returns a borrowed value reference for a present key
- `CFDictionaryGetValueIfPresent` returns `true` plus the borrowed value for a
  present key, and `false` for an absent key
- with `kCFTypeDictionaryKeyCallBacks` and
  `kCFTypeDictionaryValueCallBacks`, insertion retains keys and values
- replacement retains the new value and releases the old value
- removal releases the key and value
- container deallocation releases any remaining keys and values
- dictionary lookup works for validated `CFString` keys
- `CFDictionaryCreateCopy` returns an owned container with the same semantic
  contents and borrowed value pointers for the tested entries

## Acceptable FX Variance

- raw `CFTypeID` numbers do not need to match macOS
- raw retain-count numerics do not need to match macOS
- internal storage layout may differ from macOS
- capacity growth strategy may differ from macOS
- array and dictionary copy pointer identity may differ from macOS
- future support for custom or `NULL` callbacks may use a different internal
  dispatch strategy

## Rejected In This Slice

- importing upstream `CFArray.c`, `CFDictionary.c`, or `CFBasicHash.c`
  unchanged
- broad callback compatibility before LaunchX needs it
- treating copy-string keys as a requirement for the first container slice
- implementing enumeration, apply, or container equality/hash as a prerequisite
- starting bplist work before container ownership semantics are classified

## Subset 2A Semantic Ledger Seed

| Subset | API or behavior | MX observation artifact | FX implementation status | Match level | Known variance | Test name | Migration status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2A | empty immutable array creation | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | type numerics diagnostic | `cfarray_empty_immutable` | awaiting final MX compare |
| 2A | immutable array borrowed getter with `kCFTypeArrayCallBacks` | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | type numerics diagnostic | `cfarray_immutable_cftype_borrowed_get` | awaiting final MX compare |
| 2A | mutable array append/remove retain-release semantics | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | raw retain-count numerics diagnostic | `cfarray_mutable_append_remove_retains` | awaiting final MX compare |
| 2A | array CreateCopy ownership with borrowed child identity | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | container copy pointer identity diagnostic only | `cfarray_createcopy_borrowed_get` | awaiting final MX compare |
| 2A | empty immutable dictionary creation | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | type numerics diagnostic | `cfdictionary_empty_immutable` | awaiting final MX compare |
| 2A | immutable dictionary string-key lookup and borrowed value getter | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | type numerics diagnostic | `cfdictionary_immutable_string_key_lookup` | awaiting final MX compare |
| 2A | mutable dictionary set/replace/remove retain-release semantics | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | raw retain-count numerics diagnostic | `cfdictionary_mutable_set_replace_remove_retains` | awaiting final MX compare |
| 2A | dictionary CreateCopy ownership with borrowed value identity | `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json` | Implemented locally | `unknown` | container copy pointer identity diagnostic only | `cfdictionary_createcopy_borrowed_lookup` | awaiting final MX compare |

## MX Probe Assets

Probe source:

- `cort-mx/src/cort_mx_subset2a_container_core.c`

Expectation manifest:

- `cort-mx/expectations/subset2a_container_core_expected.json`

Run script:

- `cort-mx/scripts/run_subset2a_container_core.sh`

MX compare wrapper:

- `cort-mx/scripts/run_subset2a_container_compare.sh`

MX suite wrapper:

- `cort-mx/scripts/run_subset2a_suite.sh`

FX-vs-MX compare tool:

- `tools/compare_subset2a_container_json.exs`

FX compare-artifact wrapper:

- `cort-fx/scripts/run_subset2a_compare_artifact.sh`

FX readiness note:

- `docs/cort-subset2a-source-audit-and-readiness.md`

## Expected MX Artifacts

The MX run should preserve:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- `src/cort_mx_subset2a_container_core.c`
- `bin/cort_mx_subset2a_container_core`
- `out/subset2a_container_core.json`
- `out/subset2a_container_core.stdout`
- `out/subset2a_container_core.stderr`

The MX compare wrapper should preserve when an FX JSON is available:

- `summary.md`
- `out/subset2a_container_fx_vs_mx_report.md`
- `out/subset2a_container_fx.json`
- `out/subset2a_container_mx.json`

## Exact MX Command

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset2a_container_core.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/`

Preferred coordination command once FX publishes `subset2a_container_fx.json`:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset2a_suite.sh
```

Default suite output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-suite/`

FX can also preserve a compare-only handoff run once it has a real
`subset2a_container_fx.json`:

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-fx
make artifact-subset2a-compare \
  FX_CONTAINER_JSON=/path/to/subset2a_container_fx.json \
  MX_JSON=/path/to/subset2a_container_core.json
```

## Exit Gate

Subset 2A is ready for FX implementation when:

- MX run artifacts exist
- the report has no blocker probe failures
- empty array and dictionary semantics are recorded explicitly
- borrowed Get semantics are recorded explicitly
- array append/remove ownership is recorded explicitly
- dictionary set/replace/remove ownership is recorded explicitly
- string-key dictionary lookup is recorded explicitly
- excluded callback modes remain excluded instead of leaking into the slice

Current MX report status for the artifact above:

- blockers: `0`
- warnings: `0`
- verdict: no blocking issues found against the current manifest
