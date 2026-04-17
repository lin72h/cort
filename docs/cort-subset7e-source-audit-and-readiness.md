# CORT Subset 7E Source Audit And Readiness

Date: 2026-04-17

Subset 7E is ready for bounded FX implementation and validation.

## Read Inputs

- `docs/cort-subset7d-control-response-profile-contract.md`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `../wip-swift-gpt54x/swift/Sources/LaunchXNotifyC/LaunchXNotifyC.swift`

## What This Slice Needs To Prove

Subset 7E should target the first real packet consumer after the generic
response-profile layer:

- the notify client’s success payload parsing
- the notify client’s failure-status mapping

The Swift notify client currently does three different kinds of work after it
gets a decoded control response:

1. pull remote registration fields out of wrapped or direct registration
   payloads
2. pull a few scalar success fields out of `check`, `get_state`, and
   `is_valid_token`
3. classify control failures into notify status families using code/message
   rules

That is the right next boundary. It is narrower than broad response consumers,
but it is more useful than staying at profile classification only.

## Source Findings

### `LaunchXNotifyC.swift`

Relevant packet-consumer surfaces:

- `requestRegistration` / `updateRegistration`
  - require remote registration fields:
    `token`, `session_id`, `scope`, `name`,
    `local_transport_binding_id`, `local_transport_handle`,
    `last_seen_generation`, `first_check_pending`
- `check`
  - require `changed`, `generation`, and a wrapped registration object
- `getState`
  - require integer `state`
- `isValidToken`
  - require boolean `valid`
- `status(from payload)` / `statusFromNotifyFailure`
  - map error `code` and `message` into notify status families

### Shared notify response corpus

The imported corpus already contains the rows needed for this slice:

- notify post success
- wrapped registration success
- direct registration success
- check success
- cancel success
- state success
- validity success
- representative notify failures:
  - invalid token
  - missing name
  - invalid scope
  - invalid signal
  - invalid client registration id
  - missing dispatch queue

The current corpus does not need MX. It already captures the packet-side
surface consumed by the Swift notify client.

## Deliberate Boundaries

Do not widen 7E into:

- generic control/diagnostics consumers
- list-names packet consumers
- request/response correlation
- fast-path snapshot logic
- socket binding attachment/reconnect behavior
- cached registration merge policy

Those belong above the packet-outcome layer.

## Implementation Readiness

Subset 7E is ready because:

- 7D already validates the broader notify response families
- the current notify client reads a smaller, stable subset of fields
- the error mapping logic is explicit in Swift and already represented by the
  shared failure corpus
- no new platform oracle is needed

## Expected FX Shape

The first FX cut should live inside `FXCFControlPacket` and add:

- a bounded internal notify outcome struct
- status-family mapping from control error payloads
- remote-registration extraction from response profiles
- canonical JSON and summary render helpers

It should not add a new public header or a transport/runtime API.

## Ready-To-Run Local Gate

After implementation, the local FX gate should be:

- `make -C cort-fx test`
- `make -C cort-fx compare-subset7e-with-expected`
- `make -C cort-fx publish-subset7e-artifact`
- `./tools/run_elixir.sh tools/workflow_selfcheck.exs`
