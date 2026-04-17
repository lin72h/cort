# CORT Subset 7A Source Audit And Readiness

Date: 2026-04-17

This note records the source evidence for the first packet-facing CORT slice.

It answers a narrower question than the broad architecture report:

- what exact current packet contract does Subset 7A need to match
- what parts of the Swift packet lane matter for that proof
- what parts are explicitly out of scope

## Read Inputs

- `docs/cort-subset7a-control-packet-contract.md`
- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `../wip-swift-gpt54x/swift/Tests/LaunchXControlTests/ControlProtocolTests.swift`
- `../wip-swift-gpt54x/docs/swift-control-bplist-fixture-corpus.md`
- `../wip-swift-gpt54x/docs/swift-control-bplist-rejection-corpus.md`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`

## Audit Conclusion

Subset 7A should target packet decode compatibility against the shared Swift
corpora, not a fresh re-reading of arbitrary LaunchX service code.

Why:

- `ControlProtocol.swift` defines a narrow packet envelope on top of binary
  plist
- the imported source corpora already freeze the bounded accepted/rejected
  packet surface
- `ControlProtocolTests` already treat those corpora as the stable packet
  contract
- the proven CORT 3A bplist layer already covers the value family used by the
  packet contract

This means the first packet slice can stay narrow:

- decode accepted corpus packets
- reject malformed corpus packets
- emit semantic summaries
- compare against the derived expected corpus

Do not start with packet encode parity or dispatcher integration.

## Relevant Swift Packet Surface

Directly relevant:

| Source | Behavior | Why It Matters | Classification | Proposed Action |
| --- | --- | --- | --- | --- |
| `ControlProtocol.swift` | `ControlValue` supports object, array, string, integer, double, bool, date, data | defines the exact value family for packet payloads | `required-for-subset` | keep 7A on this value family only |
| `ControlProtocol.swift` | request envelope requires `protocol_version`, `service`, `method`, `params` | defines accepted request shape and missing-key failures | `required-for-subset` | mirror this exact top-level envelope |
| `ControlProtocol.swift` | response envelope requires `protocol_version`, `status`, and either `result` or structured `error` | defines accepted response shape and missing-key failures | `required-for-subset` | mirror this exact top-level envelope |
| `ControlProtocol.swift` | `currentVersion = 1` | defines current protocol version gate | `required-for-subset` | treat version mismatch as explicit rejection case |
| `ControlProtocol.swift` | `invalidPacket("root property list must be a dictionary")` | defines root-not-dictionary rejection boundary | `required-for-subset` | keep exact rejection family in corpus-driven compare |
| `ControlProtocol.swift` | top-level `ControlProtocolError` = `invalidPacket`, `missingKey`, `unsupportedVersion`, `unsupportedValueType` | defines bounded rejection taxonomy | `required-for-subset` | Subset 7A freezes only the first three families represented in the imported rejection corpus |
| `ControlProtocolTests.swift` | imported accepted corpus must decode through current protocol | proves the accepted-corpus authority | `required-for-subset` | treat imported accepted corpus as packet decode contract |
| `ControlProtocolTests.swift` | imported rejection corpus must fail with exact expected top-level error | proves the rejected-corpus authority | `required-for-subset` | treat imported rejection corpus as packet rejection contract |

Out of scope for 7A:

| Source | Behavior | Why It Is Out Of Scope | Classification | Proposed Action |
| --- | --- | --- | --- | --- |
| `ControlDispatcher` and service handlers in `ControlProtocol.swift` | service execution after decode | not packet decode compatibility | `exclude` | do not couple 7A to runtime service semantics |
| seqpacket socket tests | transport/session behavior | packet bytes are already frozen in corpora | `exclude` | do not widen into `FDS` or socket integration |
| packet encode helpers | Swift-side packet encoding | useful later, but not needed for first packet decode proof | `defer` | revisit only after decode compatibility is green |
| unsupported nested-type fuzzing | broad malformed payload exploration | not part of imported bounded rejection corpus | `defer` | future corpus expansion only if needed |

## Key Readiness Observations

1. MX is not the oracle for this slice.

The packet contract is defined by LaunchX Swift code plus its imported corpora.
CoreFoundation semantics are already carried by the earlier CORT subsets.

2. The current packet lane already froze the bounded accepted/rejected corpus.

That means Subset 7A does not need a new packet-discovery phase. It needs a
clean compare surface and a clean future FX probe target.

3. The current value family fits the proven 3A surface.

The packet lane uses only:

- dictionary
- array
- string
- integer
- double
- bool
- date
- data

That is exactly the 3A binary-plist value family.

4. The top-level rejection taxonomy is intentionally narrow.

The imported rejection corpus freezes only:

- `missing_key`
- `unsupported_version`
- `invalid_packet`

Do not widen 7A into general nested-field type validation unless the shared
Swift corpus is widened first.

## Recommended First FX Cut After Readiness

When implementation starts, the first FX packet cut should be:

- a decode-only packet adapter on top of the existing 3A plist reader
- one FX probe that reads the imported corpora and emits the 7A result JSON
- no transport integration
- no packet encode path yet
- no Swift bridge replacement

That keeps the first packet proof aligned with the actual consumer contract and
prevents scope drift into service semantics or socket behavior.
