# CORT Subset 0 Contract And Dependency Audit

Status: local FX runtime proof implemented and verified; MX validation still pending.

Date: 2026-04-16

This document is the first CORT execution artifact for the dedicated
`cort-fx` lane. It defines Subset 0, records the Phase 1.5 dependency audit,
and now also records a temporary local FX proof implementation in this repo.
MX observations are still required before treating the implementation as a
semantically validated reference.

## Source Basis

Working directory:

- `/Users/me/wip-launchx/wip-cort-gpt`

Reference documents read from the parent agent workspace:

- `../wip-swift-gpt54x/AGENTS.md`
- `../wip-swift-gpt54x/docs/cort-agent-handoff.md`
- `../wip-swift-gpt54x/docs/swift-cort-corefoundation-architecture-report.md`
- `../wip-swift-gpt54x/docs/cort-mx-validation-plan.md`
- `../wip-swift-gpt54x/docs/mx-cort-coordination-note.md`
- `../wip-swift-gpt54x/docs/swift-cort-c.md`
- `../wip-swift-gpt54x/docs/swift-cort-zig.md`
- `../wip-swift-gpt54x/docs/nx-terminology.md`
- `../wip-swift-gpt54x/docs/swift-seqpacket-uds-decision-report.md`
- `../wip-swift-gpt54x/docs/swift-fds-vs-libfabric-shm.md`

Audited source checkouts:

- `../nx/swift-corelibs-foundation`
  - commit `f9c1d4eee71957c49eb66fd8fb5dac856d0efbf4`
  - `f9c1d4ee Cleanup swapped item after renameat2 call in FileManager (#5454)`
- `../nx/swift-corelibs-blocksruntime`
  - commit `63319b62157ceb7642d91d42eac22b99555568f8`
  - `63319b6 Merge pull request #6 from etcwilde/ewilde/blocks-install`

Audit host:

- `FreeBSD bdw-fx15-x64z 15.0-RELEASE ... amd64`
- `FreeBSD clang version 19.1.7`

Primary source files inspected:

- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFBase.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFRuntime.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFBase.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CFRuntime.c`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/CMakeLists.txt`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/internalInclude/CFInternal.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/internalInclude/CFRuntime_Internal.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/internalInclude/CoreFoundation_Prefix.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/CFTargetConditionals.h`
- `../nx/swift-corelibs-foundation/Sources/CoreFoundation/include/ForFoundationOnly.h`
- `../nx/swift-corelibs-blocksruntime/src/BlocksRuntime/Block_private.h`

`CFBase.c` is included even though the handoff source list only named
`CFBase.h`, because Subset 0 includes allocator bootstrap and the allocator
implementation lives in `CFBase.c`.

## Subset 0 Definition

Subset 0 is the runtime and ownership proof.

It exists to prove the C object model before strings, containers, plist,
bplist, Blocks, Zig wrappers, or LaunchX packet migration.

Included FX behavior:

- runtime class registration
- runtime class lookup
- runtime instance allocation
- runtime instance initialization
- type ID lookup
- atomic retain
- atomic release
- finalizer callback on last release
- allocator bootstrap enough for runtime objects
- default system allocator, malloc allocator, and null allocator basics
- no-op diagnostic retain-count support if useful for tests

Excluded from Subset 0:

- `CFString`
- `CFData`
- `CFNumber`
- `CFBoolean`
- `CFDate`
- `CFArray`
- `CFDictionary`
- `CFError`
- `CFPropertyList`
- `CFBinaryPList`
- XML plist
- BlocksRuntime integration
- Zig wrappers
- Swift bridge
- FDS packet integration
- Objective-C bridge behavior
- Swift runtime object allocation
- private macOS `CFRuntime` probing

Subset 0 may mention container callback observations in the MX request only as
non-gating ownership evidence for future Subset 2. It does not implement
containers on FX, and container observations must not become a Subset 0
implementation or exit gate.

## Semantic Contract

### Runtime Class Registration

FX must provide `_CFRuntimeRegisterClass(const CFRuntimeClass *cls)`.

Required behavior:

- `cls` must remain owned by the caller for the process lifetime or until
  explicit unregistration is allowed by a later subset.
- `cls->className` must be a non-null, nul-terminated ASCII string.
- class version `0` is the only required initial version.
- `_kCFRuntimeCustomRefCount` is excluded from the first implementation unless
  a later subset proves a real need.
- successful registration returns a stable, process-local `CFTypeID`.
- failure returns `_kCFRuntimeNotATypeID`.
- registration is serialized internally.

Acceptable FX variance:

- numeric type IDs do not need to match native macOS.
- type IDs only need to be stable within one process.
- the first FX implementation may reject unregistration entirely.

Rejected behavior:

- no Objective-C class bridging through registration.
- no Swift class or metadata registration.
- no private macOS `CFRuntime` API dependency.

### Class Table

FX may preserve the CoreFoundation idea of a class table, but the table must be
minimal for Subset 0.

Required initial classes:

- `_kCFRuntimeIDNotAType`
- `_kCFRuntimeIDCFType`
- `_kCFRuntimeIDCFAllocator`
- dynamically registered test/runtime classes

The upstream `CFRuntime.c` hard-coded table references many non-Subset-0
classes. The first FX implementation must replace that with a minimal table
instead of stubbing unrelated classes.

Publication and concurrency rule:

- after registration publishes a class, concurrent `CFGetTypeID`,
  `CFRetain`, `CFRelease`, and class lookup must see a valid immutable class
  entry.
- if the class table can grow, growth must not invalidate readers or expose a
  partially initialized table entry.
- a fixed-capacity class table protected by a registration mutex is acceptable
  for Subset 0.
- unregistration remains excluded until a later subset defines reader safety.

### Runtime Base Layout

FX uses the pure-C layout:

```c
typedef struct __CFRuntimeBase {
    uintptr_t _cfisa;
    _Atomic(uint64_t) _cfinfoa;
} CFRuntimeBase;
```

Required FX interpretation:

- `_cfisa` is a `const CFRuntimeClass *` stored as `uintptr_t`.
- `_cfinfoa` stores type ID, refcount, deallocating/deallocated flags, allocator
  flag, and type-specific bits.
- no `_swift_rc`
- no ObjC `isa` compatibility
- no pointer-authenticated ObjC class table requirement

Acceptable FX variance:

- exact bit positions may follow upstream where useful, but the public contract
  is semantic: type ID lookup, retain/release, and finalization must work.
- FX may simplify the Subset 0 `_cfinfoa` layout by omitting deferred features
  such as custom refcount bits, zombie bits, 32-bit external retain tables, and
  broad constant-object machinery, provided the retained layout still leaves a
  documented path for later plist-valid object types.
- general constant object encoding can be deferred. Static allocator
  singletons are the exception and require explicit immortal-object handling in
  Subset 0.

### Allocation

FX must provide `_CFRuntimeCreateInstance`.

Required behavior:

- unknown type ID returns `NULL`.
- `kCFAllocatorNull` returns `NULL`.
- normal objects start with retain count `1`.
- object memory is zeroed after the runtime header.
- requested `extraBytes` are included after `CFRuntimeBase`.
- final deallocation is only through `CFRelease`.
- class `init` callback, if present, runs after the header is valid enough for
  `CFGetTypeID`, `CFRetain`, `CFRelease`, and `CFGetAllocator`.
- class `init` must see a valid object type ID.

Required allocator behavior:

- `NULL` allocator means the current default allocator.
- the initial current default allocator may be process-global for the proof.
- thread-local default allocator support may be deferred if documented.
- custom allocator ownership must not be exposed until tested.

Acceptable FX variance:

- first proof can support only system/malloc/null allocators plus enough custom
  allocator shape to allocate runtime objects in tests.
- exact memory padding can differ as long as alignment is at least pointer
  alignment and the object header is correct.

### Retain

FX must provide `CFRetain`.

Required behavior:

- `CFRetain(object)` returns the same pointer value.
- retain increments the object's retain count atomically unless the object is
  constant/immortal.
- retain on an object being deallocated must not resurrect it in the normal
  public path.
- retain is always thread-safe.

Diagnostic behavior:

- `CFGetRetainCount` may exist for tests, but tests must not rely on native
  macOS retain-count numerics.

Error behavior:

- `CFRetain(NULL)` is not a valid CORT operation. The first FX implementation
  may trap, assert, or fail fast. It must not silently succeed.

### Release And Finalization

FX must provide `CFRelease`.

Required behavior:

- release decrements the object's retain count atomically.
- when the retain count reaches zero, the class finalizer is invoked exactly
  once.
- the finalizer must not free the object memory; runtime frees memory after the
  finalizer returns.
- over-release must fail fast or be detected in a debug/checked build.
- release is always thread-safe.

Acceptable FX variance:

- resurrection during finalization is not a required Subset 0 behavior.
- custom refcount callbacks are deferred.
- zombie instrumentation is rejected for the first proof.

### Type ID Lookup

FX must provide `CFGetTypeID`.

Required behavior:

- returns the type ID embedded in the object's runtime header.
- does not consult Objective-C, Swift, or Foundation.
- rejects invalid non-CORT pointers through fail-fast/debug validation where
  practical.

Acceptable FX variance:

- `CFCopyTypeIDDescription` is deferred until minimal `CFString` exists.
- a private `_FXCFRuntimeGetClassName(typeID)` returning `const char *` may be
  used in tests before public `CFString` exists.

### Allocator Bootstrap

Allocator bootstrap is part of Subset 0 because runtime allocation requires an
allocator before ordinary object creation is fully available.

Required behavior:

- `kCFAllocatorDefault == NULL`
- `kCFAllocatorSystemDefault` exists before `_CFRuntimeCreateInstance` is used.
- `kCFAllocatorMalloc` and `kCFAllocatorNull` exist as static allocator objects
  or equivalent bootstrap objects.
- `CFAllocatorAllocate`, `CFAllocatorReallocate`, and
  `CFAllocatorDeallocate` work for the system allocator.
- `CFAllocatorGetDefault` returns a valid allocator.
- `CFAllocatorSetDefault` may be deferred or constrained to process-global
  behavior in the proof.

Static allocator singleton lifetime:

- `kCFAllocatorSystemDefault`, `kCFAllocatorMalloc`, and `kCFAllocatorNull`
  must be immortal/static objects.
- `CFRetain` on each static allocator singleton returns the same pointer and
  does not make the singleton eligible for deallocation.
- `CFRelease` on each static allocator singleton is a no-op.
- static allocator singleton finalizers must not run.
- static allocator singleton storage is never freed.
- tests must prove these rules separately from normal heap-object
  retain/release behavior.

Allocator bootstrap sequence:

1. Define static `CFRuntimeClass` structs for the allocator and sentinel runtime
   classes at compile time.
2. Initialize static allocator objects with `_cfisa` pointing directly to the
   compile-time allocator class struct.
3. Initialize each static allocator object's `_cfinfoa` with the allocator type
   ID and an immortal retain-count encoding.
4. During runtime initialization, publish those same compile-time class structs
   into the minimal class table.
5. Only after the minimal class table is published may
   `_CFRuntimeCreateInstance` allocate ordinary heap objects.

This resolves the upstream `STATIC_CLASS_REF` problem without depending on
Swift, Objective-C, or allocation-before-registration. Upstream already uses a
compile-time `CFRuntimeClass` for allocators in `CFBase.c` via
`__CFAllocatorClass`; FX should keep that pattern but make `_cfisa` point to
the class struct directly rather than to a Swift/ObjC class reference or
`NULL`.

Implementation note:

- upstream `CFBase.c` builds static allocator objects directly and does not use
  `_CFRuntimeCreateInstance` for `CFAllocatorCreate`. That shape is acceptable,
  but FX must update `_cfisa` semantics to point to the runtime class instead
  of a Swift/ObjC class reference.

### Thread Safety

Required:

- `CFRetain` and `CFRelease` are always thread-safe.
- immutable objects are thread-safe for read, retain, and release.
- runtime class registration is internally serialized.
- unregistering a class while instances exist is excluded.

Not required:

- concurrent mutation of mutable objects, because mutable objects are excluded
  from Subset 0.
- thread-local default allocator semantics in the first proof.

### Error And Diagnostic Policy

Subset 0 must not pull in `CFString` just to report diagnostics.

Required:

- internal runtime failures are classified as `null`, `unknown-type`,
  `allocation-failed`, `over-release`, `bad-class`, or `unsupported`.
- tests can observe structured failure through process exit or explicit
  internal test hooks.

Deferred:

- public `CFErrorRef`
- public `CFCopyDescription`
- public `CFCopyTypeIDDescription`
- localized macOS-like error text

## Dependency Audit Commands

The audit used the handoff patterns against the Subset 0 file set plus
`CFBase.c`:

```zsh
rg -n '_foundation_unicode|uchar\.h|ucnv_|ubrk_|ucol_|unorm_|ucase_' <files>
rg -n '#include <dispatch/|dispatch_once|dispatch_apply|dispatch_queue|dispatch_async|libdispatch_init|dispatch_release' <files>
rg -n 'DEPLOYMENT_RUNTIME_SWIFT|swift_allocObject|swift_retain|swift_release|_swift_rc|CF_IS_SWIFT' <files>
rg -n 'CF_IS_OBJC|objc_msgSend|objc_retain|NSObject|NSCF|CF_BRIDGED|OBJC_HAVE_TAGGED|TARGET_OS_MAC|TARGET_OS_IPHONE|TARGET_OS_OSX|TARGET_OS_WIN32|__APPLE__|mach|MacTypes|ptrauth|objc_' <files>
rg -n '^#\s*include' <files>
```

Audit result summary:

- ICU pattern: no matches in the Subset 0 file set.
- Dispatch: present through upstream CMake, private prefix headers, and
  `CFRuntime.c` initialization.
- Swift runtime: present and enabled by upstream private prefix defaults.
- Objective-C/Apple-only coupling: present mostly as public annotations,
  naming, ptrauth support, and compiled-out bridge paths.
- Transitive include hazards: significant; `CFInternal.h` pulls headers far
  beyond Subset 0.

## Source Dependency Table

| Source file | Dependency or symbol | Why it appears | Subset relevance | Classification | Proposed action | Blocker status |
| --- | --- | --- | --- | --- | --- | --- |
| `CMakeLists.txt` | `_FoundationICU` link | Whole upstream CoreFoundation target links ICU | Not needed for runtime/allocator | `exclude` | Do not use upstream target for Subset 0 | Blocking only if upstream target is reused |
| `CMakeLists.txt` | `dispatch` link | Whole upstream CoreFoundation target links dispatch | Not needed for Subset 0 | `exclude` | Standalone FX build with libc/pthread only | Blocking only if upstream target is reused |
| `CMakeLists.txt` | broad source list | Target builds strings, bundles, run loops, streams, URL, XML, etc. | Outside Subset 0 | `exclude` | Build a small `cort-fx` target | Blocking only if upstream target is reused |
| `CoreFoundation_Prefix.h:10-12` | `DEPLOYMENT_RUNTIME_SWIFT` defaults to `1` | Upstream is built for Swift Foundation by default | Directly conflicts with pure-C runtime | `blocking` | Force `DEPLOYMENT_RUNTIME_SWIFT=0` or replace prefix | Blocking until fixed |
| `CoreFoundation_Prefix.h:20-24` | `__HAS_DISPATCH__` defaults to `1` except WASI | Upstream assumes libdispatch is available | Not needed for runtime proof | `replace-with-pthread-or-c11` | Define `__HAS_DISPATCH__=0` or remove dispatch helpers | Blocking until fixed |
| `CoreFoundation_Prefix.h:145-158` | dispatch queue helper macro | Shared helper for broader CF | Not needed | `exclude` | Omit from Subset 0 internal header | Not blocking if excluded |
| `CFInternal.h:92-102` | includes `CFURL.h`, `CFString.h`, `CFDate.h`, `CFArray.h`, `Block.h` | Monolithic internal header | Pulls non-Subset-0 APIs | `blocking` | Create a minimal Subset 0 internal header | Blocking if included unchanged |
| `CFInternal.h:368-395` | TSD allocator lookup via `_CFGetTSD` | Thread-local default allocator | Useful later, not first proof | `defer` | Start with process-global allocator or small pthread TSD shim | Not blocking if deferred |
| `CFInternal.h:574-724` | pthread/os-unfair lock abstraction | Lock abstraction for class table and runtime | Relevant | `replace-with-pthread-or-c11` | Use pthread mutex directly or keep minimal lock shim | Not blocking |
| `CFInternal.h:726-730` | block-typed `_CF_dispatch_once` fallback | dispatch-once replacement uses block syntax | Not needed for Subset 0 | `exclude` | Use `pthread_once` or simple init guard | Blocking if compiled without Blocks |
| `CFInternal.h:1152-1203` | dispatch headers and global queues | Broader CF async helpers | Not needed | `exclude` | Remove from Subset 0 internal header | Blocking if included unchanged |
| `CFInternal.h:894-958` | ptrauth ObjC class table helpers, `CF_IS_OBJC` | Bridge and class-table support | Conflicts with `_cfisa` as `CFRuntimeClass *` | `exclude` | Replace with direct class pointer storage | Blocking if kept as design |
| `CFRuntime_Internal.h:14-108` | fixed type ID enum for full CF | Whole CoreFoundation class universe | Only allocator/type sentinels needed | `defer` | Keep minimal constants for Subset 0; reserve plist IDs later | Not blocking |
| `CFRuntime_Internal.h:130-190` | extern class declarations for all CF classes | Supports hard-coded upstream class table | Pulls non-Subset-0 symbols | `blocking` | Replace with minimal declarations | Blocking if used unchanged |
| `CFBase.h:223-230` | `CF_BRIDGED_TYPE`, `CF_RELATED_TYPE` | ObjC bridge annotations | Public header should be clean for FX/Zig | `exclude` | Define empty or remove from FX public headers | Not blocking if neutralized |
| `CFBase.h:453-519` | bridged opaque typedefs | CF-compatible public names with ObjC attributes | Public shape relevant, attributes not | `exclude` | Keep opaque names, drop bridge attributes | Not blocking |
| `CFBase.h:684-699` | ptrauth ObjC isa macros | Darwin/arm64e class pointer signing | Not needed on FX | `exclude` | Define empty in FX headers | Not blocking |
| `CFRuntime.h:13-15` | includes `CFDictionary.h` | `copyFormattingDesc` callback uses `CFDictionaryRef` | Widening header dependency | `defer` | Forward-declare `CFDictionaryRef` for Subset 0 | Not blocking |
| `CFRuntime.h:68-98` | `CFRuntimeClass` | Core class dispatch table | Required | `required-for-subset` | Keep, with deferred callbacks allowed null | Not blocking |
| `CFRuntime.h:203-231` | Swift vs pure-C `CFRuntimeBase` | Runtime layout switch | Pure-C branch required | `required-for-subset` | Use non-Swift layout only | Blocking if Swift branch selected |
| `CFRuntime.c:192-263` | hard-coded class table | Upstream registers many CF classes statically | Pulls strings, containers, run loops, streams, URL | `blocking` | Replace with Subset 0 minimal table | Blocking until replaced |
| `CFRuntime.c:298-320` | `_CFRuntimeRegisterClass` | Dynamic class registration | Required | `required-for-subset` | Adapt to minimal table and class pointer `_cfisa` | Not blocking |
| `CFRuntime.c:328-335` | lookup/unregister | Class lookup and unregister | Lookup required, unregister optional | `defer` | Implement lookup; reject or omit unregister initially | Not blocking |
| `CFRuntime.c:351-390` | `_cfinfoa` bit layout | Type/refcount/flags | Required | `required-for-subset` | Reuse or simplify with documented bit positions; Subset 0 may drop custom refcount, zombie, 32-bit external count, and broad constant-object bits except static allocator immortality | Not blocking |
| `CFRuntime.c:409-435` | `_cf_aligned_calloc` lacks BSD aligned allocation path | Alignment support | Not needed for Subset 0 runtime/allocator objects | `defer` | Use `posix_memalign` on FX when later subsets enable `_kCFRuntimeRequiresAlignment`; `posix_memalign` is available on FreeBSD and is POSIX.1-2001 | Not blocking |
| `CFRuntime.c:438-471` | `swift_allocObject`, `_swift_rc` | Swift runtime allocation branch | Rejected | `blocking` | Compile out and remove references | Blocking if compiled |
| `CFRuntime.c:472-560` | pure-C `_CFRuntimeCreateInstance` | Runtime allocation path | Required | `required-for-subset` | Adapt with `_cfisa = CFRuntimeClass *` and minimal allocator | Not blocking |
| `CFRuntime.c:731-740` | `CFGetTypeID` | Type lookup | Required | `required-for-subset` | Keep, without ObjC/Swift dispatch | Not blocking |
| `CFRuntime.c:772-775` | `CFCopyTypeIDDescription` creates `CFString` | Type ID description | Observation only before strings | `defer` | Exclude from Subset 0 public gate | Not blocking if deferred |
| `CFRuntime.c:786-808` | public retain/release wrappers | Ownership API | Required | `required-for-subset` | Keep, with FX fail-fast null handling | Not blocking |
| `CFRuntime.c:813-831` | collection retain/release helpers | Container callbacks | Useful later | `defer` | Keep concept for Subset 2, not Subset 0 | Not blocking |
| `CFRuntime.c:1172-1184` | `libdispatch_init` | Upstream initializes dispatch on non-macOS | Rejected for runtime proof | `blocking` | Compile with dispatch disabled or remove init | Blocking until fixed |
| `CFRuntime.c:1205-1207` | `__CFTSDInitialize` | Thread-specific data setup | Not needed for proof if allocator default is global | `defer` | Avoid CFPlatform TSD in first proof | Not blocking if deferred |
| `CFRuntime.c:1226-1233` | `__CFSwiftGetBaseClass` and ObjC class table fill | Swift bridge | Rejected | `blocking` | Compile out with `DEPLOYMENT_RUNTIME_SWIFT=0` | Blocking if compiled |
| `CFRuntime.c:1420-1509` | `_CFRetain` Swift branch and atomic pure-C branch | Ownership implementation | Pure-C branch required | `required-for-subset` | Use C11 atomics branch; drop Swift branch | Not blocking |
| `CFRuntime.c:1512-1533` | `_CFTryRetain`, ObjC tagged pointer checks | Weak/ObjC support | Not required | `exclude` | Omit until needed | Not blocking |
| `CFRuntime.c:1540-1735` | `_CFRelease` | Release/finalization/deallocation | Required | `required-for-subset` | Adapt pure-C branch, reject resurrection as required behavior | Not blocking |
| `CFRuntime.c:1740-1860` | Swift bridge structs/functions | Swift Foundation bridge | Rejected | `exclude` | Omit from FX runtime | Blocking if compiled |
| `CFBase.c:20-23` | macOS malloc zone/mach/dlfcn includes | Darwin allocator zone compatibility | Not needed on FX | `exclude` | Use non-macOS allocator path | Not blocking |
| `CFBase.c:298-402` | static allocator objects | Allocator bootstrap | Required with adaptation | `required-for-subset` | Keep concept, set `_cfisa` to allocator class pointer | Not blocking |
| `CFBase.c:410-416` | allocator description uses `CFStringCreateWithFormat` | Debug description | Deferred before strings | `defer` | Omit `copyDebugDesc` for allocator in Subset 0 | Not blocking |
| `CFBase.c:446-456` | `__CFAllocatorClass` | Static allocator `CFRuntimeClass` record | Required | `required-for-subset` | Keep this compile-time class-struct pattern, no string description callback, and point static allocator `_cfisa` fields at it directly | Not blocking |
| `CFBase.c:496-500` | `CFAllocatorCreate` halts under Swift runtime | Swift branch disallows custom allocators | Rejected | `blocking` | Force pure-C branch | Blocking if Swift branch selected |
| `CFBase.c:502-580` | `CFAllocatorCreate` pure-C implementation | Custom allocator path | Useful but can be constrained | `defer` | Include only after system allocator proof | Not blocking |
| `CFBase.c:582-727` | allocator allocate/reallocate/deallocate | Allocator API | Required minimum | `required-for-subset` | Keep simplified system/malloc/null behavior | Not blocking |
| `CFBase.c:762-789` | block-typed reallocation failure callbacks | Convenience helper uses Blocks syntax | Not needed | `exclude` | Omit from Subset 0 or replace with C callback | Blocking if compiled without Blocks |
| `CFBase.c:805-839` | `CFNull` singleton and string descriptions | Non-Subset-0 scalar | Excluded | `exclude` | Do not include `CFNull` in Subset 0 | Not blocking |
| `ForFoundationOnly.h:786-790` | `STATIC_CLASS_REF` becomes `NULL` outside Swift | Upstream pure-C leaves `_cfisa` without a runtime class pointer for static objects | Conflicts with CORT decision and static allocator bootstrap | `blocking` | Replace static init macros with FX class pointer init using compile-time `CFRuntimeClass` structs published into the class table during runtime init | Blocking until fixed |
| `Block_private.h:225-236` | `_Block_use_RR2` callbacks | Future BlocksRuntime ownership integration | Not Subset 0 | `defer` | Use in Subset 5 only | Not blocking |
| `Block_private.h:146` | `NSObject` mention in flag comment | ABI comment, not dependency | None for Subset 0 | `defer` | Ignore for runtime proof | Not blocking |

## Audit Judgment

The upstream source is viable as a semantic and structural reference for
Subset 0, but `CFRuntime.c` and `CFBase.c` should not be copied unchanged into
`cort-fx`.

The first implementation should either:

1. extract and heavily trim the pure-C runtime/allocator paths, or
2. implement a small clean FX runtime directly using the upstream layout and
   ownership algorithm as reference.

The second path is likely clearer for Subset 0 because the unchanged upstream
translation units pull in broad CoreFoundation class tables, dispatch
initialization, Swift branches, CFString diagnostics, TSD support, block
syntax, and non-runtime objects.

Hard implementation gates before FX code:

- force or replace `DEPLOYMENT_RUNTIME_SWIFT=0`
- disable dispatch for Subset 0
- replace the upstream hard-coded class table
- replace ObjC/Swift class table use with `_cfisa = CFRuntimeClass *`
- avoid `CFInternal.h` as a monolithic dependency
- avoid `CFString` diagnostics
- keep allocator bootstrap explicit

No evidence from this audit suggests ICU is required for Subset 0.

## First FX Implementation Readiness

Implementation placement:

- durable planning remains in this repo under `docs/` until the user explicitly
  permits creating the planned CORT workspace.
- the expected implementation location after permission is
  `../nx/cort/cort-fx/`.
- do not scatter Subset 0 source files under the Swift package or unrelated
  trees.

Build system choice:

- use a minimal standalone `Makefile` for the Subset 0 proof unless the final
  repository layout explicitly chooses CMake before implementation starts.
- produce a static library first.
- link only against libc and pthread for the runtime proof.
- do not reuse the upstream CoreFoundation CMake target for Subset 0.

Candidate initial file set for later implementation:

- `include/CFBase.h` trimmed for scalar base types, allocator declarations, and
  polymorphic functions
- `include/CFRuntime.h` trimmed for `CFRuntimeClass`, `CFRuntimeBase`, runtime
  registration, and instance creation
- `src/CFAllocator.c` or `src/CFBase.c` with only allocator bootstrap and
  `CFRangeMake`
- `src/CFRuntime.c` with only class table, object create, type ID, retain,
  release, finalization, and allocator lookup
- `src/FXCFInternal.h` with bit helpers, locks, atomics, and fail-fast helpers
- `tests/runtime_ownership_tests.c`

Required local shims:

- pthread mutex or C11 lock wrapper for class table registration
- C11 atomic helper for `_cfinfoa`
- minimal fail-fast/log hook that does not allocate CF objects
- minimal default allocator state
- optional test-only class name lookup returning `const char *`

Do not include in the first implementation:

- upstream `CFInternal.h` unchanged
- upstream `CoreFoundation_Prefix.h` unchanged
- upstream `CMakeLists.txt`
- `CFString`
- `CFNull`
- block-syntax helpers
- dispatch initialization
- Swift bridge structs
- ObjC class table/ptrauth helpers

## Local FX Proof Status

Temporary implementation location in this repo:

- `cort-fx/include/CoreFoundation/CFBase.h`
- `cort-fx/include/CoreFoundation/CFRuntime.h`
- `cort-fx/include/CoreFoundation/CoreFoundation.h`
- `cort-fx/src/FXCFInternal.h`
- `cort-fx/src/CFBase.c`
- `cort-fx/src/CFRuntime.c`
- `cort-fx/tests/runtime_ownership_tests.c`
- `cort-fx/tests/runtime_abort_tests.c`
- `cort-fx/tests/c_consumer_smoke.c`
- `cort-fx/Makefile`
- `cort-fx/README.md`
- `cort-fx/scripts/run_subset0_fx_artifacts.sh`
- `cort-fx/exports/subset0-exported-symbols.txt`
- `cort-mx/README.md`
- `cort-mx/scripts/common.sh`
- `cort-mx/scripts/run_subset0_runtime_ownership.sh`
- `cort-mx/scripts/run_subset0_public_allocator_compare.sh`
- `cort-mx/scripts/run_subset0_suite.sh`
- `cort-mx/expectations/subset0_runtime_ownership_expected.json`
- `cort-mx/src/cort_mx_subset0_runtime_ownership.c`
- `cort-mx/src/cort_subset0_public_allocator_compare.c`
- `tools/compare_subset0_json.exs`
- `tools/report_subset0_runtime_ownership.exs`
- `docs/cort-subset0-public-allocator-compare.md`
- `docs/cort-subset0-validation-workflow.md`
- `docs/cort-fx-subset0-provenance.md`

Local build and verification commands:

- `cd cort-fx && make clean test`
- `cd cort-fx && make check-deps`
- `cd cort-fx && make check-exports`
- `cd cort-fx && make compare-fx`
- `cd cort-fx && make artifact-run`
- `cd cort-fx && make compare-with-mx MX_JSON=/path/to/subset0_public_compare_mx.json`
- `cd cort-fx && make test-installed`
- `cd cort-mx && scripts/run_subset0_runtime_ownership.sh`
- `cd cort-mx && scripts/run_subset0_public_allocator_compare.sh`
- `cd cort-mx && scripts/run_subset0_suite.sh`

Local verification result on the FX host:

- strict build succeeded with `-std=c11 -Wall -Wextra -Werror -pedantic`
- `runtime_ownership_tests` passed
- `runtime_abort_tests` passed
- `c_consumer_smoke` passed
- installed-header `c_consumer_smoke` passed through `make test-installed`
- exported symbol list matched `cort-fx/exports/subset0-exported-symbols.txt`
- shared allocator comparison harness emitted
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`
- FX artifact-run packaging emitted a preservable run directory at
  `../wip-cort-gpt-artifacts/cort-fx/runs/subset0-fx-local/`
- FX-vs-MX comparison wrapper is ready and self-compare reports zero blockers
- MX execution scripts, expectation manifest, and runtime-ownership reporter are
  now present in the repo
- MX shell syntax and Elixir tooling were validated locally
- runtime-ownership reporting was exercised locally against a sample JSON
  fixture
- dynamic dependencies are limited to `libthr`, `libc`, and `libsys`
- no `swift`, `dispatch`, `objc`, `icu`, or `CoreFoundation` symbol matches
  were found in `nm -a build/bin/runtime_ownership_tests build/lib/libcortfx.a`
- default-hidden symbol visibility is enabled so local `_FXCF*` helpers do not
  leak into the pinned package surface
- misuse abort policy is locally proven for `CFRelease(NULL)`, `CFRetain(NULL)`,
  `CFAllocatorGetContext(non-allocator)`, double release, and retain during
  finalization

Current proof scope actually implemented:

- minimal public `CFBase.h` and `CFRuntime.h`
- minimal class table with built-in type sentinels and allocator class
- compile-time allocator bootstrap using static `CFRuntimeClass` structs and
  immortal allocator singletons
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

Local proof constraints and known variances:

- still temporary in this repo; no `../nx/cort/` workspace was created
- process-global default allocator only; no thread-local default allocator yet
- fixed-capacity runtime class table
- `_CFRuntimeUnregisterClassWithTypeID` remains a no-op
- `kCFAllocatorUseContext` is not supported as an input allocator
- custom allocator support is present only far enough for Subset 0 proof
- `CFAllocatorReallocate` returns `NULL` when an allocator does not provide a
  reallocate callback; there is no lossy allocate-and-free fallback
- MX validated the current runtime-ownership manifest with zero blockers and
  zero warnings on macOS
- MX and FX allocator comparison found zero blockers and zero warnings on the
  shared allocator surface
- allocator singleton retain-count magnitudes differ diagnostically between FX
  and MX, but pointer identity and required ownership semantics match
- implementation provenance and deliberate departures are recorded in
  `docs/cort-fx-subset0-provenance.md`
- shared FX/MX public allocator comparison flow is recorded in
  `docs/cort-subset0-public-allocator-compare.md`
- end-to-end validation workflow is recorded in
  `docs/cort-subset0-validation-workflow.md`
- MX execution instructions are recorded in `cort-mx/README.md`

## Subset 0 Semantic Ledger

| Subset | API or behavior | MX observation artifact | FX implementation status | Match level | Known variance | Test name | Migration status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | `CFRetain` returns same pointer and increments ownership | `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/out/subset0_runtime_ownership.json` | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `semantic` | retain-count numerics flexible | `runtime_retain_returns_same_pointer` | MX validated |
| 0 | `CFRelease` consumes one retain | `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/out/subset0_runtime_ownership.json` | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `semantic` | failure mode text flexible | `runtime_release_balances_retain` | MX validated |
| 0 | finalizer fires exactly once at last release | FX-only internal plus MX ownership observations | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `fx-specific` | macOS private runtime finalizer not probed | `runtime_finalizer_once` | Local proof only |
| 0 | `CFGetTypeID` returns stable process-local type ID | `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/out/subset0_runtime_ownership.json` | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `semantic` | numeric IDs may differ | `runtime_type_id_stable` | MX validated |
| 0 | `_CFRuntimeRegisterClass` registers custom class | No private MX probe by design | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `fx-specific` | public MX has no supported equivalent | `runtime_register_custom_class` | Local proof only |
| 0 | `_CFRuntimeCreateInstance` allocates typed object | No private MX probe by design | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `fx-specific` | internal FX mechanism only | `runtime_create_instance_sets_type` | Local proof only |
| 0 | default/system allocator bootstraps runtime allocation | `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/out/subset0_public_compare_mx.json` | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `semantic` | FX can use simpler bootstrap | `allocator_system_allocates_runtime_object` | MX validated |
| 0 | static allocator singletons are immortal | FX-specific, informed by public allocator singleton behavior | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `fx-specific` | native retain-count numerics ignored | `allocator_static_singletons_immortal` | Local proof only |
| 0 | `kCFAllocatorNull` refuses allocation | `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/out/subset0_public_compare_mx.json` | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `semantic` | error text flexible | `allocator_null_returns_null` | MX validated |
| 0 | retain/release thread safety | `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/out/subset0_runtime_ownership.json` | Implemented locally in `cort-fx`; verified by `runtime_ownership_tests` | `semantic` | exact retain count flexible | `runtime_concurrent_retain_release` | MX validated |
| 0 | `CFCopyTypeIDDescription` | Observation only | Deferred | `unknown` | requires `CFString` | Deferred | Not ready |
| 0 | `CFCopyDescription` | Observation only | Deferred | `unknown` | requires `CFString` | Deferred | Not ready |
| 0 | Objective-C bridge behavior | Out of scope | Rejected | `rejected` | no ObjC on FX | None | Not applicable |
| 0 | Swift runtime allocation | Out of scope | Rejected | `rejected` | no Swift runtime on FX | link audit | Not applicable |
| 0 | dispatch dependency | Out of scope | Rejected | `rejected` | no dispatch in runtime proof | link audit | Not applicable |
| 0 | ICU dependency | No matches in audit | Rejected for Subset 0 | `rejected` | none | link audit | Not applicable |

No LaunchX production path may depend on any row whose match level remains
`unknown`.

Actual Subset 0 MX validation outcome recorded on macOS:

- runtime-ownership report:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/summary.md`
- runtime-ownership JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/runtime-ownership/out/subset0_runtime_ownership.json`
- public allocator comparison report:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/out/subset0_public_compare_report.md`
- public allocator comparison MX JSON:
  `../wip-cort-gpt-artifacts/cort-mx/runs/subset0-mx-suite/public-allocator-compare/out/subset0_public_compare_mx.json`

Observed outcome:

- runtime-ownership: 0 blockers, 0 warnings
- allocator comparison: 0 blockers, 0 warnings
- diagnostic-only variance:
  `allocator_singleton_retain_identity` retain-count magnitude differs between
  FX and MX, but pointer identity semantics still match

## Exact MX Probe Request

Send this bounded request to the MX validation agent:

```text
Please validate CORT Subset 0: runtime identity and ownership core.

Use native macOS C CoreFoundation APIs only. Do not use Foundation, Objective-C,
private CoreFoundation runtime APIs, NSObject, NSArray, NSDictionary, NSString,
NSNumber, objc/runtime.h, objc/message.h, or Swift.

Do not call private CFRuntime registration or allocation APIs on macOS.
Subset 0 FX will have internal runtime registration/allocation, but MX should
only validate public observable CoreFoundation behavior that informs it.

Produce a run directory using the cort-mx validation plan shape:

  host.txt
  toolchain.txt
  summary.md
  sha256.txt
  src/cort_mx_subset0_runtime_ownership.c
  bin/cort_mx_subset0_runtime_ownership
  out/subset0_runtime_ownership.json
  out/subset0_runtime_ownership.stdout
  out/subset0_runtime_ownership.stderr

Compile with:

  clang -Wall -Wextra -Werror \
    -framework CoreFoundation \
    src/cort_mx_subset0_runtime_ownership.c \
    -o bin/cort_mx_subset0_runtime_ownership

The MX probe intentionally creates native macOS objects beyond the Subset 0 FX
implementation scope, including strings, data, numbers, booleans, dates,
arrays, dictionaries, and errors. That is because macOS already provides these
types, and their public ownership behavior is useful evidence for the FX
runtime substrate. FX does not implement those object types in Subset 0; it only
implements the runtime and allocator mechanisms that will later support them.

Required Subset 0 cases:

1. Record host and toolchain:
   - macOS version
   - architecture
   - clang version
   - SDK path
   - exact compile command

2. Public type identity:
   - create a CFString with CFStringCreateWithCString
   - create a CFData with CFDataCreate
   - create a CFNumber with CFNumberCreate
   - use kCFBooleanTrue and kCFBooleanFalse
   - create a CFDate with CFDateCreate
   - create an empty CFArray with CFArrayCreate
   - create an empty CFDictionary with CFDictionaryCreate
   - create a CFError with CFErrorCreate if possible without Foundation
   - record CFGetTypeID for each
   - record CFCopyTypeIDDescription for each as diagnostic text only
   - classify numeric type ID equality as not required for FX
   - classify object-specific type implementation for strings, data, numbers,
     booleans, dates, arrays, dictionaries, and errors as future-subset
     evidence, not as a Subset 0 FX implementation gate

3. Basic retain/release:
   - for at least CFString and CFData, record that CFRetain returns the same
     pointer
   - balance retains with CFRelease
   - record CFGetRetainCount only as diagnostic output, not as required parity
   - do not intentionally call CFRetain(NULL) or CFRelease(NULL)

4. Create/Copy/Get ownership:
   - show a Create API returns a usable owned object that must be released
   - show a Copy API returns a usable owned object that must be released,
     using a public C CoreFoundation Copy function that does not require
     Foundation or Objective-C
   - show a Get API returns a borrowed value using CFArrayGetValueAtIndex or
     CFDictionaryGetValue and does not trigger callback retain/release
   - classify this as public ownership-convention evidence only; it does not
     require FX containers in Subset 0

Optional non-gating future Subset 2 evidence cases:

5. Container callback ownership observation:
   - create a CFMutableArray with custom retain/release callbacks that only
     increment counters and return the input pointer
   - append a sentinel pointer value
   - call CFArrayGetValueAtIndex
   - remove the value
   - release the array
   - record retain and release callback counts
   - classify as semantic-match-only with an ownership note that this is
     semantic evidence for future Subset 2, not required for Subset 0

6. Dictionary callback ownership observation:
   - create a CFMutableDictionary with custom key and value callbacks that only
     increment counters and return the input pointer
   - set one sentinel key/value pair
   - get the value
   - replace the value for the same key if supported by the callbacks
   - remove the key
   - release the dictionary
   - record key retain/release and value retain/release counts
   - classify as semantic-match-only with an ownership note that this is
     semantic evidence for future Subset 2, not required for Subset 0

7. Description observation:
   - call CFCopyDescription for one sample object if it is useful
   - classify the result as semantic-only/diagnostic, not a Subset 0 FX
     requirement because FX will not have CFString in this subset

JSON output requirements:

Emit JSON manually with `fprintf` plus a minimal string-escape helper. Do not
add Foundation, Objective-C, or a third-party JSON dependency just to format
probe output.

For each case, emit:

  name
  classification: required, semantic-match-only, macos-only-observation,
                  required-but-error-text-flexible, or out-of-scope
  success
  api_calls
  type_id where relevant
  type_description where relevant
  pointer_identity_same where relevant
  retain_callback_count where relevant
  release_callback_count where relevant
  ownership_note
  fx_match_level: exact, semantic, error-text-flexible, macos-only,
                  rejected, or unknown

Expected classifications:

- public retain/release ownership: required / semantic
- Create/Copy/Get ownership: required / semantic
- container insertion/removal callback ownership: semantic-match-only with
  `ownership_note` marking it as future Subset 2 evidence, not required for
  Subset 0
- numeric CFTypeID values: semantic-match-only, numeric equality not required
- CFCopyDescription text: semantic-match-only diagnostic
- Objective-C bridge behavior: out-of-scope or macos-only-observation if
  accidentally encountered

Please include a summary.md that states which observations are required for FX
Subset 0, which are semantic-only, and which are explicitly not required.
```

Draft probe source for that request now exists at
`cort-mx/src/cort_mx_subset0_runtime_ownership.c`. It is intentionally manual
`fprintf` JSON with no Foundation, Objective-C, or third-party JSON dependency.

## Exit Gate For This Artifact

Subset 0 is ready for the next handoff step when:

- MX accepts and runs the probe request above.
- MX artifacts are preserved.
- semantic ledger rows are updated from `Pending` to concrete artifact paths.
- the current local FX proof is compared against the MX-required and
  semantic-only rows and any mismatches are recorded.
- extraction or migration beyond the local proof starts only against rows
  classified as required or fx-specific.

A temporary local FX runtime proof was implemented as part of this artifact.
That proof is a useful milestone, but it is not the semantic oracle. MX
validation remains the gate before broader CORT extraction or LaunchX use.
