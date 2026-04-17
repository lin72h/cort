# CORT Subset 3A Binary Plist Source Audit And FX Readiness

Date: 2026-04-17

Status: source audit complete; MX probe assets prepared; FX implementation has
not started.

This document is the extraction and implementation-readiness note for CORT
Subset 3A: bounded binary-plist read/write for immutable property-list-valid
graphs.

It does not replace the semantic contract in
`docs/cort-subset3a-bplist-core-contract.md`. It answers a narrower question:
which upstream source can inform FX, which source must be excluded, and what
must be true before FX starts a clean local binary-plist implementation.

Related execution documents:

- `docs/cort-subset3a-validation-workflow.md`
- `docs/cort-subset3a-fx-implementation-plan.md`
- `docs/cort-subset3a-fixture-corpus.md`

## Source Basis

Working directory:

- `/Users/me/wip-launchx/wip-cort-gpt`

Audited source checkout:

- `../nx/swift-corelibs-foundation`
  - commit `f9c1d4eee71957c49eb66fd8fb5dac856d0efbf4`
  - `f9c1d4ee Cleanup swapped item after renameat2 call in FileManager (#5454)`

Primary files inspected:

- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFPropertyList.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFPropertyList.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFBinaryPList.c`

Existing FX files inspected for fit:

- `cort-fx/include/CoreFoundation/CFBase.h`
- `cort-fx/include/CoreFoundation/CoreFoundation.h`
- `cort-fx/include/CoreFoundation/CFArray.h`
- `cort-fx/include/CoreFoundation/CFData.h`
- `cort-fx/include/CoreFoundation/CFDate.h`
- `cort-fx/include/CoreFoundation/CFDictionary.h`
- `cort-fx/include/CoreFoundation/CFNumber.h`
- `cort-fx/include/CoreFoundation/CFString.h`
- `cort-fx/src/CFArray.c`
- `cort-fx/src/CFData.c`
- `cort-fx/src/CFDate.c`
- `cort-fx/src/CFDictionary.c`
- `cort-fx/src/CFNumber.c`
- `cort-fx/src/CFString.c`
- `cort-fx/src/CFRuntime.c`
- `cort-fx/Makefile`

## Current Decision

Do not copy `CFBinaryPList.c`, `CFPropertyList.c`, or `CFPropertyList.h`
directly into `cort-fx`.

Use them as API-shape and algorithm references only. The upstream binary-plist
layer is much broader than Subset 3A and carries dependencies that are
incompatible with the CORT platform model:

- `CFPropertyList.c` mixes binary plist with XML, OpenStep, stream APIs,
  top-level-key helpers, deep-copy, and broader validation machinery
- `CFBinaryPList.c` mixes the required tag read/write paths with set, UID,
  filtered-keypath parsing, mutable parse modes, and stream/private helper
  scaffolding
- both files depend on private headers and helper APIs that do not match the
  smaller validated FX surface
- the first LaunchX consumer needs only the in-memory binary read/write path,
  not the full property-list subsystem

This follows the same decision pattern used for Subset 0, Subset 1A, Subset
1B, and Subset 2A: preserve the required public semantic slice, but rewrite the
implementation cleanly in pure C.

## Dependency Scan

Search terms used against the files above:

```text
CFBinaryPList CFPropertyListCreateData CFPropertyListCreateWithData
CFPropertyListWrite CFPropertyListCreateWithStream CFError CFStream
CFSet CFUUID CFNull CFKeyedArchiverUID os_log_fault
CFDictionaryGetKeysAndValues CFArrayGetValues CFDictionaryAddValue
CFArrayReplaceValues CFDataCreateMutable CFStringCreateMutableCopy
kCFPropertyListMutableContainers kCFPropertyListMutableContainersAndLeaves
kCFBinaryPlistMarkerUID kCFBinaryPlistMarkerSet kCFBinaryPlistMarkerOrdset
CFPropertyListIsValid _CFPropertyListCreateFiltered __CFBinaryPlistCopyTopLevelKeys
```

High-level result:

- `CFPropertyList.h` exposes the full property-list API, much broader than the
  first binary-plist slice
- `CFPropertyList.c` is the public wrapper layer, but it is tightly coupled to
  XML, streams, and broad validation/error paths
- `CFBinaryPList.c` contains the right algorithmic core, but it is entangled
  with private runtime helpers, mutable parse paths, and unsupported object
  tags
- clean local binary-plist TUs will be simpler and safer than copy-and-trim
  extraction

## Dependency Audit Table

| Source file | Dependency or symbol | Why it appears | Subset 3A relevance | Classification | Proposed FX action | Blocker status |
| --- | --- | --- | --- | --- | --- | --- |
| `include/CFPropertyList.h` | full format and stream API surface | public API includes XML, stream, and deprecated entry points | Only binary in-memory create/read is needed in 3A | `replace-with-local` | create a narrow FX `CFPropertyList.h` with only the validated enums and functions | Blocking for source copy, not blocking for clean implementation |
| `CFPropertyList.c` | XML and OpenStep parser/writer paths | complete property-list subsystem | Out of scope for 3A | `exclude` | keep the first FX property-list layer binary-only | Blocking for source copy, not blocking for clean implementation |
| `CFPropertyList.c` | `CFWriteStream`, `CFReadStream` | stream APIs and stream-backed error handling | Out of scope for 3A | `exclude` | implement data-based APIs first; defer stream APIs | Blocking for source copy, not blocking for clean implementation |
| `CFPropertyList.c` | `_CFPropertyListIsValidWithErrorString` | broad public validation wrapper | Useful concept, too broad to import | `replace-with-local` | build a small local graph validator for the supported binary slice only | Not blocking |
| `CFPropertyList.c` | `CFError` wrapping and debug descriptions | public error reporting | Semantically relevant, wording is not | `defer` | plan a minimal FX error surface or equivalent internal error code mapping for 3A | Not blocking for readiness, but a real implementation gate |
| `CFBinaryPList.c` | `CFStream.h`, external-buffer allocators | stream/external-buffer write helpers | Out of scope for 3A | `exclude` | write/read `CFData` first | Blocking for source copy, not blocking for clean implementation |
| `CFBinaryPList.c` | `CFSet`, `CFUUID`, keyed-archiver UID | extra object tags beyond the first packet slice | Out of scope for 3A | `exclude` | do not implement those tags in the first FX binary-plist slice | Not blocking |
| `CFBinaryPList.c` | filtered-keypath helpers and top-level-key extraction | optimized plist navigation | Out of scope for 3A | `defer` | revisit only if LaunchX needs filtered or partial decode later | Not blocking |
| `CFBinaryPList.c` | mutable parse paths (`CFDataCreateMutable`, `CFStringCreateMutableCopy`, mutable containers) | mutable parse options | Out of scope for 3A | `exclude` | support immutable mode only in the first FX binary-plist slice | Not blocking |
| `CFBinaryPList.c` | `CFDictionaryGetKeysAndValues`, `CFArrayGetValues`, `CFDictionaryAddValue`, `CFArrayReplaceValues` | broad container helpers used by upstream serializer/parser | Current FX 2A surface is narrower | `replace-with-local` | write local iteration/assembly code against the validated 2A surface or widen containers deliberately later | Not blocking for readiness, but important implementation note |
| `CFBinaryPList.c` | `os_log_fault`, private internal headers, transfer helpers | diagnostics, private recursion helpers, transfer constructors | Too broad for direct import | `replace-with-local` | keep only the needed safe bounds checks and recursion accounting | Blocking for source copy, not blocking for clean implementation |
| `CFBinaryPList.c` | `bplist00` header/trailer parse, integer/ref sizing, safe bounds checks | core binary format semantics | Directly relevant | `required-for-subset` | adapt these algorithms carefully in clean local code | Not blocking |
| `CFBinaryPList.c` | `_appendString`, `_appendNumber`, `_appendObject` | write-path tag selection and encoding behavior | Directly relevant | `required-for-subset` | adapt the tested ASCII/UTF-16 string, number, date, data, array, and dictionary tag selection logic | Not blocking |
| `CFBinaryPList.c` | `__CFBinaryPlistCreateObjectFiltered` | read-path object materialization and safe rejection | Directly relevant | `required-for-subset` | adapt the safe read logic only for the supported tags and immutable mode | Not blocking |

## Public ABI Readiness

Subset 3A will require a new public header after MX validation:

- `cort-fx/include/CoreFoundation/CFPropertyList.h`

Required initial declarations after MX validates the semantic slice:

- `CFPropertyListFormat`
- `kCFPropertyListBinaryFormat_v1_0`
- `kCFPropertyListImmutable`
- `kCFPropertyListReadCorruptError`
- `kCFPropertyListWriteStreamError`
- `CFPropertyListCreateData`
- `CFPropertyListCreateWithData`

Header plan:

- include only the validated binary in-memory surface
- exclude XML/OpenStep/stream/deprecated entry points
- include the header from `CoreFoundation.h`
- keep the first FX property-list ABI smaller than the upstream header

Export plan:

- add a new pinned export snapshot when FX Subset 3A implementation begins
- keep `make check-exports` strict

## Internal Runtime Readiness

The current runtime and type surface already provide the core object set needed
for 3A:

- `CFBoolean`
- `CFNumber`
- `CFDate`
- `CFData`
- `CFString`
- `CFArray`
- `CFDictionary`

Readiness gaps that 3A will have to resolve explicitly:

- narrow `CFPropertyList` public enums and function declarations
- a minimal `CFError` strategy or equivalent internal error-to-public-API
  mapping
- binary serializer traversal using the validated 2A container APIs
- binary parser bounds checking and recursion limits

## Recommended First FX Shape

Recommended first write/read shape:

```text
CFPropertyListCreateData
  -> validate supported immutable graph
  -> flatten supported objects to a small object table
  -> emit bplist00 header
  -> emit object table for supported tags only
  -> emit offset table and trailer

CFPropertyListCreateWithData
  -> validate header and trailer
  -> parse supported tags only
  -> materialize immutable FX objects
  -> reject malformed input safely
```

Implementation notes:

- start with `CFData`-backed in-memory read/write only
- support immutable mode only
- support only the validated tag set:
  - bool
  - int
  - real
  - date
  - data
  - ASCII string
  - UTF-16 string
  - array
  - dict with string keys
- reject unsupported tags safely
- keep offset-table and recursion validation explicit and simple
- do not optimize for presizing, stream buffering, or filtered lookup in the
  first slice

## FX Readiness Gate

FX Subset 3A implementation should not start until:

- MX artifacts exist for `subset3a_bplist_core`
- binary-format out-parameter behavior is classified explicitly
- non-string dictionary-key write rejection is classified explicitly
- invalid header and truncated-input rejection is classified explicitly
- the team accepts that the first implementation is a clean rewrite, not a
  `CFBinaryPList.c` extraction project

Current readiness note:

- this package makes Subset 3A ready for bounded MX validation
- the concrete FX cut plan now lives in
  `docs/cort-subset3a-fx-implementation-plan.md`
- the planned MX/FX fixture contract now lives in
  `docs/cort-subset3a-fixture-corpus.md`
- the real FX implementation should still wait for the current MX Subset 3A
  artifacts before starting code changes
