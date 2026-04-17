# cort-fx Subset 0, 1A, 1B, 2A, 3A, 7A, And 7B Proof

This directory holds the temporary standalone `cort-fx` proof for CORT
Subset 0, Subset 1A, Subset 1B, Subset 2A, Subset 3A, and Subset 7A:

- Subset 0: runtime and ownership core
- Subset 1A: immutable scalar core without `CFString`
- Subset 1B: minimal immutable `CFString` for plist-valid keys and later
  bplist string paths
- Subset 2A: bounded container core for arrays, dictionaries, and ownership
  semantics for plist-valid object graphs
- Subset 3A: bounded binary-plist core read/write work on top of the proven
  runtime, scalar, string, and container surface
- Subset 7A: control-packet decode and rejection compatibility against the
  imported Swift packet corpora
- Subset 7B: internal request/response control-envelope accessors and semantic
  summaries on top of the proven packet decode surface

The active 3A coordination documents live in:

- `../docs/cort-subset3a-bplist-core-contract.md`
- `../docs/cort-subset3a-source-audit-and-readiness.md`
- `../docs/cort-subset3a-validation-workflow.md`
- `../docs/cort-subset3a-fx-implementation-plan.md`
- `../docs/cort-subset3a-fixture-corpus.md`

Subset 7A control-packet decode compatibility now lives on top of the proven
plist surface.

The Subset 7A docs live in:

- `../docs/cort-subset7a-control-packet-contract.md`
- `../docs/cort-subset7a-source-audit-and-readiness.md`
- `../docs/cort-subset7a-validation-workflow.md`

The Subset 7B docs live in:

- `../docs/cort-subset7b-control-envelope-contract.md`
- `../docs/cort-subset7b-source-audit-and-readiness.md`
- `../docs/cort-subset7b-validation-workflow.md`

The shared imported packet corpora live in:

- `../fixtures/control/`

It is not the final repository layout. The final target remains
`../nx/cort/cort-fx/` after explicit permission.

## Current Scope

Implemented:

- `CFRuntimeBase` with `_cfisa` as `CFRuntimeClass *`
- minimal runtime class table
- allocator bootstrap with immortal static allocator singletons
- `_CFRuntimeRegisterClass`
- `_CFRuntimeGetClassWithTypeID`
- `_CFRuntimeCreateInstance`
- `_CFRuntimeInitStaticInstance`
- `CFGetTypeID`
- `CFRetain`
- `CFRelease`
- `CFGetRetainCount`
- `CFGetAllocator`
- `CFEqual`
- `CFHash`
- `CFAllocatorGetDefault`
- `CFAllocatorSetDefault`
- `CFAllocatorCreate`
- `CFAllocatorAllocate`
- `CFAllocatorReallocate`
- `CFAllocatorDeallocate`
- `CFAllocatorGetPreferredSizeForSize`
- `CFAllocatorGetContext`
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
- `CFStringGetTypeID`
- `CFStringCreateWithCString`
- `CFStringCreateWithBytes`
- `CFStringCreateWithCharacters`
- `CFStringCreateCopy`
- `CFStringGetLength`
- `CFStringGetCharacters`
- `CFStringGetCString`
- `CFStringGetBytes`
- `CFArrayGetTypeID`
- `CFArrayCreate`
- `CFArrayCreateCopy`
- `CFArrayCreateMutable`
- `CFArrayGetCount`
- `CFArrayGetValueAtIndex`
- `CFArrayAppendValue`
- `CFArrayRemoveValueAtIndex`
- `kCFTypeArrayCallBacks`
- `CFDictionaryGetTypeID`
- `CFDictionaryCreate`
- `CFDictionaryCreateCopy`
- `CFDictionaryCreateMutable`
- `CFDictionaryGetCount`
- `CFDictionaryGetValue`
- `CFDictionaryGetValueIfPresent`
- `CFDictionarySetValue`
- `CFDictionaryRemoveValue`
- `kCFTypeDictionaryKeyCallBacks`
- `kCFTypeDictionaryValueCallBacks`
- `CFErrorGetTypeID`
- `CFErrorGetCode`
- `CFPropertyListCreateData`
- `CFPropertyListCreateWithData`
- binary plist core for bool, int, real, date, data, string, array, and
  dictionary with string keys
- internal control-packet decode and rejection compatibility for the imported
  request/response packet corpora
- internal request/response control-envelope accessors and semantic summaries
  for the imported packet corpora

Explicitly not implemented here:

- XML plist
- OpenStep plist
- property-list stream APIs
- mutable property-list parse modes
- unsupported binary-plist tags such as UID / set / ordset
- BlocksRuntime integration
- Objective-C bridge behavior
- Swift runtime allocation
- broader plist tag families beyond the bounded 3A surface
- mutable parse/write modes beyond the bounded 3A surface
- packet encode parity
- transport/session behavior beyond the bounded decode/rejection surface
- public packet-specific APIs beyond the bounded internal envelope layer

## Provenance

This proof is based on the `CoreFoundation` source shape in
`../nx/swift-corelibs-foundation/Sources/CoreFoundation`, especially:

- `include/CFBase.h`
- `include/CFRuntime.h`
- `CFBase.c`
- `CFRuntime.c`

It is not a direct copy. The implementation here was rewritten as a small
standalone proof because the upstream translation units assume Swift-runtime
defaults, dispatch, and a much broader class table than Subset 0 allows.

Native macOS CoreFoundation remains the semantic oracle for MX validation.

Default build and run artifacts are written outside this repo to a sibling
directory named after the repo, currently `../wip-cort-gpt-artifacts/`.
Override that root with `CORT_ARTIFACTS_ROOT=/path/to/artifacts`.

## Build

```sh
cd cort-fx
make clean test
```

This builds under `../wip-cort-gpt-artifacts/cort-fx/build/` by default:

- `lib/libcortfx.a`
- `bin/runtime_ownership_tests`
- `bin/runtime_abort_tests`
- `bin/scalar_core_tests`
- `bin/string_core_tests`
- `bin/container_core_tests`
- `bin/bplist_core_tests`
- `bin/control_packet_tests`
- `bin/control_envelope_tests`
- `bin/c_consumer_smoke`
- `bin/subset0_public_compare_fx`
- `bin/subset1_scalar_core_fx`
- `bin/subset1b_cfstring_fx`
- `bin/subset2a_container_fx`
- `bin/subset3a_bplist_fx`
- `bin/subset7a_control_packet_fx`
- `bin/subset7b_control_envelope_fx`

## Verification Targets

```sh
cd cort-fx
make check-deps
make check-exports
make compare-fx
make compare-subset1a-fx
make compare-subset1b-fx
make compare-subset2a-fx
make compare-subset3a-fx
make compare-subset7a-fx
make compare-subset7b-fx
make compare-subset1a-with-mx
make compare-subset1b-with-mx
make compare-subset2a-with-mx
make compare-subset3a-with-mx
make compare-subset7a-with-expected
make compare-subset7b-with-expected
make artifact-run
make artifact-subset1a-compare
make artifact-subset2a-compare
make artifact-subset3a-compare
make test-installed
```

What they enforce:

- no Swift, Objective-C, dispatch, ICU, or CoreFoundation link/symbol coupling
- public exported symbol set matches `exports/subset3a-exported-symbols.txt`
- abort-on-misuse policy remains enforced for invalid public calls and
  ownership failures
- dependency/export audits are normalized across Darwin and `ldd`-based hosts
- wrong-type abort coverage includes `CFDataCreateCopy`, `CFDataGetBytePtr`,
  `CFNumberGetType`, `CFArrayCreateCopy`, `CFArrayRemoveValueAtIndex`,
  `CFDictionaryCreateCopy`, `CFDictionaryGetValueIfPresent`,
  `CFDictionaryRemoveValue`, `CFErrorGetCode`, `CFEqual`, and `CFHash`
- installed headers and static library are usable by a standalone C consumer,
  including Subset 3A binary plist create/read and public error-code handling
- shared allocator comparison harness emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`
- FX scalar-core probe emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx.json`
- FX CFString probe emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1b_cfstring_fx.json`
- FX container probe emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset2a_container_fx.json`
- FX binary-plist probe emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset3a_bplist_fx.json`
- FX control-packet probe emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7a_control_packet_fx.json`
- FX control-packet compare emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7a_control_packet_fx_vs_expected_report.md`
- FX control-envelope probe emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7b_control_envelope_fx.json`
- FX control-envelope compare emits
  `../wip-cort-gpt-artifacts/cort-fx/build/out/subset7b_control_envelope_fx_vs_expected_report.md`

The 7A packet slice is implemented with:

- `tests/control_packet_tests.c`
- `tests/cort_subset7a_control_packet_fx.c`
- `tests/subset7a_control_packet_support.h`
- `tests/generated/subset7a_control_packet_cases.h`
- `../tools/build_subset7a_control_packet_cases_header.exs`

The 7B control-envelope slice is implemented with:

- `src/FXCFControlPacket.c`
- `src/FXCFControlPacket.h`
- `tests/control_envelope_tests.c`
- `tests/cort_subset7b_control_envelope_fx.c`
- `tests/subset7b_control_envelope_support.h`
- `../tools/build_subset7b_control_envelope_expected.exs`

Shared handoff artifact:

- `../subset7a_control_packet_fx.json`
- Subset 1A compare wrapper can compare that FX JSON against MX and preserve a
  dedicated handoff artifact run
- shared handoff artifacts may also be committed at repo root for MX
  consumption, including `subset0_public_compare_fx.json`,
  `subset1_scalar_core_fx.json`, `subset1b_cfstring_fx.json`,
  `subset2a_container_fx.json`, `subset3a_bplist_fx.json`, and
  `subset7a_control_packet_fx.json`
- Subset 2A compare wrapper can compare a real FX container JSON against MX and
  preserve a dedicated handoff artifact run once the FX artifact exists
- Subset 3A compare wrapper can compare a real FX binary-plist JSON against MX
  and preserve a dedicated handoff artifact run once the FX artifact exists
- artifact-run packaging emits a preservable FX run directory under
  `../wip-cort-gpt-artifacts/cort-fx/runs/`
- repo workflow selfcheck can validate the compare/report tooling and the FX
  `make clean test` target with `../tools/run_elixir.sh ../tools/workflow_selfcheck.exs`
- when that selfcheck runs on Darwin, it also executes the real MX Subset 0,
  Subset 1A, Subset 1B, and Subset 2A scripts under a temporary artifact root

## Install

```sh
cd cort-fx
make install PREFIX=/usr/local DESTDIR=/tmp/cort-fx-stage
```

Installed files:

- `include/CoreFoundation/CFBase.h`
- `include/CoreFoundation/CFArray.h`
- `include/CoreFoundation/CFData.h`
- `include/CoreFoundation/CFDate.h`
- `include/CoreFoundation/CFDictionary.h`
- `include/CoreFoundation/CFError.h`
- `include/CoreFoundation/CFNumber.h`
- `include/CoreFoundation/CFPropertyList.h`
- `include/CoreFoundation/CFString.h`
- `include/CoreFoundation/CFRuntime.h`
- `include/CoreFoundation/CoreFoundation.h`
- `lib/libcortfx.a`
- `share/doc/cort-fx/README.md`

## Shared Compare Harness

The shared public allocator comparison harness source lives at:

- `../cort-mx/src/cort_subset0_public_allocator_compare.c`

FX runs it with:

```sh
cd cort-fx
make compare-fx
```

That produces:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_public_compare_fx.json`

MX can compile the same source against native macOS CoreFoundation to produce a
matching JSON artifact for side-by-side review.

## Scalar-Core Probe

FX runs the local Subset 1A scalar-core probe with:

```sh
cd cort-fx
make compare-subset1a-fx
```

That produces:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx.json`

MX should compare that against:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1-mx-scalar-core/out/subset1_scalar_core.json`

## CFString Probe

FX runs the local Subset 1B CFString probe with:

```sh
cd cort-fx
make compare-subset1b-fx
```

That produces:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1b_cfstring_fx.json`

MX should compare that against:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json`

## Container Probe

FX runs the local Subset 2A container probe with:

```sh
cd cort-fx
make compare-subset2a-fx
```

That produces:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset2a_container_fx.json`

MX should compare that against:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset2a-mx-container-core/out/subset2a_container_core.json`

## Binary-Plist Probe

FX runs the local Subset 3A binary-plist probe with:

```sh
cd cort-fx
make compare-subset3a-fx
```

That produces:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset3a_bplist_fx.json`

The shared handoff artifact may also be published at:

- `../subset3a_bplist_fx.json`

MX should compare that against:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/out/subset3a_bplist_core.json`

## Publish Shared Handoff Artifacts

Refresh the repo-root FX handoff JSONs from the latest local build outputs
with:

```sh
cd cort-fx
make publish-shared-artifacts
```

Subset-specific publish targets also exist:

- `make publish-subset0-artifact`
- `make publish-subset1a-artifact`
- `make publish-subset1b-artifact`
- `make publish-subset2a-artifact`
- `make publish-subset3a-artifact`

The workflow selfcheck treats those shared repo-root JSONs as part of the
coordination contract and verifies that they exactly match freshly built FX
probe output before the workflow is considered clean.

## Subset 2A Compare With MX

Compare a real FX Subset 2A container JSON against the MX container JSON with:

```sh
cd cort-fx
make compare-subset2a-with-mx \
  FX_CONTAINER_JSON=/path/to/subset2a_container_fx.json \
  MX_JSON=/path/to/subset2a_container_core.json
```

This uses:

- `../tools/compare_subset2a_container_json.exs`

and writes:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset2a_container_fx_vs_mx_report.md`

To preserve a compare-only handoff run:

```sh
cd cort-fx
make artifact-subset2a-compare \
  FX_CONTAINER_JSON=/path/to/subset2a_container_fx.json \
  MX_JSON=/path/to/subset2a_container_core.json
```

Default output:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset2a-fx-vs-mx/`

The compare artifact directory includes:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- `out/subset2a_container_fx.json`
- `out/subset2a_container_mx.json`
- `out/subset2a_container_fx_vs_mx_report.md`

## Artifact Run

Capture a preservable local FX run directory:

```sh
cd cort-fx
make artifact-run
```

Default output:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset0-fx-local/`

Override the run directory:

```sh
cd cort-fx
make artifact-run RUN_DIR=/absolute/path/subset0-fx-20260416
```

The run directory includes:

- host and toolchain metadata
- commands used
- per-step stdout/stderr
- the pinned and actual export snapshots
- the FX allocator comparison JSON
- installed headers and built binaries
- `sha256.txt`

## Compare With MX

Once MX produces `subset0_public_compare_mx.json`, compare it against the FX
artifact with:

```sh
cd cort-fx
make compare-with-mx MX_JSON=/path/to/subset0_public_compare_mx.json
```

This uses:

- `../tools/compare_subset0_json.exs`

and writes:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset0_fx_vs_mx_report.md`

## Subset 1A Compare With MX

Compare the local FX scalar-core JSON against the MX scalar-core JSON with:

```sh
cd cort-fx
make compare-subset1a-with-mx \
  MX_JSON=/path/to/subset1_scalar_core.json
```

This uses:

- `../tools/compare_subset1_scalar_core_json.exs`

and writes:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset1_scalar_core_fx_vs_mx_report.md`

To preserve a compare-only handoff run:

```sh
cd cort-fx
make artifact-subset1a-compare \
  FX_SCALAR_JSON=/path/to/subset1_scalar_core_fx.json \
  MX_JSON=/path/to/subset1_scalar_core.json
```

Default output:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset1a-fx-vs-mx/`

The compare artifact directory includes:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- `out/subset1_scalar_core_fx.json`
- `out/subset1_scalar_core_mx.json`
- `out/subset1a_fx_vs_mx_report.md`

## Subset 2A Compare With MX

Compare the local FX container JSON against the MX container JSON with:

```sh
cd cort-fx
make compare-subset2a-with-mx \
  MX_JSON=/path/to/subset2a_container_core.json
```

This uses:

- `../tools/compare_subset2a_container_json.exs`

and writes:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset2a_container_fx_vs_mx_report.md`

## Subset 3A Compare With MX

Compare the FX Subset 3A binary-plist JSON against the MX JSON with:

```sh
cd cort-fx
make compare-subset3a-with-mx \
  FX_BPLIST_JSON=/path/to/subset3a_bplist_fx.json \
  MX_JSON=/path/to/subset3a_bplist_core.json
```

This uses:

- `../tools/compare_subset3a_bplist_json.exs`

and writes:

- `../wip-cort-gpt-artifacts/cort-fx/build/out/subset3a_bplist_fx_vs_mx_report.md`

To preserve a compare-only handoff run:

```sh
cd cort-fx
make artifact-subset3a-compare \
  FX_BPLIST_JSON=/path/to/subset3a_bplist_fx.json \
  MX_JSON=/path/to/subset3a_bplist_core.json
```

Default output:

- `../wip-cort-gpt-artifacts/cort-fx/runs/subset3a-fx-vs-mx/`

The compare artifact directory includes:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- `out/subset3a_bplist_fx.json`
- `out/subset3a_bplist_mx.json`
- `out/subset3a_bplist_fx_vs_mx_report.md`

## Constraints

- process-global default allocator only
- fixed-capacity class table
- `_CFRuntimeUnregisterClassWithTypeID` is a no-op
- `kCFAllocatorUseContext` is not supported as an input allocator
- `CFAllocatorReallocate` returns `NULL` when the allocator does not provide a
  reallocate callback
- containers currently support only the tested `kCFType` callback mode
- container equality/hash, enumeration, and broader callback compatibility are
  intentionally deferred
- this proof is useful evidence, not the semantic oracle

MX validation artifacts remain the gate before broader extraction or LaunchX
use.
