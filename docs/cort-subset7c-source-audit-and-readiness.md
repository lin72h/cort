# CORT Subset 7C Source Audit And Readiness

Date: 2026-04-17

This note records the source evidence for the first request-consumer slice on
top of the proven packet envelope lane.

## Read Inputs

- `docs/cort-subset7c-control-request-route-contract.md`
- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/subset7b_control_envelope_expected_v1.json`

## Audit Conclusion

Subset 7C should stay request-only and classify the shared request corpus into
stable route kinds plus a small typed parameter family.

Why:

- the next useful consumer step after 7B is request routing, not response
  handling
- the shared request corpus already freezes the accepted service/method pairs
- the known request parameter family is small and stable across the current
  corpus
- this improves future consumer clarity without dragging in dispatcher logic

This means the first 7C cut can stay narrow:

- classify request service/method into route kinds
- surface the known parameter family through typed fields
- keep unknown service/method handling conservative and local
- preserve the existing top-level rejection surface

Do not widen 7C into response handling, encode work, or service execution.

## Relevant Request Surface

Directly relevant:

| Source | Behavior | Why It Matters | Classification | Proposed Action |
| --- | --- | --- | --- | --- |
| shared request corpus | accepted request service/method pairs are frozen | defines stable route-kind inventory | `required-for-subset` | classify only the accepted shared request routes |
| shared request corpus | known request parameter keys are small and reused | makes a typed parameter view practical | `required-for-subset` | extract only the bounded known parameter family |
| `ControlProtocol.swift` | top-level request envelope is already proven by 7B | removes need to reopen envelope work | `required-for-subset` | build 7C strictly on top of 7B |
| shared rejection corpus | malformed request packets already freeze top-level rejection categories | preserves failure semantics | `required-for-subset` | reuse the 7A request rejection surface unchanged |

Out of scope for 7C:

| Source | Behavior | Why It Is Out Of Scope | Classification | Proposed Action |
| --- | --- | --- | --- | --- |
| service handlers | actual runtime behavior after route selection | not request-route compatibility | `exclude` | do not couple 7C to service execution |
| response packet handling | result-side consumer logic | separate next slice | `defer` | keep 7C request-only |
| encode helpers | packet write path | not needed for the first consumer cut | `defer` | revisit only if a response consumer requires it |

## Key Readiness Observations

1. The request route inventory is already frozen in the shared corpus.

There is no need for a new discovery phase.

2. Known request parameters can be extracted without crossing into dispatcher work.

Typed fields such as `token`, `scope`, and `target_pid` are still packet-level
concerns. They are not service execution.

3. Request-only is the right cut.

It is smaller, directly useful, and avoids mixing request routing with
response/result semantics.
