# CORT Subset 3A Fixture Corpus Plan

Date: 2026-04-17

Status: planned corpus contract for the current MX probe and the later FX
implementation.

This document records the fixture names and semantic expectations for the first
binary-plist slice.

It exists for one practical reason:

- once MX starts producing `.bplist` fixtures under the artifact root, the FX
  lane should already know which files matter, what each file means, and which
  tests should consume each one

This document does not require the binary fixture bytes to live in the repo.
The current policy still applies:

- keep large generated output outside the repo under `../wip-cort-gpt-artifacts/`
- only commit small shared artifacts deliberately when they are needed for
  coordination

## Artifact Root Shape

The MX Subset 3A probe writes fixtures under:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-bplist-core/fixtures/`

The same filenames should later appear under suite runs at:

- `../wip-cort-gpt-artifacts/cort-mx/runs/subset3a-mx-suite/bplist-core/fixtures/`

FX should not assume those paths are in-repo. The stable contract is the
fixture name, not the temporary absolute host path.

## Fixture Set

### `ascii_string_root.bplist`

Produced by:

- `bplist_ascii_string_roundtrip`

Top-level semantic meaning:

- root object is a `CFString`
- logical text is `launchd.packet-key`
- written form begins with `bplist00`
- expected tag class is ASCII string

Required later FX assertions:

- FX parser returns a `CFString`
- `CFStringGetLength` matches the MX semantic summary
- UTF-8 export matches the expected hex summary

### `unicode_string_root.bplist`

Produced by:

- `bplist_unicode_string_roundtrip`

Top-level semantic meaning:

- root object is a `CFString`
- logical text is the current bounded non-ASCII sample
- written form begins with `bplist00`
- expected tag class is UTF-16 string

Required later FX assertions:

- FX parser returns a `CFString`
- logical string value matches the MX semantic summary
- UTF-16 path is exercised, not only ASCII fallback behavior

### `mixed_array_root.bplist`

Produced by:

- `bplist_mixed_array_roundtrip`

Top-level semantic meaning:

- root object is a `CFArray`
- expected element kinds:
  - bool
  - int
  - real
  - date
  - data
  - string

Required later FX assertions:

- element count matches MX
- each supported scalar child round-trips semantically
- FX parser rejects if any element tag is unsupported rather than silently
  misparsing

### `string_key_dictionary_root.bplist`

Produced by:

- `bplist_string_key_dictionary_roundtrip`

Top-level semantic meaning:

- root object is a `CFDictionary`
- keys are strings only
- values exercise the supported scalar and string surface

Required later FX assertions:

- dictionary count matches MX
- each expected key is present
- value semantics match MX summaries
- dictionary lookup through string keys works after parse

### `invalid_header.bplist`

Produced by:

- `bplist_invalid_header_rejected`

Top-level semantic meaning:

- data is intentionally not a valid `bplist00` payload
- parser must reject safely

Required later FX assertions:

- parse returns `NULL`
- error object is present when `errorOut != NULL`
- `CFErrorGetCode` matches the validated read-corrupt category
- no crash, no out-of-bounds read

### `truncated_trailer.bplist`

Produced by:

- `bplist_truncated_trailer_rejected`

Top-level semantic meaning:

- data started as a valid binary plist but was truncated
- parser must reject safely

Required later FX assertions:

- parse returns `NULL`
- error object is present when `errorOut != NULL`
- `CFErrorGetCode` matches the validated read-corrupt category
- no crash, no out-of-bounds read

## Intentionally Missing Fixture

There should be no `.bplist` file for:

- `bplist_non_string_dictionary_key_write_rejected`

Reason:

- the writer is expected to reject before a valid binary payload exists
- preserving a bogus partial output would blur the success/failure contract

The semantic artifact for that row is the JSON observation, not a binary file.

## Semantic Summary Contract

The JSON summaries are the first cross-platform contract. The fixture bytes are
secondary evidence.

For valid fixtures, later FX tests should keep the same semantic summaries that
the MX probe uses:

- `length_value`
- `primary_value_text`
- `alternate_value_text`

Interpretation:

- `length_value` is logical top-level size
- `primary_value_text` is the normalized semantic payload summary
- `alternate_value_text` carries the expected format/tag class summary

For rejection fixtures:

- `length_value` is the expected public error-code class
- `primary_value_text` should stay `returned-null error-present`
- `alternate_value_text` is the normalized rejection label

## How FX Should Use The Corpus

After MX returns the real 3A artifacts, the FX lane should use the corpus in
two ways.

### Unit-test fixtures

The unit-test layer should consume the preserved MX fixture files for:

- parse-only tests
- malformed-input rejection tests
- cross-checking semantic summaries independently from the FX writer

### FX probe parity

The FX probe should emit semantic rows that match the MX probe contract, even
when the serialized bytes differ in non-semantic ways such as object-table
ordering.

The compare tool should keep treating:

- raw `type_id`
- exact binary layout
- exact error wording

as diagnostic-only unless later evidence proves otherwise.

## Shared-Artifact Policy

Do not commit the whole generated fixture tree by default.

Acceptable later exceptions:

- a very small shared corpus if MX and FX need one or two preserved binaries in
  the repo to unblock coordination
- a small handoff directory with explicit filenames and checksums, if keeping
  the files outside the repo causes repeated coordination failures

If that exception becomes necessary, keep it narrow and document why the repo
copy exists.

## Current Conclusion

The current fixture contract is sufficient for the first 3A code cut:

- four valid semantic fixtures
- two invalid read fixtures
- one rejection row with no binary artifact

That is enough to prove the first binary-plist implementation without dragging
in the full upstream property-list corpus.
