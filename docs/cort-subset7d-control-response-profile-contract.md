# CORT Subset 7D Control Response Profile Contract

Date: 2026-04-17

Subset 7D is the response-side consumer slice that follows Subset 7C.

It stays decode-side and corpus-driven.

## Purpose

Subset 7D proves one narrow claim:

- FX can classify accepted response envelopes into stable typed result families
- FX can expose the bounded response fields that packet-facing consumers need
  first
- FX does not need dispatcher/service execution to do that work

The semantic authority for this slice is the shared Swift packet corpus.

MX is not the oracle for this slice.

## Source Of Truth

- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/subset7c_control_request_route_expected_v1.json`

Repo-local derived expected corpus:

- `fixtures/control/subset7d_control_response_profile_expected_v1.json`

## Included

- accepted response-profile classification for the shared response corpus
- typed success/error profile extraction for:
  - `error`
  - `notify.post`
  - `notify.registration_wrapper`
  - `notify.cancel`
  - `notify.registration`
  - `notify.name_state`
  - `notify.validity`
  - `notify.check`
  - `notify.name_list`
  - `control.capabilities`
  - `control.health`
  - `diagnostics.snapshot`
  - `generic.object`
- canonical response-profile JSON and summary compare against the derived corpus

## Excluded

- packet encode parity
- dispatcher/service execution
- response-to-request correlation
- transport/session behavior
- broad job-response consumer work
- ad hoc traversal of every nested result field

## Required FX Behavior

For accepted response rows:

- `_FXCFControlResponseProfileInit` succeeds
- the profile kind matches the shared corpus
- the bounded extracted fields match the derived expected corpus
- canonical profile JSON and summary match the derived expected corpus

This slice does not expand the 7A/7B rejection surface.

Malformed response-envelope rejection remains owned by Subset 7A/7B.

## Canonical Semantic Surface

The FX 7D probe emits one row per accepted shared response case with:

- `name`
- `classification`
- `validation_mode`
- `kind`
- `success`
- `primary_value_text`
- `alternate_value_text`

Required meanings:

- `classification`: `required`
- `validation_mode`: `accept`
- `kind`: `response`
- `success`: whether FX matched the expected response profile
- `primary_value_text`: canonical response-profile JSON
- `alternate_value_text`: short response-profile summary

## Dependencies

Subset 7D depends on:

- Subset 7A packet decode compatibility
- Subset 7B response envelope compatibility
- Subset 3A binary-plist read semantics

It does not depend on:

- encode parity
- MX validation
- dispatcher execution

## Exit Gate

Subset 7D is accepted when:

- an FX response-profile probe runs against the shared response corpus
- compare against `fixtures/control/subset7d_control_response_profile_expected_v1.json`
  returns `0` blockers
- the shared handoff artifact `subset7d_control_response_profile_fx.json` is
  published

## Stop Conditions

Stop and escalate if:

- response result families stop being derivable from the shared corpus without
  dispatcher context
- response-side consumer work starts depending on request correlation or service
  execution
- packet consumers need encode parity before this decode-side profile layer is
  accepted
