# CORT Subset 7E Notify Client Outcome Contract

Date: 2026-04-17

Subset 7E is the first notify-client consumer slice on top of the proven
Subset 7D response-profile layer.

It stays decode-side and corpus-driven.

## Purpose

Subset 7E proves one narrow claim:

- FX can turn accepted notify response packets into stable notify-client outcome
  families without manual raw-dictionary walking
- FX can expose the exact bounded fields the current Swift `LaunchXNotifyC`
  client consumes first
- FX can map notify control failures into stable notify status families without
  transport or dispatcher execution

The semantic authority for this slice is:

- `../wip-swift-gpt54x/swift/Sources/LaunchXNotifyC/LaunchXNotifyC.swift`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- the already-proven Subset 7D response-profile surface

MX is not the oracle for this slice.

## Included

- notify response outcome classification for the shared notify response corpus,
  excluding `list_names`
- unified success outcome families for:
  - `notify.post`
  - `notify.registration`
  - `notify.check`
  - `notify.cancel`
  - `notify.name_state`
  - `notify.validity`
  - `error`
- notify-client status-family mapping for error payloads:
  - `ok`
  - `invalid_request`
  - `invalid_name`
  - `invalid_token`
  - `invalid_signal`
  - `invalid_file`
  - `failed`
- remote-registration extraction for the fields the current notify client stores:
  - `token`
  - `session_id`
  - `scope`
  - `name`
  - `binding_id`
  - `binding_handle`
  - `last_seen_generation`
  - `first_check_pending`
- canonical outcome JSON and summary compare against the derived corpus

## Excluded

- `notify.list_names`
- control / diagnostics / generic-object response consumers
- request/response correlation
- fast-path snapshot behavior
- local socket attachment or file-descriptor reuse behavior
- cached-registration merge policy
- transport/session behavior beyond the decoded payload surface
- encode parity

## Required FX Behavior

For every included notify response row:

- `_FXCFControlNotifyOutcomeInit` succeeds
- the outcome kind matches the derived corpus
- the notify status family matches the derived corpus
- bounded extracted fields match the derived corpus
- canonical outcome JSON and summary match the derived corpus

This slice does not widen Subset 7A rejection semantics or Subset 7D response
classification semantics.

## Canonical Semantic Surface

The FX 7E probe emits one row per included notify response case with:

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
- `success`: whether FX matched the expected notify-client outcome
- `primary_value_text`: canonical notify outcome JSON
- `alternate_value_text`: short notify outcome summary

## Dependencies

Subset 7E depends on:

- Subset 7A packet decode compatibility
- Subset 7B response envelope compatibility
- Subset 7D response-profile compatibility
- the shared Swift packet corpus

It does not depend on:

- MX validation
- dispatcher execution
- fast-path snapshot behavior
- socket/file-descriptor runtime behavior

## Exit Gate

Subset 7E is accepted when:

- an FX notify-client outcome probe runs against the shared notify response
  corpus
- compare against
  `fixtures/control/subset7e_notify_client_outcome_expected_v1.json`
  returns `0` blockers
- the shared handoff artifact
  `subset7e_notify_client_outcome_fx.json`
  is published

## Stop Conditions

Stop and escalate if:

- notify-client success paths start depending on request correlation or local
  runtime state not present in the shared corpus
- the shared corpus stops being sufficient to derive notify-client status
  families deterministically
- notify-client behavior starts depending on live transport attachment or
  fast-path snapshot state before the packet outcome layer is accepted
