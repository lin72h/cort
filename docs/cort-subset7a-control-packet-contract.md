# CORT Subset 7A Control Packet Decode Contract

Date: 2026-04-17

This document defines the first bounded packet-facing slice after Subset 3A.

Subset 7A is not a broad `ControlProtocol` rewrite. It is the first
packet-compatibility proof on top of the proven CORT plist surface.

## Purpose

Subset 7A exists to prove one narrow claim:

- FX can decode the current Swift `ControlProtocol` binary-plist packet corpus
  into semantically matching packet objects
- FX rejects the bounded malformed-packet corpus with the same top-level
  rejection categories

This slice uses the Swift lane as the semantic authority because the packet
shape is a LaunchX-owned contract, not a CoreFoundation contract.

MX is not the semantic oracle for this slice.

## Source Of Truth

The packet contract is defined by:

- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `../wip-swift-gpt54x/swift/Tests/LaunchXControlTests/ControlProtocolTests.swift`
- the imported corpora under `fixtures/control/`

Repo-local shared packet artifacts:

- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_corpus_v1.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.json`
- `fixtures/control/subset7a_control_packet_expected_v1.json`

## Included

- accepted request-packet decode compatibility for the imported accepted corpus
- accepted response-packet decode compatibility for the imported accepted corpus
- rejected request/response packet compatibility for the imported rejection
  corpus
- exact top-level rejection categories:
  - `missing_key`
  - `unsupported_version`
  - `invalid_packet`
- canonical semantic summaries derived from the source corpora
- shared compare/report workflow for future FX packet probes

Current bounded corpus size:

- accepted cases: `59`
- rejected cases: `14`

## Excluded

- packet encode parity against Swift in this slice
- broad dispatcher/service behavior after a packet has decoded
- transport concerns:
  - `FDS`
  - socket framing
  - descriptor attachments
- packet write ordering parity
- byte-for-byte bplist output parity for future FX encoders
- broad malformed nested-type fuzzing beyond the imported rejection corpus
- replacing Swift `ControlProtocol`

## Required FX Behavior

For accepted corpus rows:

- FX packet decode succeeds
- decoded packet semantics match the canonical source-corpus packet object
- packet kind remains correct: request vs response
- response success/error shape remains correct

For rejected corpus rows:

- FX packet decode fails
- the failure matches the expected top-level rejection category and payload:
  - `missing_key(key)`
  - `unsupported_version(version)`
  - `invalid_packet(detail)`

## Canonical Semantic Surface

The future FX probe should emit one result per corpus case with this shape:

- `name`
- `classification`
- `validation_mode`
- `kind`
- `success`
- `primary_value_text`
- `alternate_value_text`

Required meanings:

- `classification`: `required`
- `validation_mode`: `accept` or `reject`
- `kind`: `request` or `response`
- `success`: whether FX matched the expected packet outcome for that case
- `primary_value_text`:
  - accepted case: canonical JSON form of the decoded packet object
  - rejected case: canonical JSON form of the expected top-level error object
- `alternate_value_text`:
  - accepted case: short envelope summary
  - rejected case: short rejection summary

## Dependencies

Subset 7A depends on:

- Subset 1A scalar semantics
- Subset 1B string semantics
- Subset 2A container semantics
- Subset 3A binary plist read semantics

It does not depend on:

- XML plist
- BlocksRuntime
- Zig wrappers
- Objective-C bridge behavior

## Exit Gate

Subset 7A is ready for implementation when:

- the shared control corpora are present in this repo
- the derived expected semantic corpus exists
- the compare workflow is stable
- the scope remains decode-only and rejection-boundary-only

Subset 7A is accepted when:

- an FX packet probe runs against the imported corpora
- compare against `fixtures/control/subset7a_control_packet_expected_v1.json`
  returns `0` blockers
- no row remains semantically `unknown`

## Stop Conditions

Stop and escalate if any of these happen:

- the imported Swift corpora and `ControlProtocol.swift` disagree
- future FX packet work requires packet semantics not represented in the shared
  corpora
- the packet slice starts depending on transport or dispatcher semantics
- the packet decode slice starts requiring broad encode parity before decode
  parity is proven
