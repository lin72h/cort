# CORT Subset 7C Control Request Route Contract

Date: 2026-04-17

This document defines the next bounded packet-facing slice after Subset 7B.

Subset 7C is the first consumer-facing packet cut on top of the proven
request/response envelope layer. It still stays decode-side.

## Purpose

Subset 7C exists to prove one narrow claim:

- FX can classify accepted request packets into stable internal route kinds
- FX can expose the known request parameter family through a typed internal
  view without forcing future consumers to walk raw dictionaries
- FX preserves the bounded 7A rejection surface for malformed request packets

This slice still uses the LaunchX Swift lane as the semantic authority.

MX is not the semantic oracle for this slice.

## Source Of Truth

The request-route contract is defined by:

- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/subset7b_control_envelope_expected_v1.json`

Repo-local derived expected corpus for this slice:

- `fixtures/control/subset7c_control_request_route_expected_v1.json`

## Included

- accepted request-route classification for the shared request corpus
- typed extraction of the known request-parameter family:
  - `name`
  - `scope`
  - `expected_session_id`
  - `client_registration_id`
  - `queue_name`
  - `token`
  - `signal`
  - `target_pid`
  - `state`
  - `reuse_existing_binding`
- route summaries derived from the shared request corpus
- preservation of the bounded top-level rejection categories for malformed
  request packets

## Excluded

- response-route or response-result consumer logic
- packet encode parity
- dispatcher/service execution
- transport/session behavior
- per-method service validation beyond stable route/typed-parameter extraction
- broad unknown-key policy

## Required FX Behavior

For accepted request rows:

- `_FXCFControlRequestRouteInit` succeeds
- the route kind matches the expected request family
- typed known-parameter extraction matches the shared request corpus
- canonical route JSON and summary match the derived expected corpus

For rejected request rows:

- `_FXCFControlRequestRouteInit` fails
- top-level rejection category and payload remain aligned with the bounded 7A
  request surface

## Canonical Semantic Surface

The FX 7C probe emits one result row per shared request case with:

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
- `kind`: `request`
- `success`: whether FX matched the expected route behavior
- `primary_value_text`:
  - accepted row: canonical route JSON
  - rejected row: canonical top-level rejection JSON
- `alternate_value_text`:
  - accepted row: short route summary
  - rejected row: short rejection summary

## Dependencies

Subset 7C depends on:

- Subset 7A packet decode and rejection compatibility
- Subset 7B request envelope compatibility
- Subset 3A binary-plist read semantics

It does not depend on:

- packet encode parity
- response consumer work
- dispatcher execution
- MX validation

## Exit Gate

Subset 7C is accepted when:

- an FX request-route probe runs against the shared request corpus
- compare against `fixtures/control/subset7c_control_request_route_expected_v1.json`
  returns `0` blockers
- the shared handoff artifact `subset7c_control_request_route_fx.json` is
  published

## Stop Conditions

Stop and escalate if any of these happen:

- the shared request corpus stops carrying enough information to derive stable
  route kinds
- request-route work starts requiring response encode or dispatcher semantics
- a future consumer needs per-method semantics not represented in the shared
  request corpus
