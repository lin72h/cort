# CORT Subset 1B Minimal CFString Contract

Date: 2026-04-16

Status: bounded MX validation exists on the original 1B surface; bounded FX
implementation exists locally; final tightened-surface MX rerun is still
pending.

This document defines the next narrow slice after validated Subset 1A:
minimal immutable `CFString` semantics required by future bplist and
plist-valid key paths.

Purpose:

- validate the smallest useful `CFString` surface on macOS before broadening
  FX string work
- support the string creation and extraction paths that future binary plist
  work will actually need
- keep ICU, locale, formatting, mutable strings, and broad text processing out
  of the first FX string slice

## Scope

This is not “all of `CFString`”.

This slice is the minimum immutable string core that can later support:

- plist-valid dictionary keys
- bplist ASCII string write paths
- bplist Unicode string write paths
- bplist ASCII and UTF-16 string read paths

It is deliberately smaller than the public CoreFoundation header.

## Included APIs

### Type and ownership

- `CFStringGetTypeID`
- `CFRetain`
- `CFRelease`
- `CFGetTypeID`
- `CFEqual`
- `CFHash`

### Creation

- `CFStringCreateWithCString`
  - tested with `kCFStringEncodingASCII`
  - tested with `kCFStringEncodingUTF8`
- `CFStringCreateWithBytes`
  - tested with `kCFStringEncodingASCII`
  - `isExternalRepresentation = false`
- `CFStringCreateWithCharacters`
- `CFStringCreateCopy`

### Access

- `CFStringGetLength`
- `CFStringGetCharacters`
- `CFStringGetCString`
  - required UTF-8 export path
- `CFStringGetBytes`
  - required only for full-range ASCII extraction used by later bplist write
    fast paths

## Explicitly Excluded

- mutable string APIs
- no-copy creation APIs
- `CFSTR`
- `CFStringGetCStringPtr`
- `CFStringGetCharactersPtr`
- locale-sensitive compare behavior
- `CFStringCompare`
- `CFStringFind`
- `CFStringGetIntValue`
- normalization
- case folding
- localized formatting
- file-system representation APIs
- broad legacy encoding support beyond the tested ASCII and UTF-8 create paths

## Required MX Observations

For the tested surface, MX must preserve observations for:

- type identity via `CFGetTypeID`
- UTF-16 code-unit length via `CFStringGetLength`
- UTF-8 export through `CFStringGetCString`
- UTF-16 export through `CFStringGetCharacters`
- zero-length creation and roundtrip through the tested creation paths
- ASCII bytes creation via `CFStringCreateWithBytes`
- UTF-16 character creation via `CFStringCreateWithCharacters`
- Create/Copy ownership via `CFStringCreateCopy`
- equality and hash coherence across equivalent strings created by different
  APIs
- exact-fit success and buffer-too-small failure through `CFStringGetCString`
- malformed UTF-8 rejection through `CFStringCreateWithBytes`
- malformed UTF-8 rejection through `CFStringCreateWithCString`

Required interpretation rules:

- `CFStringGetLength` is observed as UTF-16 code-unit count, not Unicode scalar
  count and not grapheme count
- raw `CFHash` numbers are diagnostic only across implementations
- raw `CFTypeID` numbers are diagnostic only across implementations
- `CFStringCreateCopy` pointer identity is diagnostic only; owned-copy
  semantics matter, not whether the same pointer is returned
- malformed UTF-8 rejection must be judged by safe failure and absence of a
  returned object, not error wording

## Required FX Behavior

### Creation and storage

- strings are immutable
- `CFStringCreateWithCString` accepts tested ASCII and UTF-8 inputs
- zero-length tested inputs are accepted across the included creation APIs
- `CFStringCreateWithBytes` accepts tested ASCII byte inputs with
  `isExternalRepresentation = false`
- `CFStringCreateWithCharacters` copies the caller-supplied UTF-16 code units
- `CFStringCreateCopy` returns an owned string with equal public value
- malformed tested UTF-8 input is rejected safely with a `NULL` result

### Access

- `CFStringGetLength` returns UTF-16 code-unit count
- `CFStringGetCharacters` returns the stored code units exactly for the tested
  ranges
- `CFStringGetCString` emits UTF-8 bytes for the tested strings when the buffer
  is large enough
- `CFStringGetCString` succeeds for an exact-fit output buffer
- `CFStringGetCString` returns `false` when the supplied buffer is too small
- `CFStringGetBytes` over the full range of an ASCII string with ASCII encoding
  returns the full character count and byte count needed by the future bplist
  write fast path

### Equality and hashing

- strings with equal code-unit content compare equal even when created through
  different included APIs
- equal strings hash coherently within one FX process
- unequal tested strings compare unequal

## Acceptable FX Variance

- raw `CFHash` numbers do not need to match macOS
- raw `CFTypeID` numbers do not need to match macOS
- internal storage may differ from macOS
- `CFStringCreateCopy` may or may not return the same pointer as macOS
- ASCII fast-path implementation details may differ as long as the observed
  public result matches the tested behavior

## Rejected In This Slice

- importing upstream `CFString.c` unchanged
- broad string comparison or search work before plist paths need it
- ICU-backed normalization or locale work
- pointer-return optimizations treated as requirements
- mutable string implementation as a prerequisite for immutable string support

## Subset 1B Semantic Ledger Seed

| Subset | API or behavior | MX observation artifact | FX implementation status | Match level | Known variance | Test name | Migration status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1B | ASCII `CFStringCreateWithCString` roundtrip | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | type/hash numerics diagnostic | `cfstring_ascii_cstring_roundtrip` | MX run pending |
| 1B | zero-length string creation and roundtrip across tested creation APIs | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | type/hash numerics diagnostic | `cfstring_empty_roundtrip` | MX run pending |
| 1B | UTF-8 `CFStringCreateWithCString` roundtrip | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | type/hash numerics diagnostic | `cfstring_utf8_cstring_roundtrip` | MX run pending |
| 1B | ASCII `CFStringCreateWithBytes` plus ASCII `CFStringGetBytes` full-range extraction | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | type/hash numerics diagnostic | `cfstring_bytes_ascii_roundtrip` | MX run pending |
| 1B | UTF-16 `CFStringCreateWithCharacters` roundtrip | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | type/hash numerics diagnostic | `cfstring_utf16_characters_roundtrip` | MX run pending |
| 1B | `CFStringCreateCopy` ownership, equality, and hash coherence | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | copy pointer identity diagnostic only | `cfstring_copy_equality_hash` | MX run pending |
| 1B | `CFStringGetCString` exact-fit success and buffer-too-small failure | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | buffer contents outside success contract | `cfstring_getcstring_small_buffer` | MX run pending |
| 1B | malformed UTF-8 rejection via `CFStringCreateWithBytes` | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | error wording not relevant | `cfstring_bytes_invalid_utf8_rejected` | MX run pending |
| 1B | malformed UTF-8 rejection via `CFStringCreateWithCString` | `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/out/subset1b_cfstring_core.json` | Not implemented | `unknown` | error wording not relevant | `cfstring_cstring_invalid_utf8_rejected` | MX run pending |

## MX Probe Assets

Probe source:

- `cort-mx/src/cort_mx_subset1b_cfstring_core.c`

Expectation manifest:

- `cort-mx/expectations/subset1b_cfstring_core_expected.json`

Run script:

- `cort-mx/scripts/run_subset1b_cfstring_core.sh`

Future FX-vs-MX compare tool:

- `tools/compare_subset1b_cfstring_json.exs`

FX readiness note:

- `docs/cort-subset1b-source-audit-and-readiness.md`

## Expected MX Artifacts

The MX run should preserve:

- `host.txt`
- `toolchain.txt`
- `commands.txt`
- `summary.md`
- `sha256.txt`
- `src/cort_mx_subset1b_cfstring_core.c`
- `bin/cort_mx_subset1b_cfstring_core`
- `out/subset1b_cfstring_core.json`
- `out/subset1b_cfstring_core.stdout`
- `out/subset1b_cfstring_core.stderr`

## Exact MX Command

```sh
cd /Users/me/wip-launchx/wip-cort-gpt/cort-mx
scripts/run_subset1b_cfstring_core.sh
```

Default output:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset1b-mx-cfstring-core/`

## Exit Gate

Subset 1B is ready for FX implementation when:

- MX run artifacts exist
- the report has no blocker probe failures
- the included creation and access paths are classified explicitly
- zero-length creation semantics are recorded explicitly
- malformed UTF-8 rejection is recorded explicitly
- ASCII fast-path extraction semantics are recorded explicitly
- excluded behaviors remain excluded instead of leaking into the slice
