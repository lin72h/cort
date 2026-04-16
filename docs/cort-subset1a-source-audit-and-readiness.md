# CORT Subset 1A Source Audit And FX Readiness

Date: 2026-04-16

Status: source audit complete; MX gate satisfied; local FX scalar
implementation now exists; host-aware FX verification now passes on Darwin.

This document is the extraction and implementation-readiness note for CORT
Subset 1A: immutable scalar core without `CFString`.

It does not replace the semantic contract in
`docs/cort-subset1a-scalar-core-contract.md`. It answers a narrower question:
which upstream CoreFoundation source can inform FX, which source must be
excluded or rewritten, and what must be ready once MX validates the public
semantics.

## Source Basis

Working directory:

- `/Users/me/wip-launchx/wip-cort-gpt`

Audited source checkout:

- `../nx/swift-corelibs-foundation`
  - commit `f9c1d4eee71957c49eb66fd8fb5dac856d0efbf4`
  - `f9c1d4ee Cleanup swapped item after renameat2 call in FileManager (#5454)`

Primary files inspected:

- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFData.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFNumber.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFDate.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFData.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFNumber.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFDate.h`

Existing FX files inspected for fit:

- `cort-fx/include/CoreFoundation/CFBase.h`
- `cort-fx/include/CoreFoundation/CFRuntime.h`
- `cort-fx/include/CoreFoundation/CoreFoundation.h`
- `cort-fx/src/FXCFInternal.h`
- `cort-fx/src/CFBase.c`
- `cort-fx/src/CFRuntime.c`
- `cort-fx/Makefile`

## Current Decision

Do not copy `CFData.c`, `CFNumber.c`, or `CFDate.c` directly into `cort-fx`.

Use them as algorithm references only. The FX implementation should be clean
and small because Subset 1A is deliberately narrower than the upstream
translation units:

- `CFData.c` mixes immutable data, mutable data, no-copy data, inline storage,
  Swift dispatch paths, private runtime flag helpers, and string descriptions.
- `CFNumber.c` has useful conversion, equality, and hash logic, but also
  static number constants, integer caching, dispatch initialization, ObjC and
  Swift dispatch paths, and string formatting.
- `CFDate.c` contains the small `CFDate` object shape needed by 1A, but the
  file also owns timebase, Gregorian calendar, time-zone, array, dictionary,
  string, dispatch, and platform clock paths.
- The public headers expose broader CoreFoundation surfaces than 1A will
  implement.

This matches the Subset 0 precedent: preserve public C semantics, keep the
runtime pure C, and avoid importing broad upstream coupling just to get a few
validated APIs.

## Dependency Scan

Search terms used against the files above:

```text
_foundation_unicode uchar.h ucnv_ ubrk_ ucol_ unorm_ ucase_
#include <dispatch/ dispatch_once dispatch_apply dispatch_queue dispatch_async
DEPLOYMENT_RUNTIME_SWIFT swift_allocObject swift_retain swift_release _swift_rc CF_IS_SWIFT
CF_IS_OBJC objc_msgSend objc_retain NSObject NSCF CF_BRIDGED
TARGET_OS_MAC TARGET_OS_IPHONE CFString CFArray CFDictionary CFTimeZone CFCalendar
mach_absolute_time mach/mach.h
```

High-level result:

- no ICU dependency appears in the audited 1A files
- dispatch appears in `CFDate.c` and `CFNumber.c`
- Swift dispatch/runtime macros appear in `CFData.c` and `CFNumber.c`
- ObjC bridge declarations and dispatch macros appear in the headers and source
- `CFDate.c` has broad `CFString`, `CFArray`, `CFDictionary`, and `CFTimeZone`
  coupling that is out of scope for 1A
- `CFData.c`, `CFNumber.c`, and `CFDate.c` all depend on private runtime and
  internal headers that are not suitable as FX public or build dependencies

## Dependency Audit Table

| Source file | Dependency or symbol | Why it appears | Subset 1A relevance | Classification | Proposed FX action | Blocker status |
| --- | --- | --- | --- | --- | --- | --- |
| `include/CFData.h` | `CF_BRIDGED_TYPE(NSData)`, `CF_BRIDGED_MUTABLE_TYPE(NSMutableData)` | Apple/CoreFoundation bridge annotations | Not part of FX; no ObjC bridge | `exclude` | Define plain opaque `struct __CFData *` types in FX header | Not blocking |
| `include/CFData.h` | `CFDataCreateWithBytesNoCopy`, mutable data APIs, `CFDataFind` | Broader CFData surface | Excluded from 1A | `defer` | Omit from initial FX header or leave unexported until a later subset | Not blocking |
| `CFData.c` | `DEPLOYMENT_RUNTIME_SWIFT`, `CF_SWIFT_NSDATA_FUNCDISPATCHV`, `CF_IS_SWIFT` | Swift Foundation dispatch integration | Rejected by FX platform model | `exclude` | Do not import; implement direct C object paths | Blocking for source copy, not blocking for clean implementation |
| `CFData.c` | `CFInternal.h`, `CFRuntime_Internal.h`, `CFPriv.h` | Private runtime flags, assertions, hash helpers, internal allocator behavior | Some concepts relevant, headers too broad | `replace-with-local` | Use existing `FXCFInternal.h`; add only required local helpers | Not blocking |
| `CFData.c` | `mach/mach.h`, `TARGET_OS_MAC`, page-size inline threshold | Platform tuning for inline bytes | Immutable 1A data does not need this | `defer` | Store bytes in one simple allocation; revisit inline storage after semantic parity | Not blocking |
| `CFData.c` | `CFStringCreateWithFormat`, `CFStringAppendFormat`, `CFSTR` | debug/copy description | `CFCopyDescription` not in 1A | `defer` | Leave description callbacks `NULL` until CFString exists | Not blocking |
| `CFData.c` | mutable/growable flags and capacity helpers | Mutable data implementation | Excluded from 1A | `defer` | Implement immutable fixed-length bytes only | Not blocking |
| `CFData.c` | `CFHashBytes`, `memcmp` equality | Public value equality/hash | Relevant concept | `required-for-subset` | Implement local byte equality and hash; raw hash numerics remain diagnostic | Not blocking |
| `include/CFNumber.h` | `CF_BRIDGED_TYPE(NSNumber)` | ObjC bridge annotation | Rejected by FX platform model | `exclude` | Define plain opaque boolean and number refs | Not blocking |
| `include/CFNumber.h` | full `CFNumberType` enum | Public number ABI | 1A uses only `SInt32`, `SInt64`, `Float64` | `required-for-subset` | Include enum values needed by tested API; aliases can be added without implementing all conversions | Not blocking |
| `include/CFNumber.h` | `kCFNumberPositiveInfinity`, `kCFNumberNegativeInfinity`, `kCFNumberNaN` | Native singleton constants for special floats | Not tested in 1A | `defer` | Do not export constants until MX validates special-float behavior | Not blocking |
| `CFNumber.c` | `DECLARE_STATIC_CLASS_REF`, `INIT_CFRUNTIME_BASE_WITH_CLASS_AND_FLAGS` | Static boolean and number constants | Boolean singleton relevant; number special constants deferred | `replace-with-local` | Use Subset 0 static immortal-object pattern for booleans only | Not blocking |
| `CFNumber.c` | `CFStringCreateWithFormat`, `CFStringAppend`, `CFSTR`, formatting-description helpers | number descriptions and plist formatting support | Out of scope before CFString and plist | `defer` | Leave description callbacks `NULL`; add formatting only when CFString/plist needs it | Not blocking |
| `CFNumber.c` | `dispatch_once` | environment-driven integer-cache initialization | Integer cache not required for 1A | `defer` | No cache in first FX implementation; use heap numbers | Not blocking |
| `CFNumber.c` | `CF_OBJC_FUNCDISPATCHV`, `CF_SWIFT_FUNCDISPATCHV`, `CF_IS_OBJC` | ObjC and Swift number bridging | Rejected by FX platform model | `exclude` | Direct C-only validation and operations | Blocking for source copy, not blocking for clean implementation |
| `CFNumber.c` | `__CFNumberTypeTable`, `__CFNumberGetValueCompat`, `CFNumberCompare` | numeric conversion/comparison semantics | Relevant, but broader than first tests | `required-for-subset` | Adapt only tested `SInt32`, `SInt64`, and `Float64` conversion/equality paths after MX classification | Not blocking |
| `CFNumber.c` | `_CFHashInt`, `_CFHashDouble` | hash coherence for numeric values | Relevant concept | `required-for-subset` | Implement local hash helpers; require same-value hash coherence, not MX numeric parity | Not blocking |
| `include/CFDate.h` | `CF_BRIDGED_TYPE(NSDate)`, `CFTimeZoneRef` bridge | ObjC bridge and calendar/time-zone API | Bridge and time-zone paths excluded | `exclude` | Define plain `CFDateRef`; omit Gregorian/time-zone APIs in 1A FX header | Not blocking |
| `include/CFDate.h` | `CFAbsoluteTimeGetCurrent`, epoch constants, `CFDateCompare`, `CFDateGetTimeIntervalSinceDate` | Broader date API than probe uses | Not required by current 1A MX probe | `defer` | Start with `CFDateCreate` and `CFDateGetAbsoluteTime`; add compare/time only after explicit validation | Not blocking |
| `CFDate.c` | `CFTimeZone.h`, `CFDictionary.h`, `CFArray.h`, `CFString.h`, `CFNumber.h` | Gregorian/calendar and descriptions | Excluded from 1A | `exclude` | Do not copy `CFDate.c`; implement the tiny date object directly | Blocking for source copy, not blocking for clean implementation |
| `CFDate.c` | `<dispatch/dispatch.h>` | timebase/thread-safe current-time initialization | Not needed for stored-date roundtrip | `defer` | Avoid `CFAbsoluteTimeGetCurrent` in 1A | Not blocking |
| `CFDate.c` | `mach_absolute_time`, `TARGET_OS_MAC`, platform clock code | high-resolution current time | Not needed for `CFDateCreate`/`CFDateGetAbsoluteTime` | `defer` | Exclude current-time implementation until validated | Not blocking |
| `CFDate.c` | `__CFDateEqual`, `__CFDateHash`, `CFDateCreate`, `CFDateGetAbsoluteTime` | stored absolute-time object semantics | Directly relevant | `required-for-subset` | Reimplement simple object with `double` storage, equality, and coherent hash | Not blocking |
| `CFData.c`, `CFNumber.c`, `CFDate.c` | `CFRuntime_Internal.h` and runtime flag helpers | upstream packed metadata and type-specific flags | FX has its own `_cfinfoa` layout | `replace-with-local` | Add type-private fields in object structs instead of upstream runtime flag packing unless there is a measured need | Not blocking |
| `CFData.c`, `CFNumber.c`, `CFDate.c` | `CFInternal.h` transitive includes | broad internal CoreFoundation convenience layer | Too broad for FX | `replace-with-local` | Pull individual ideas only; never include this header in FX | Blocking for source copy, not blocking for clean implementation |

## Public ABI Readiness

Subset 1A will require new public APIs beyond the current Subset 0 export set.
After MX artifacts are classified, the FX implementation should add:

- `CFEqual`
- `CFHash`
- `CFBooleanGetTypeID`
- `CFBooleanGetValue`
- `kCFBooleanTrue`
- `kCFBooleanFalse`
- `CFDataGetTypeID`
- `CFDataCreate`
- `CFDataCreateCopy`
- `CFDataGetLength`
- `CFDataGetBytePtr`
- `CFNumberGetTypeID`
- `CFNumberCreate`
- `CFNumberGetType`
- `CFNumberGetValue`
- `CFDateGetTypeID`
- `CFDateCreate`
- `CFDateGetAbsoluteTime`

Header plan:

- add `cort-fx/include/CoreFoundation/CFData.h`
- add `cort-fx/include/CoreFoundation/CFNumber.h`
- add `cort-fx/include/CoreFoundation/CFDate.h`
- include those from `cort-fx/include/CoreFoundation/CoreFoundation.h`
- keep declarations to the validated 1A surface rather than copying full
  upstream headers
- strip ObjC bridge annotations from FX headers

Export plan:

- update `cort-fx/exports/subset0-exported-symbols.txt` only when the exported
  surface intentionally moves from Subset 0 to Subset 1A, or add a new
  `subset1a-exported-symbols.txt` if both snapshots need to coexist
- keep `make check-exports` strict

## Internal Runtime Readiness

The current Subset 0 runtime already has the required class callback slots:

- `finalize`
- `equal`
- `hash`
- debug and formatting description callbacks, which can remain `NULL`

Required internal additions after MX validation:

- reserve process-local type IDs for `CFBoolean`, `CFNumber`, `CFData`, and
  `CFDate`
- publish the scalar classes during runtime initialization
- implement `CFEqual` using pointer identity, type identity, then the class
  `equal` callback
- implement `CFHash` using the class `hash` callback where present
- keep raw hash numeric values implementation-local; only same-value coherence
  is required

The type ID numbers must remain stable within one process, but they do not need
to match macOS.

## Per-Type FX Shape

### `CFBoolean`

Recommended first shape:

```c
struct __CFBoolean {
    CFRuntimeBase _base;
    Boolean value;
};
```

Implementation notes:

- create two compile-time static objects
- initialize `_cfisa` directly to a static boolean class
- mark both objects immortal using the Subset 0 static-object rule
- `CFRetain` returns the same pointer
- `CFRelease` is a no-op
- `CFBooleanGetValue` validates the type and returns the stored value
- no finalizer should ever run

### `CFData`

Recommended first shape:

```c
struct __CFData {
    CFRuntimeBase _base;
    CFIndex length;
    UInt8 bytes[];
};
```

Implementation notes:

- `CFDataCreate` copies bytes into object-owned storage
- `CFDataCreateCopy` can create a new immutable copy for 1A unless MX
  validation promotes same-pointer copy elision to a requirement
- zero-length data is valid
- negative length returns `NULL` or aborts only if MX evidence requires it;
  classify this before implementation
- `CFDataGetBytePtr` returns a borrowed pointer valid for the object lifetime
- equality is byte length plus `memcmp`
- hash is local and stable for equal data within the process
- no no-copy bytes, mutable bytes, capacity growth, or inline page-size tuning
  in 1A

### `CFNumber`

Recommended first shape:

```c
struct __CFNumber {
    CFRuntimeBase _base;
    CFNumberType type;
    union {
        SInt64 i64;
        double f64;
    } value;
};
```

Implementation notes:

- implement only validated `kCFNumberSInt32Type`, `kCFNumberSInt64Type`, and
  `kCFNumberFloat64Type` behavior first
- `CFNumberGetType` should return the stored canonical type chosen by FX for
  that object
- `CFNumberGetValue` should support the MX-tested request types first
- integer-to-integer and exact-integer float comparison are required for the
  tested Subset 1A forms
- do not generalize that into broad `CFNumber` canonicalization beyond the
  current tested forms without a new MX slice
- NaN and infinity singleton constants are out of 1A unless the MX probe is
  expanded and the contract is amended
- no integer cache in the first FX implementation

### `CFDate`

Recommended first shape:

```c
struct __CFDate {
    CFRuntimeBase _base;
    CFAbsoluteTime at;
};
```

Implementation notes:

- store exactly the `CFAbsoluteTime` passed to `CFDateCreate`
- `CFDateGetAbsoluteTime` returns that stored value
- equality is stored-time equality after MX confirms the expected treatment for
  ordinary finite values
- hash must be coherent for equal dates; exact macOS numeric hash is diagnostic
- do not implement `CFAbsoluteTimeGetCurrent`, Gregorian APIs, time-zone APIs,
  or platform clock paths in 1A

## Tests To Add After MX Validation

FX tests should be added only after the MX `subset1_scalar_core` artifacts are
available and classified.

Required first tests:

- boolean true/false singleton identity, value, retain/release immortality
- boolean equality/hash coherence
- data create/copy length, byte pointer, equality, hash coherence
- number create/get for `SInt32`, `SInt64`, and `Float64`
- number equality/hash coherence for same-value numbers
- cross-type number equality test matching the classified MX result
- date create/get absolute time, equality, hash coherence
- C consumer smoke test including the new headers
- export snapshot check for the new public ABI
- dependency check proving no ICU, dispatch, Swift runtime, Objective-C, or
  system CoreFoundation linkage

Abort/failure tests to consider after MX classification:

- null object passed to scalar getters
- wrong CF type passed to scalar getters
- negative `CFDataCreate` length
- invalid `CFNumberType`
- null `valuePtr` passed to `CFNumberCreate` or `CFNumberGetValue`

## Implementation Start Gate

This gate is now satisfied. MX provided:

- `subset1_scalar_core.json`
- `summary.md`
- blocker/warning count from the Elixir manifest reporter
- explicit classification for `cfnumber_cross_type_equality`

Observed gate result:

- blockers: `0`
- warnings: `0`
- `cfnumber_cross_type_equality` observed equal for tested `42` integer/float
  forms on MX
- latest MX review reported FX-vs-MX scalar compare clean: `0` blockers,
  `0` warnings

The executed first FX implementation slice is:

1. add `CFEqual` and `CFHash` to the existing runtime base
2. add minimal scalar headers and export snapshot
3. implement `CFBoolean`
4. implement immutable `CFData`
5. implement tested `CFNumber`
6. implement stored-time `CFDate`
7. add the Subset 1A FX comparison artifact
8. send the FX JSON back through this repo for MX comparison

Current readiness note:

- the FX-vs-MX Subset 1A comparison wrapper and compare-artifact packaging are
  now wired in the repo
- the actual FX-emitted `subset1_scalar_core_fx.json` is now published in-repo
- the shared FX artifact compare against fresh MX evidence is clean

Stop and request review if MX shows that required 1A behavior depends on
`CFString`, ObjC bridging, Swift runtime dispatch, ICU, mutable containers, or
plist code. Those would contradict the current Subset 1A boundary.
