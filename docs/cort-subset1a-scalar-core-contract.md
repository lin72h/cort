# CORT Subset 1A Scalar Core Contract

Date: 2026-04-16

Status: MX validation clean; local FX implementation and FX scalar-core probe
exist.

This document defines the next narrow semantic slice after validated Subset 0:
immutable scalar core without `CFString`.

Purpose:

- validate public scalar value semantics on macOS before FX implementation
- advance booleans, numbers, data, and dates without pretending `CFString` is
  a small follow-on detail
- keep the next implementation slice narrow enough that its semantics fit on
  one page

## Why This Is Subset 1A, Not Full Subset 1

The original scalar subset included:

- `CFBoolean`
- `CFNumber`
- `CFDate`
- `CFData`
- minimal `CFString`

That remains the long-term scalar target, but `CFString` is the hardest member
of the set and already has its own architectural warning:

- minimal string support is required later for bplist and plist-valid keys
- full upstream `CFString` extraction is likely worse than a small direct
  implementation for the first supported slice
- locale, normalization, ICU, and broad string surface must stay out of the
  first validated scalar implementation

So the next useful execution slice is:

```text
Subset 1A:
  CFBoolean
  CFNumber (tested numeric forms only)
  CFData
  CFDate

Subset 1B:
  minimal CFString for bplist and plist-valid key paths
```

This keeps the semantics-first loop intact instead of either:

- stalling all scalar work on `CFString`, or
- smuggling `CFString` in without a dedicated contract

## Included APIs

### `CFBoolean`

- `kCFBooleanTrue`
- `kCFBooleanFalse`
- `CFBooleanGetValue`
- `CFRetain`
- `CFRelease`
- `CFGetTypeID`
- `CFEqual`
- `CFHash`

### `CFNumber`

Tested forms in this slice:

- `kCFNumberSInt32Type`
- `kCFNumberSInt64Type`
- `kCFNumberFloat64Type`

APIs:

- `CFNumberCreate`
- `CFNumberGetValue`
- `CFNumberGetType`
- `CFRetain`
- `CFRelease`
- `CFGetTypeID`
- `CFEqual`
- `CFHash`

### `CFData`

- `CFDataCreate`
- `CFDataCreateCopy`
- `CFDataGetLength`
- `CFDataGetBytePtr`
- `CFRetain`
- `CFRelease`
- `CFGetTypeID`
- `CFEqual`
- `CFHash`

### `CFDate`

- `CFDateCreate`
- `CFDateGetAbsoluteTime`
- `CFRetain`
- `CFRelease`
- `CFGetTypeID`
- `CFEqual`
- `CFHash`

## Explicitly Excluded

- all public `CFString` creation and access APIs
- locale-sensitive string comparison
- Unicode normalization, case folding, ICU-backed behavior
- mutable scalars
- containers
- plist and bplist
- XML plist
- Objective-C bridge behavior

## Required MX Observations

For each included scalar type, capture:

- public type identity via `CFGetTypeID`
- value extraction through the type's public getter APIs
- Create/Copy ownership where the type has a Create/Copy path
- equality behavior through `CFEqual`
- hash coherence through `CFHash`

Required interpretation rules:

- raw `CFTypeID` numbers are diagnostic only across implementations
- raw `CFHash` numbers are diagnostic only across implementations
- required semantic match is about public value/equality/hash coherence, not
  byte-for-byte parity of numeric diagnostics

## Required FX Behavior

### `CFBoolean`

- expose process-singleton `true` and `false` objects
- `CFBooleanGetValue` returns the correct logical value
- `CFRetain` returns the same pointer
- `CFRelease` does not deallocate the singleton
- equal booleans hash coherently within the process

### `CFData`

- immutable byte storage
- `CFDataGetLength` returns the stored byte length
- `CFDataGetBytePtr` exposes the stored bytes for immutable data
- copied equal data compares equal and hashes coherently

### `CFNumber`

- preserve roundtrip value semantics for the tested numeric forms
- `CFNumberGetType` remains stable for the stored representation in the initial
  implementation
- equal tested numbers hash coherently within the process
- cross-type numeric equality is observed in 1A but not yet promoted to a hard
  requirement

### `CFDate`

- preserve the stored absolute time
- equal dates compare equal and hash coherently within the process

## Acceptable FX Variance

- raw `CFHash` numbers do not need to match macOS
- raw `CFTypeID` numbers do not need to match macOS
- immortal singleton retain-count magnitudes remain diagnostic only
- cross-type `CFNumber` equality may remain provisional until MX evidence is
  reviewed and LaunchX decides whether it matters

## Rejected In This Slice

- broad “all scalar types at once including strings” implementation
- any ICU or locale dependency
- Objective-C bridge semantics
- string-dependent debug-description work used as a blocker for scalar core

## Subset 1A Semantic Ledger Seed

| Subset | API or behavior | MX observation artifact | FX implementation status | Match level | Known variance | Test name | Migration status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1A | `CFBoolean` singleton identity and value | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json` | Implemented locally in `cort-fx`; FX probe emits `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx.json` | `semantic` | retain-count numerics diagnostic | `cfboolean_true_singleton`, `cfboolean_false_singleton` | FX local done; FX-vs-MX compare pending |
| 1A | `CFBoolean` equality/hash coherence | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json` | Implemented locally | `semantic` | raw hash numerics diagnostic | `cfboolean_equality_hash` | FX local done; FX-vs-MX compare pending |
| 1A | `CFData` length/bytes/equality/hash | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json` | Implemented locally | `semantic` | raw hash numerics diagnostic | `cfdata_value_roundtrip` | FX local done; FX-vs-MX compare pending |
| 1A | `CFNumber` `SInt32` roundtrip/equality/hash | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json` | Implemented locally | `semantic` | raw hash numerics diagnostic | `cfnumber_sint32_roundtrip` | FX local done; FX-vs-MX compare pending |
| 1A | `CFNumber` `Float64` roundtrip/equality/hash | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json` | Implemented locally | `semantic` | raw hash numerics diagnostic | `cfnumber_float64_roundtrip` | FX local done; FX-vs-MX compare pending |
| 1A | `CFNumber` cross-type equality observation | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json` | Implemented locally to match observed `42` integer/float equality on tested forms | `semantic` | remains semantic-only until broader MX classification | `cfnumber_cross_type_equality` | FX local done; FX-vs-MX compare pending |
| 1A | `CFDate` absolute-time roundtrip/equality/hash | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json` | Implemented locally | `semantic` | raw hash numerics diagnostic | `cfdate_absolute_time_roundtrip` | FX local done; FX-vs-MX compare pending |
| 1B | minimal `CFString` for bplist/key paths | Not started | Not implemented | `unknown` | dedicated slice required | Deferred | Not ready |

The MX gate for local FX Subset 1A implementation is now satisfied. The next
gate is FX-vs-MX comparison on the emitted FX JSON artifact.

## MX Probe Assets

Probe source:

- `cort-mx/src/cort_mx_subset1_scalar_core.c`

Expectation manifest:

- `cort-mx/expectations/subset1_scalar_core_expected.json`

Run script:

- `cort-mx/scripts/run_subset1_scalar_core.sh`

FX source audit and pre-implementation readiness:

- `docs/cort-subset1a-source-audit-and-readiness.md`

## Expected MX Artifacts

The MX run should preserve:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- `src/cort_mx_subset1_scalar_core.c`
- `bin/cort_mx_subset1_scalar_core`
- `out/subset1_scalar_core.json`
- `out/subset1_scalar_core.stdout`
- `out/subset1_scalar_core.stderr`

## Exact MX Command

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset1_scalar_core.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/`

## Exit Gate

Subset 1A is ready for FX-vs-MX semantic comparison when:

- MX run artifacts exist
- the report has no blocker probe failures
- the required rows have concrete artifact paths in the ledger
- any semantic-only `CFNumber` cross-type behavior is classified explicitly
- `CFString` remains deferred to a dedicated 1B contract instead of leaking back
  into 1A

The FX local implementation now satisfies that gate and emits:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx.json`
