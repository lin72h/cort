# CORT Subset 7B Source Audit And Readiness

Date: 2026-04-17

This note records the source evidence for the internal control-envelope slice
that follows Subset 7A.

It answers a narrower question than the broad architecture report:

- what exact envelope surface does the next packet-facing FX cut need
- what Swift packet behavior matters for this proof
- what remains out of scope

## Read Inputs

- `docs/cort-subset7b-control-envelope-contract.md`
- `../wip-swift-gpt54x/swift/Sources/LaunchXControl/ControlProtocol.swift`
- `../wip-swift-gpt54x/swift/Tests/LaunchXControlTests/ControlProtocolTests.swift`
- `fixtures/control/bplist_packet_corpus_v1.source.json`
- `fixtures/control/bplist_packet_rejection_corpus_v1.source.json`
- `fixtures/control/subset7a_control_packet_expected_v1.json`

## Audit Conclusion

Subset 7B should stay on the decode side and package the already-proven packet
surface into stable internal request/response views.

Why:

- the Swift control protocol already defines a narrow top-level request and
  response envelope
- Subset 7A already proved decode and rejection compatibility for the shared
  corpus
- the next useful consumer cut should stop manually walking raw packet
  dictionaries
- no new CoreFoundation or MX discovery is needed for this slice

This means the first 7B cut can stay narrow:

- decode packet payload into an owned root
- surface borrowed request/response fields
- emit canonical envelope JSON and summaries
- compare against a derived expected corpus

Do not widen 7B into encode, transport, or dispatcher work.

## Relevant Swift Packet Surface

Directly relevant:

| Source | Behavior | Why It Matters | Classification | Proposed Action |
| --- | --- | --- | --- | --- |
| `ControlProtocol.swift` | request envelope requires `protocol_version`, `service`, `method`, `params` | defines the exact request accessor surface | `required-for-subset` | expose these as borrowed request fields |
| `ControlProtocol.swift` | response envelope requires `protocol_version`, `status`, and either `result` or `error` | defines the exact response accessor surface | `required-for-subset` | expose status plus borrowed result/error fields |
| `ControlProtocol.swift` | binary-plist payload check exists before decode | keeps packet validation bounded | `required-for-subset` | expose a small internal header-check helper |
| imported corpora | accepted request/response packets already freeze the envelope surface | removes the need for fresh packet discovery | `required-for-subset` | derive expected 7B summaries from the shared corpora |
| imported rejection corpus | rejection families remain `missing_key`, `unsupported_version`, `invalid_packet` | preserves the bounded failure surface | `required-for-subset` | reuse 7A rejection semantics unchanged |

Out of scope for 7B:

| Source | Behavior | Why It Is Out Of Scope | Classification | Proposed Action |
| --- | --- | --- | --- | --- |
| `ControlProtocol` encode helpers | packet write path | not needed for the first typed-envelope consumer cut | `defer` | revisit only if packet encode becomes a real consumer need |
| dispatcher/service execution | runtime behavior after decode | not envelope compatibility | `exclude` | keep 7B away from service semantics |
| session framing and descriptor transport | transport behavior | not packet-envelope semantics | `exclude` | do not widen into `FDS` or socket flow |
| broader malformed nested payload fuzzing | extra packet archaeology | not represented in the shared corpora | `defer` | widen only after the corpus widens |

## Key Readiness Observations

1. The semantic authority is still the Swift packet lane.

MX does not add value to this slice because the contract is LaunchX-owned and
already frozen in the shared corpora.

2. Subset 7A already proved the hard decode boundary.

7B should not re-open packet validity rules. It should package the proven
packet root into a cleaner internal surface.

3. The envelope slice is useful because it removes raw dictionary walking.

That is the next real consumer-facing improvement. It is more useful than
premature encode or transport work.

4. The slice should remain internal-only.

There is no reason to leak packet-specific types into public CoreFoundation
headers yet.

## Recommended First FX Cut

The first 7B implementation cut should be:

- internal request/response structs in `FXCFControlPacket`
- init/clear helpers that own the decoded packet root
- borrowed field access through those structs
- envelope JSON/summary helpers
- one FX probe that compares against the derived 7B expected corpus

That is the right next step because it tightens the packet lane without
dragging in encode or runtime service behavior.
