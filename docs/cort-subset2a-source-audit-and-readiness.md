# CORT Subset 2A Container Source Audit And FX Readiness

Date: 2026-04-17

Status: source audit complete; MX probe assets prepared; FX implementation has
not started.

This document is the extraction and implementation-readiness note for CORT
Subset 2A: minimal `CFArray` and `CFDictionary` support for plist-valid object
graphs using `kCFType` callback modes only.

It does not replace the semantic contract in
`docs/cort-subset2a-container-core-contract.md`. It answers a narrower
question: which upstream source can inform FX, which source must be excluded,
and what must be true before FX starts a clean local container implementation.

## Source Basis

Working directory:

- `/Users/me/wip-launchx/wip-cort-gpt`

Audited source checkout:

- `../nx/swift-corelibs-foundation`
  - commit `f9c1d4eee71957c49eb66fd8fb5dac856d0efbf4`
  - `f9c1d4ee Cleanup swapped item after renameat2 call in FileManager (#5454)`

Primary files inspected:

- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFArray.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFDictionary.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFArray.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFDictionary.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/internalInclude/CFBasicHash.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFBasicHash.c`

Existing FX files inspected for fit:

- `cort-fx/include/CoreFoundation/CFBase.h`
- `cort-fx/include/CoreFoundation/CoreFoundation.h`
- `cort-fx/include/CoreFoundation/CFRuntime.h`
- `cort-fx/include/CoreFoundation/CFString.h`
- `cort-fx/src/FXCFInternal.h`
- `cort-fx/src/CFRuntime.c`
- `cort-fx/src/CFString.c`
- `cort-fx/Makefile`

## Current Decision

Do not copy `CFArray.c`, `CFDictionary.c`, `CFBasicHash.c`, or the upstream
container headers directly into `cort-fx`.

Use them as API-shape and algorithm references only. The upstream container
layer is much broader than Subset 2A and carries dependencies that are
incompatible with the CORT platform model:

- `CFArray.c` mixes the required `kCFType` callback behavior with custom
  callbacks, `NULL` callbacks, extra mutation APIs, and ObjC/Swift dispatch
- `CFDictionary.c` is a thin wrapper over `CFBasicHash`, so extraction really
  means extracting the hash engine too
- `CFBasicHash.c` carries `Block.h`, dispatch, DTrace, OSAtomic, and a large
  callback/flag matrix that Subset 2A does not need
- the first LaunchX consumer needs only a narrow container surface, not the
  full collection subsystem

This follows the same decision pattern used for Subset 0, Subset 1A, and
Subset 1B: preserve the required public semantic slice, but rewrite the
implementation cleanly in pure C.

## Dependency Scan

Search terms used against the files above:

```text
dispatch Block.h CFBasicHash CFStorage CFSet CFBag CFString
CFInternal CFPriv CFRuntime_Internal
CF_IS_OBJC CF_IS_SWIFT CF_OBJC_FUNCDISPATCHV CF_SWIFT_FUNCDISPATCHV
OSAtomic DTRACE TARGET_OS_MAC
kCFCopyStringDictionaryKeyCallBacks
CFArrayCreate CFArrayCreateCopy CFArrayCreateMutable CFArrayAppendValue
CFArrayRemoveValueAtIndex CFArrayGetValueAtIndex
CFDictionaryCreate CFDictionaryCreateCopy CFDictionaryCreateMutable
CFDictionarySetValue CFDictionaryRemoveValue CFDictionaryGetValue
CFDictionaryGetValueIfPresent
```

High-level result:

- `CFArray.h` and `CFDictionary.h` expose the full public callback surface, far
  beyond the first container slice
- `CFArray.c` has useful callback and storage concepts, but it is entangled
  with private runtime headers and ObjC/Swift dispatch
- `CFDictionary.c` is operationally coupled to `CFBasicHash`
- `CFBasicHash.c` is the hardest extraction problem in this slice because it
  pulls in `Block.h`, dispatch, OSAtomic, and a broad callback/flag matrix
- clean local container implementations will be simpler and safer than
  copy-and-trim extraction

## Dependency Audit Table

| Source file | Dependency or symbol | Why it appears | Subset 2A relevance | Classification | Proposed FX action | Blocker status |
| --- | --- | --- | --- | --- | --- | --- |
| `include/CFArray.h` | full callback struct surface | public API supports `NULL`, custom, and CFType callbacks | Only `kCFTypeArrayCallBacks` is in scope for 2A | `replace-with-local` | create a narrow FX `CFArray.h` exposing only the validated APIs and callback constant | Blocking for source copy, not blocking for clean implementation |
| `include/CFDictionary.h` | full callback struct surface and `kCFCopyStringDictionaryKeyCallBacks` | public API supports custom, `NULL`, copy-string, and CFType callbacks | Only `kCFType` key/value callbacks are in scope for 2A | `replace-with-local` | create a narrow FX `CFDictionary.h` exposing only the validated APIs and `kCFType` callback constants | Blocking for source copy, not blocking for clean implementation |
| `CFArray.c` | `CFPriv.h`, `CFInternal.h`, `CFRuntime_Internal.h` | private runtime flags, allocator helpers, descriptions | Concepts are useful; direct header dependency is too broad | `replace-with-local` | lift only minimal local helpers into FX container TUs and `FXCFInternal.h` | Blocking for source copy, not blocking for clean implementation |
| `CFArray.c` | ObjC/Swift dispatch macros | bridge and toll-free dispatch paths | Excluded from FX | `exclude` | remove all ObjC/Swift dispatch from the FX slice | Blocking for source copy, not blocking for clean implementation |
| `CFArray.c` | deque storage, extra mutation APIs | broad mutable array feature set | Only append/remove and borrowed get are needed | `defer` | start with one simple resizable vector representation | Not blocking |
| `CFArray.c` | callback matrix (`NULL`, CFType, custom) | full public callback compatibility | Only CFType mode is needed | `defer` | implement a small local callback-mode enum for CFType-only in 2A | Not blocking |
| `CFDictionary.c` | `CFBasicHash.h` | dictionary implementation is delegated to hash engine | Directly relevant, but too broad to import unchanged | `replace-with-local` | implement a small local open-addressed dictionary or equivalent | Blocking for source copy, not blocking for clean implementation |
| `CFDictionary.c` | `CFString.h` | copy-string key callbacks and descriptions | string keys are relevant; copy-string callback mode is not | `defer` | depend only on validated `CFString` hash/equality/getter behavior in FX | Not blocking |
| `CFDictionary.c` | ObjC/Swift dispatch macros | bridge and toll-free dispatch paths | Excluded from FX | `exclude` | remove all ObjC/Swift dispatch from the FX slice | Blocking for source copy, not blocking for clean implementation |
| `CFDictionary.c` | `kCFCopyStringDictionaryKeyCallBacks` | mutable-string-key copy mode | Excluded from 2A | `exclude` | do not implement in the first container slice | Not blocking |
| `internalInclude/CFBasicHash.h` | `CFInternal.h` and `CFString.h` | internal callback and description machinery | Too broad for direct use | `replace-with-local` | define a much smaller local internal dictionary interface | Blocking for source copy, not blocking for clean implementation |
| `CFBasicHash.c` | `Block.h` | block-based apply and debug description paths | Excluded from FX 2A | `exclude` | do not use block-based traversal in the first FX dictionary implementation | Blocking for source copy |
| `CFBasicHash.c` | `dispatch/dispatch.h` | hash table internals and diagnostics | Excluded from FX | `exclude` | no dispatch dependency in the first FX dictionary implementation | Blocking for source copy |
| `CFBasicHash.c` | `OSAtomic*` and DTrace machinery | instrumentation and mutation accounting | Excluded from FX | `replace-with-c11` | use C11 atomics only where genuinely needed; otherwise keep container mutation single-threaded by contract | Blocking for source copy, not blocking for clean implementation |
| `CFBasicHash.c` | large callback/flag matrix | supports sets, bags, multiset counts, indirect keys, weak/strong modes | Far beyond 2A | `defer` | build a much smaller dictionary engine for keyed strong CFType entries only | Blocking for source copy, not blocking for clean implementation |
| `CFArray.c` and `CFDictionary.c` | `CFArrayCreate`, `CFArrayCreateCopy`, `CFArrayCreateMutable`, `CFArrayGetValueAtIndex`, `CFArrayAppendValue`, `CFArrayRemoveValueAtIndex`, `CFDictionaryCreate`, `CFDictionaryCreateCopy`, `CFDictionaryCreateMutable`, `CFDictionaryGetValue`, `CFDictionaryGetValueIfPresent`, `CFDictionarySetValue`, `CFDictionaryRemoveValue` | required public semantics | Directly relevant | `required-for-subset` | adapt only the minimal semantics required by the 2A MX contract | Not blocking |

## Public ABI Readiness

Subset 2A will require new public headers after MX validation:

- `cort-fx/include/CoreFoundation/CFArray.h`
- `cort-fx/include/CoreFoundation/CFDictionary.h`

Required initial declarations after MX validates the semantic slice:

- `CFArrayCallBacks`
- `kCFTypeArrayCallBacks`
- `CFArrayRef`
- `CFMutableArrayRef`
- `CFArrayGetTypeID`
- `CFArrayCreate`
- `CFArrayCreateCopy`
- `CFArrayCreateMutable`
- `CFArrayGetCount`
- `CFArrayGetValueAtIndex`
- `CFArrayAppendValue`
- `CFArrayRemoveValueAtIndex`
- `CFDictionaryKeyCallBacks`
- `CFDictionaryValueCallBacks`
- `kCFTypeDictionaryKeyCallBacks`
- `kCFTypeDictionaryValueCallBacks`
- `CFDictionaryRef`
- `CFMutableDictionaryRef`
- `CFDictionaryGetTypeID`
- `CFDictionaryCreate`
- `CFDictionaryCreateCopy`
- `CFDictionaryCreateMutable`
- `CFDictionaryGetCount`
- `CFDictionaryGetValue`
- `CFDictionaryGetValueIfPresent`
- `CFDictionarySetValue`
- `CFDictionaryRemoveValue`

Header plan:

- include only the validated CFType-callback container core
- do not expose copy-string or custom callback modes yet
- include the new headers from `CoreFoundation.h`
- keep the first FX container ABI smaller than the upstream headers

Export plan:

- add a new pinned export snapshot when FX Subset 2A implementation begins
- keep `make check-exports` strict

## Internal Runtime Readiness

The current runtime already has the callback surface needed for containers:

- class `finalize`
- class `equal`
- class `hash`

Required runtime additions once MX validates 2A:

- reserve process-local `CFArray` and `CFDictionary` type IDs
- register runtime classes during initialization
- add finalizers that release contained children according to the validated
  callback mode
- keep the first mutable-container thread-safety contract explicit:
  retain/release thread-safe, mutation externally synchronized

## Recommended First FX Shape

Recommended initial immutable/mutable array shape:

```c
struct __CFArray {
    CFRuntimeBase _base;
    CFIndex count;
    CFIndex capacity;
    uint32_t flags;
    const void **values;
};
```

Recommended initial dictionary shape:

```c
struct __CFDictionaryEntry {
    const void *key;
    const void *value;
    uint32_t state;
};

struct __CFDictionary {
    CFRuntimeBase _base;
    CFIndex count;
    CFIndex capacity;
    uint32_t flags;
    struct __CFDictionaryEntry *entries;
};
```

Implementation notes:

- start with one simple resizable contiguous array representation
- start with one simple open-addressed dictionary representation
- implement only the validated CFType callback mode in 2A
- reuse existing `CFEqual` and `CFHash` for validated string/scalar keys and
  values
- `Get` APIs must return borrowed stored pointers without retaining
- mutable container operations should be explicit and narrow:
  append/remove for arrays, set/remove for dictionaries
- do not optimize for pointer-return fast paths, copy-string keys, or broad
  callback compatibility yet

## FX Readiness Gate

FX Subset 2A implementation should not start until:

- MX artifacts exist for `subset2a_container_core`
- borrowed Get semantics are classified explicitly
- replacement-release semantics for dictionary values are classified explicitly
- empty container behavior is classified explicitly
- string-key lookup behavior is classified explicitly
- the team accepts that the first implementation is a clean rewrite, not a
  `CFBasicHash` extraction project
