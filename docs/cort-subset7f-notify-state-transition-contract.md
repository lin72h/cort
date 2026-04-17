# CORT Subset 7F Notify State Transition Contract

Date: 2026-04-17

Subset 7F is the first local-state consumer slice on top of the proven Subset
7E notify outcome layer.

It stays decode-side, corpus-driven, and local-policy-only.

## Purpose

Subset 7F proves one narrow claim:

- FX can apply the bounded cached-registration transition rules used by the
  current Swift `LaunchXNotifyC` client
- FX can expose stable store / merge / keep / discard outcomes without raw
  dictionary walking
- FX can keep local notify cache policy separate from transport, fast-path, and
  daemon execution

The semantic authority for this slice is:

- `../wip-swift-gpt54x/swift/Sources/LaunchXNotifyC/LaunchXNotifyC.swift`
- `fixtures/control/subset7e_notify_client_outcome_expected_v1.json`
- `fixtures/control/subset7f_notify_state_transition_cases_v1.json`

MX is not the oracle for this slice.

## Included

- bounded cached-registration transition operations for:
  - `registration_store`
  - `registration_update`
  - `check`
  - `cancel`
  - `get_state`
  - `is_valid_token`
- cached-registration storage fields:
  - `token`
  - `session_id`
  - `scope`
  - `name`
  - `binding_id`
  - `last_seen_generation`
  - `first_check_pending`
- check merge semantics:
  - preserve cached `binding_id` when the remote registration omits it
  - `last_seen_generation = max(remote, cached, observed_generation)`
  - `first_check_pending = remote.first_check_pending && cached.first_check_pending`
  - `changed_result = remote_changed && (cached.first_check_pending || observed_generation > cached.last_seen_generation)`
- discard semantics for:
  - cancel success
  - `is_valid_token=false`
  - mapped `invalid_token` failures
- keep semantics for non-discarding success/error paths
- canonical transition JSON and summary compare against the derived corpus

## Excluded

- fast-path snapshot behavior
- live socket/binding ownership maps
- binding handle tracking after initial decode
- dispatcher execution
- notify state or generation fetch behavior beyond the local cache decision
- request issuance / retry / reconnection
- encode parity

## Required FX Behavior

For every included transition row:

- FX can derive a transition from the 7E notify outcome and optional cached
  registration input
- the transition action matches the derived corpus
- the resulting cached registration, if any, matches the derived corpus
- `changed_result`, `valid_result`, and `released_binding_id`, when present,
  match the derived corpus
- canonical transition JSON and summary match the derived corpus

## Canonical Semantic Surface

The FX 7F probe emits one row per included transition case with:

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
- `kind`: `notify_transition`
- `success`: whether FX matched the expected notify state transition
- `primary_value_text`: canonical transition JSON
- `alternate_value_text`: short transition summary

## Dependencies

Subset 7F depends on:

- Subset 7E notify outcome compatibility
- the shared Swift notify client source
- the shared transition scenario file

It does not depend on:

- MX validation
- fast-path snapshot files
- socket runtime behavior
- encode parity

## Exit Gate

Subset 7F is accepted when:

- an FX notify transition probe runs against the shared 7F scenario set
- compare against
  `fixtures/control/subset7f_notify_state_transition_expected_v1.json`
  returns `0` blockers
- the shared handoff artifact
  `subset7f_notify_state_transition_fx.json`
  is published

## Stop Conditions

Stop and escalate if:

- the next notify consumer slice needs live binding tables or fast-path state
  before this local transition layer is accepted
- the shared corpus is no longer sufficient to derive transition outcomes
  deterministically
- cache transition behavior starts depending on packet fields not present in the
  7E outcome surface
