# CORT Subset 1B CFString Source Audit And FX Readiness

Date: 2026-04-16

Status: source audit complete; MX probe assets prepared; FX implementation has
not started.

This document is the extraction and implementation-readiness note for CORT
Subset 1B: minimal immutable `CFString` support for future bplist and
plist-valid key paths.

It does not replace the semantic contract in
`docs/cort-subset1b-cfstring-contract.md`. It answers a narrower question:
which upstream source can inform FX, which source must be excluded, and what
must be true before FX starts a direct string implementation.

## Source Basis

Working directory:

- `/Users/me/wip-launchx/wip-cort-gpt`

Audited source checkout:

- `../nx/swift-corelibs-foundation`
  - commit `f9c1d4eee71957c49eb66fd8fb5dac856d0efbf4`
  - `f9c1d4ee Cleanup swapped item after renameat2 call in FileManager (#5454)`

Primary files inspected:

- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFString.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFString.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFBinaryPList.c`

Existing FX files inspected for fit:

- `cort-fx/include/CoreFoundation/CFBase.h`
- `cort-fx/include/CoreFoundation/CFRuntime.h`
- `cort-fx/include/CoreFoundation/CoreFoundation.h`
- `cort-fx/src/FXCFInternal.h`
- `cort-fx/src/CFBase.c`
- `cort-fx/src/CFRuntime.c`
- `cort-fx/Makefile`

## Current Decision

Do not copy `CFString.c` or `CFString.h` directly into `cort-fx`.

Use them as algorithm and API-shape references only. The upstream string layer
is much broader than Subset 1B and carries dependencies that are incompatible
with the CORT platform model:

- `CFString.h` pulls broad CoreFoundation headers and bridge macros
- `CFString.c` owns normalization, localization, formatting, comparison,
  mutable strings, constant strings, and multiple storage strategies
- `CFString.c` depends on private runtime and Unicode support headers that are
  not acceptable as direct FX build dependencies
- `CFBinaryPList.c` demonstrates that the first consumer only needs a narrow
  subset of the public string surface

This is the same decision pattern used for Subset 0 and Subset 1A: preserve
the public semantic slice, but rewrite the implementation cleanly in pure C.

## Dependency Scan

Search terms used against the files above:

```text
_foundation_unicode uchar.h ucnv_ ubrk_ ucol_ unorm_ ucase_
#include <dispatch/ dispatch_once dispatch_apply dispatch_queue dispatch_async
DEPLOYMENT_RUNTIME_SWIFT swift_allocObject swift_retain swift_release _swift_rc CF_IS_SWIFT
CF_IS_OBJC objc_msgSend objc_retain NSObject NSCF CF_BRIDGED
CFLocale CFCharacterSet CFArray CFDictionary CFNumberFormatter
CFUnicodeDecomposition CFUnicodePrecomposition CFStringLocalizedFormattingInternal
CFStringCreateWithCString CFStringCreateWithBytes CFStringCreateWithCharacters
CFStringGetLength CFStringGetCharacters CFStringGetCString CFStringGetBytes
CFStringCompare CFStringFind CFStringFold CFSTR
```

High-level result:

- direct Unicode and ICU-adjacent dependencies appear in `CFString.c`
- the public upstream header pulls multiple non-Subset-1B CoreFoundation
  headers
- `CFString.c` mixes required creation/getter logic with excluded comparison,
  locale, formatting, mutable, and constant-string machinery
- `CFBinaryPList.c` confirms the first bplist consumer needs only a small
  string surface

## Dependency Audit Table

| Source file | Dependency or symbol | Why it appears | Subset 1B relevance | Classification | Proposed FX action | Blocker status |
| --- | --- | --- | --- | --- | --- | --- |
| `include/CFString.h` | `CFArray.h`, `CFData.h`, `CFDictionary.h`, `CFCharacterSet.h`, `CFLocale.h` | broad header graph for full CFString API | Too broad for 1B | `replace-with-local` | create a narrow FX `CFString.h` with only needed forward types, encodings, and declarations | Blocking for source copy, not blocking for clean implementation |
| `include/CFString.h` | `CFSTR`, constant-string macros, `DEPLOYMENT_RUNTIME_SWIFT` constant-string class wiring | compile-time constant string support | Excluded from 1B | `exclude` | do not implement `CFSTR` or constant strings in the first FX string slice | Blocking for source copy, not blocking for clean implementation |
| `include/CFString.h` | no-copy APIs, mutable APIs, comparison/search APIs | broad CFString public surface | Out of scope for 1B | `defer` | keep FX header limited to the validated immutable core | Not blocking |
| `CFString.c` | `CFStringEncodingConverterExt.h`, `CFStringEncodingConverterPriv.h`, `CFUniChar.h`, `CFUnicodeDecomposition.h`, `CFUnicodePrecomposition.h`, `_foundation_unicode/uchar.h` | Unicode conversion, normalization, and broad encoding support | Only narrow create/get paths are needed; normalization is excluded | `exclude` | implement direct ASCII/UTF-8 decode and UTF-8 encode for tested paths without importing these layers | Blocking for source copy, not blocking for clean implementation |
| `CFString.c` | `CFNumberFormatter.h`, `CFStringLocalizedFormattingInternal.h`, `CFError_Private.h` | formatting and localized output | Excluded from 1B | `exclude` | leave formatting/debug-description callbacks `NULL` until a later slice needs them | Blocking for source copy, not blocking for clean implementation |
| `CFString.c` | `CFInternal.h`, `CFString_Internal.h`, `CFRuntime_Internal.h` | private runtime flags and storage helpers | Concepts are useful, headers are too broad | `replace-with-local` | add only the minimal local helpers needed in `FXCFInternal.h` and the string TU | Blocking for source copy, not blocking for clean implementation |
| `CFString.c` | multiple storage variants, inline strings, no-copy buffers, mutable gaps | performance and full-feature storage model | Not required for first immutable slice | `defer` | use one simple immutable representation first; optimize later only if needed | Not blocking |
| `CFString.c` | `CFStringCreateWithCString`, `CFStringCreateWithBytes`, `CFStringCreateWithCharacters`, `CFStringGetLength`, `CFStringGetCharacters`, `CFStringGetCString`, `CFStringGetBytes` | public create/get semantics | Directly relevant | `required-for-subset` | adapt algorithms only for tested ASCII, UTF-8, UTF-16, and ASCII bytes fast-path behavior | Not blocking |
| `CFString.c` | `__CFStringHash`, `CFHash` helpers | string hash coherence | Directly relevant | `required-for-subset` | implement local code-unit-based hash with equal-string coherence; exact numerics remain diagnostic | Not blocking |
| `CFString.c` | `CFStringCompare`, `CFStringFind`, `CFStringFold`, locale paths | broad comparison and search semantics | Excluded from 1B | `defer` | do not implement until a later validated slice requires them | Not blocking |
| `CFBinaryPList.c` | `CFStringGetLength`, `CFStringGetBytes`, `CFStringGetCharacters` | bplist write path for ASCII and UTF-16 strings | Directly relevant consumer surface | `required-for-subset` | keep 1B aligned with these read/write needs | Not blocking |
| `CFBinaryPList.c` | `CFStringCreateWithBytes`, `CFStringCreateWithCharacters` | bplist read path for ASCII and UTF-16 strings | Directly relevant consumer surface | `required-for-subset` | validate and implement these exact creation paths first | Not blocking |
| `CFBinaryPList.c` | `CFStringGetCStringPtr`, `CFStringGetCString` short-key optimization | dictionary key fast path during plist lookups | Useful later, but pointer-return optimization is out of scope for 1B | `defer` | require `CFStringGetCString`; defer pointer-return optimization | Not blocking |
| `CFBinaryPList.c` | `CFStringCompare`, `CFStringGetIntValue` | filtered key-path parsing | Out of scope for first immutable string slice | `defer` | revisit when filtered key-path behavior is validated explicitly | Not blocking |

## Public ABI Readiness

Subset 1B will require a new public header after MX validation:

- `cort-fx/include/CoreFoundation/CFString.h`

Required initial declarations after MX validates the semantic slice:

- `CFStringEncoding`
- tested encoding constants:
  - `kCFStringEncodingASCII`
  - `kCFStringEncodingUTF8`
  - `kCFStringEncodingUnicode`
  - `kCFStringEncodingUTF16`
- `CFStringGetTypeID`
- `CFStringCreateWithCString`
- `CFStringCreateWithBytes`
- `CFStringCreateWithCharacters`
- `CFStringCreateCopy`
- `CFStringGetLength`
- `CFStringGetCharacters`
- `CFStringGetCString`
- `CFStringGetBytes`

Header plan:

- include only the validated immutable string surface
- strip bridge annotations and constant-string macros
- include the header from `CoreFoundation.h`
- keep the first FX string ABI smaller than the upstream header

Export plan:

- add a new pinned export snapshot when Subset 1B implementation begins
- keep `make check-exports` strict

## Internal Runtime Readiness

The current runtime already has the callback surface needed for strings:

- class `equal`
- class `hash`
- `finalize`

Required runtime additions once MX validates 1B:

- reserve a process-local `CFString` type ID
- register a `CFString` runtime class during initialization
- add a string finalizer that releases only FX-owned storage
- keep string equality and hash code-unit-based for the validated slice

## Recommended First FX Shape

Recommended initial immutable shape:

```c
struct __CFString {
    CFRuntimeBase _base;
    CFIndex length;
    uint32_t flags;
    UniChar contents[];
};
```

Implementation notes:

- store canonical UTF-16 code units in-object for the first slice
- use one immutable representation only
- use a small flag such as `FX_CFSTRING_IS_ASCII` when every code unit is less
  than `0x80`
- decode tested ASCII and UTF-8 creation paths into UTF-16 code units once
- `CFStringCreateWithCharacters` copies the caller buffer verbatim
- `CFStringGetLength` returns code-unit length
- `CFStringGetCharacters` copies the stored code units
- `CFStringGetCString` encodes UTF-8 on demand for the tested path
- `CFStringGetBytes` must support the full-range ASCII extraction used by later
  bplist writing
- no pointer-return optimization, no mutable gaps, no no-copy ownership, and no
  inline-vs-outline storage variants in the first slice

## FX Readiness Gate

FX Subset 1B implementation should not start until:

- MX artifacts exist for `subset1b_cfstring_core`
- malformed UTF-8 rejection is classified explicitly
- UTF-16 code-unit length semantics are recorded explicitly
- ASCII `CFStringGetBytes` full-range behavior is recorded explicitly
- the team accepts that the first implementation is a clean rewrite, not an
  extraction project
