# CORT Subset 7B Control Envelope Contract

Date: 2026-04-17

This document defines the next bounded packet-facing slice after Subset 7A.

Subset 7B is still not a broad `ControlProtocol` rewrite. It adds a small
internal typed-envelope layer on top of the proven 7A packet decode surface.

## Purpose

Subset 7B exists to prove one narrow claim:

- FX can decode the current Swift `ControlProtocol` packet corpus into stable
  internal request/response envelope views
- FX can summarize those envelope views in a small semantic surface suitable
  for future packet consumers
- FX preserves the bounded 7A rejection behavior while moving callers off raw
  top-level dictionary walking

This slice still uses the LaunchX Swift lane as the semantic authority.

MX is not the semantic oracle for this slice.

## Source Of Truth

The control-envelope contract is defined by:

- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `../wip-swift-gpt54x/swift/Tests/LaunchXControlTests/ControlProtocolTests.swift`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/subset7a_control_packet_expected_v1.json`

Repo-local derived expected corpus for this slice:

- `fixtures/control/subset7b_control_envelope_expected_v1.json`

## Included

- internal request envelope decode:
  - `protocol_version`
  - `service`
  - `method`
  - `params`
- internal response envelope decode:
  - `protocol_version`
  - `status`
  - success `result`
  - structured error payload with `code` and `message`
- borrowed-reference access to envelope fields while the owning packet root is
  retained internally
- request/response envelope JSON summaries derived from the shared corpora
- reuse of the bounded 7A accepted and rejected packet corpora
- binary-plist header detection for packet payloads

## Excluded

- packet encode parity
- transport/session behavior
- dispatcher/service execution after decode
- public CoreFoundation header exposure for packet-specific APIs
- broad packet mutation APIs
- widening the accepted/rejected corpus beyond the imported Swift lane

## Required FX Behavior

For accepted request rows:

- `_FXCFControlRequestInit` succeeds
- borrowed request fields expose the correct envelope data
- envelope JSON and envelope summary match the derived expected corpus

For accepted response rows:

- `_FXCFControlResponseInit` succeeds
- success/error envelope shape remains correct
- envelope JSON and envelope summary match the derived expected corpus

For rejected rows:

- the typed-envelope init helper fails
- top-level rejection category and payload remain aligned with the bounded 7A
  surface

## Canonical Semantic Surface

The future FX 7B probe emits one result row per shared packet case with:

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
- `success`: whether FX matched the expected envelope behavior
- `primary_value_text`:
  - accepted row: canonical envelope JSON
  - rejected row: canonical top-level rejection JSON
- `alternate_value_text`:
  - accepted row: short envelope summary
  - rejected row: short rejection summary

## Dependencies

Subset 7B depends on:

- Subset 7A packet decode and rejection compatibility
- Subset 3A binary-plist read semantics
- earlier scalar/string/container semantics through the existing plist layer

It does not depend on:

- XML plist
- BlocksRuntime
- Zig wrappers
- Objective-C bridge behavior
- packet encode parity

## Exit Gate

Subset 7B is ready when:

- the derived expected envelope corpus exists
- the compare workflow is stable
- the slice stays internal-only and decode-side

Subset 7B is accepted when:

- an FX envelope probe runs against the shared packet corpus
- compare against `fixtures/control/subset7b_control_envelope_expected_v1.json`
  returns `0` blockers
- the shared handoff artifact `subset7b_control_envelope_fx.json` is published

## Stop Conditions

Stop and escalate if any of these happen:

- the Swift packet corpora stop carrying enough envelope detail for this slice
- packet envelope work starts requiring encode parity to make progress
- the typed-envelope slice starts leaking into transport or dispatcher
  semantics
- a future consumer needs packet fields not represented in the current shared
  corpora
