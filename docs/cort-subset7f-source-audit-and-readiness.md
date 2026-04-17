# CORT Subset 7F Source Audit And Readiness

Date: 2026-04-17

## Read First

- `docs/cort-subset7e-control-notify-outcome-contract.md`
- `docs/cort-subset7e-source-audit-and-readiness.md`
- `docs/cort-subset7e-validation-workflow.md`
- `../wip-swift-gpt54x/swift/Sources/LaunchXNotifyC/LaunchXNotifyC.swift`
- `fixtures/control/subset7e_notify_client_outcome_expected_v1.json`
- `fixtures/control/subset7f_notify_state_transition_cases_v1.json`

## Why 7F Exists

Subset 7E proved payload extraction and notify status-family mapping.

The next real consumer gap is the local state policy that Swift applies after
decode:

- `store(_ remote:)`
- `merge(_:withCached:observedGeneration:)`
- `discardRegistration(...)`
- `check(token:output:)`
- `cancel(token:)`
- `updateRegistration(method:token:)`
- `getState(token:output:)`
- `isValidToken(_:)`

That policy is independent from transport execution, but it is not the same
thing as raw packet decoding. It deserves its own cut.

## Source Findings

### `LaunchXNotifyC.swift`

The current notify client stores only:

- `token`
- `sessionID`
- `scope`
- `name`
- `bindingID`
- `lastSeenGeneration`
- `firstCheckPending`

It does not keep `bindingHandle` in the cached registration.

The key transition rules are:

- direct registration/update success stores the remote registration
- check success merges remote + cached state
- cancel success discards cached registration
- `is_valid_token=false` discards cached registration
- mapped `invalid_token` failures discard cached registration
- non-`invalid_token` failures keep cached registration

### Shared 7E Outcome Corpus

The 7E expected corpus already carries the normalized notify outcome families
needed for 7F:

- `notify.registration`
- `notify.check`
- `notify.cancel`
- `notify.name_state`
- `notify.validity`
- `error`

That means 7F can stay above the 7E surface instead of re-parsing raw packet
objects.

## FX Readiness Decision

7F should be implemented as:

- a bounded internal cached-registration struct
- a bounded internal transition struct
- a single transition initializer that takes:
  - a 7E notify outcome
  - an operation enum
  - an optional cached registration
- canonical JSON/summary renderers for compare/reporting

7F should not introduce:

- socket descriptors
- binding maps
- fast-path snapshot dependencies
- request/response correlation

## First FX Cut

The first useful 7F cut is:

- transition operation enum
- cached-registration copy/clear helpers
- store / merge / keep / discard transition logic
- expected corpus derived from:
  - 7E expected outcomes
  - a repo-local scenario file
- C tests and an FX probe against that derived corpus

## Acceptance Gate

Before downstream notify consumers depend on 7F:

- `make -C cort-fx compare-subset7f-with-expected` must return `0` blockers
- `subset7f_notify_state_transition_fx.json` must be published
- `./tools/run_elixir.sh tools/workflow_selfcheck.exs` must pass
