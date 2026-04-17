# CORT Subset 7D Source Audit And Readiness

Date: 2026-04-17

## Readiness Verdict

Subset 7D is ready for bounded FX implementation and validation.

Reason:

- the shared Swift corpus carries stable response result families without MX
  work
- the profile work can stay decode-side
- the first consumer fields are bounded and disjoint enough to classify without
  dispatcher semantics

## Inputs Reviewed

- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/subset7b_control_envelope_expected_v1.json`
- `fixtures/control/subset7c_control_request_route_expected_v1.json`

## Response Families Found In The Shared Corpus

- error payload
- notify registration wrapper
- notify cancel
- notify direct registration state
- notify post summary
- notify name/state summary
- notify validity
- notify check summary
- notify name list
- control capabilities
- control health
- diagnostics snapshot
- generic scalar-coverage object

These top-level result signatures are stable enough for a bounded consumer cut.

## Extraction Decision

Do not build a dispatcher-aware response layer.

Build a typed response-profile layer inside `FXCFControlPacket` instead:

- classify accepted response envelopes by result shape
- expose only the bounded fields needed by the shared corpora
- keep raw nested dictionaries/arrays as borrowed internal references where that
  is sufficient

## Required File Set

- `cort-fx/src/FXCFControlPacket.c`
- `cort-fx/src/FXCFControlPacket.h`
- `cort-fx/tests/control_response_profile_tests.c`
- `cort-fx/tests/cort_subset7d_control_response_profile_fx.c`
- `cort-fx/tests/subset7d_control_response_profile_support.h`
- `tools/build_subset7d_control_response_profile_expected.exs`
- `tools/compare_subset7d_control_response_json.exs`
- `fixtures/control/subset7d_control_response_profile_expected_v1.json`

## Known Non-Goals

- no MX probe
- no new plist semantics
- no request/response correlation
- no encode work
- no response mutation or synthesis

## Acceptance Gate

The first implementation cut is ready when:

- `make -C cort-fx compare-subset7d-with-expected` returns `0` blockers
- `make -C cort-fx test` passes with the 7D probe/test lane included
- `subset7d_control_response_profile_fx.json` can be published as a shared
  repo-root handoff artifact
