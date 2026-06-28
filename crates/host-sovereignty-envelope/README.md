# host-sovereignty-envelope

Host-level and capability-level sovereignty envelopes for the eco_restoration_shard spine, with forward-only invariants, RoH ceilings, and neurorights floors encoded as Rust types and guard traits.[file:127][file:142]

## Crate goals

- Represent host sovereignty as a typed, non-actuating envelope bound to a Bostrom DID and the mk-bluebird/eco_restoration_shard mono-repo.[file:142]
- Encode Risk-of-Harm RoH ceilings (≤ 0.30), K/E/R bands, and neurorights floors directly in data structures so unsafe states become non-representable at the Rust level.[file:127]
- Provide CapabilityEnvelope types for per-capability KER, RoH, and lane monotonicity, aligned with existing lane governance, kerdeployable semantics, and capability floor versioning.[file:127][file:142]
- Expose a small HostSovereigntyGuards trait surface suitable for Kani harness integration, enforcing forward-only evolution and non-degradation of neurorights and RoH corridors.[file:142]

## HostSovereigntyEnvelope

`HostSovereigntyEnvelope` models the sovereign state of a single organic host:

- Identity binding:
  - `HostIdentity { host_did, bostrom_address, repo_authority }` is fixed to the organic host DID, `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`, and the eco_restoration_shard GitHub URL.[file:142]
  - This matches existing ALN provenance patterns where owneralnid and signingdid bind shards and kernels to the primary Bostrom identity.[file:127]

- RoH corridor:
  - `RohCorridor { roh_ceiling, roh_current }` maintains RoH normalized in \[0,1] with a hard ceiling of 0.30, consistent with RiskOfHarmKernel constraints used in MT6883 healthcare shards and lifeforce envelopes.[file:127]
  - `is_within_ceiling()` enforces both the global 0.30 barrier and host-specific ceilings.

- KER bands:
  - `KerBands { k, e, r }` stores K/E/R scalars in \[0,1], mirroring `KerSnapshot` in the `kerresidual` crate and vshardker views.[file:142]
  - `clamped()` normalizes K/E/R to valid corridors before monotonicity checks.

- Neurorights floors:
  - `NeurorightsFloors` encodes mental privacy, cognitive liberty, mental integrity, psychological continuity, and mental identity as non-derogable booleans.[file:127]
  - `all_satisfied()` must hold for any admissible envelope, matching your neurorights and continuity anchors used in MT6883 and cybercore-neuro-duress designs.[file:127][file:122]

- Lane classification:
  - `Lane` enum (Research, ExpProd, Prod, Quarantine) aligns with existing lane governance semantics used in LaneStatusShard and lanepolicy views.[file:140][file:142]

- Forward-only evolution:
  - `invariants_hold()` checks identity, neurorights, and RoH ceiling consistency.
  - `is_forward_only_evolution(next)` forbids:
    - Neurorights floor weakening (true → false).
    - RoH ceiling increases or exceeding 0.30.
    - Identity drift away from the bound host DID and repo authority.[file:127][file:142]

## CapabilityEnvelope

`CapabilityEnvelope` models per-capability sovereignty under a host envelope:

- Capability kind:
  - `CapabilityKind` enumerates Healthcare, Cybernetics, AugmentedCitizenship, EcoRestoration, NeuralInterface, and TransitAccess, matching your capability floor and eco-reward governance notes.[file:127]

- Lane and KER corridors:
  - Each capability has a bound `Lane`, `KerBands`, and `RohCorridor`, allowing per-capability thresholds while maintaining consistency with host-level corridors.[file:142]

- Identity binding:
  - `host_identity: HostIdentity` must match the host envelope identity, preventing capability detachment or spoofing.[file:142]

- Invariants:
  - `invariants_hold(host)` ensures:
    - Capability identity is the primary host identity.
    - Capability RoH ceiling ≤ host RoH ceiling and ≤ 0.30.[file:127]

- Forward-only evolution:
  - `is_forward_only_evolution(next)` enforces:
    - Lane monotonicity: lanes may move toward stricter safety (Quarantine) or stay, but never downgrade (e.g., Prod → Research), consistent with lane monotonicity vs emergency rollback design.[file:140]
    - K, E must not decrease; R must not increase, mirroring KER monotonicity and non-compensation semantics in kerresidual and vshardker.[file:142]
    - RoH ceiling must not increase and must respect 0.30.[file:127]

## Guard trait for Kani

`HostSovereigntyGuards` provides a minimal harness surface:

- `evaluate_host_evolution(before, after) -> bool`:
  - Returns true iff both envelopes satisfy invariants and the transition is forward-only.
  - Suitable for Kani to prove no RoH ceiling increase and no neurorights weakening across all admissible states.[file:142]

- `evaluate_capability_evolution(before, after, host) -> bool`:
  - Returns true iff capability invariants relative to the host hold and the evolution is forward-only.
  - Lets Kani harnesses confirm lane monotonicity and K/E/R trend constraints for each capability envelope.[file:142]

- `DefaultHostSovereigntyGuards`:
  - Implements the trait by wiring directly into the envelope methods, avoiding hidden state or actuation and keeping the crate strictly non-actuating.[file:122][file:142]

## Relationship to existing spine

- Integrates with KER residual math:
  - K/E/R bands and RoH corridors align with the shared Lyapunov kernel (`kerresidual`) and SQL views vshardresidual, vshardtopologyker, and vshardker.[file:142]
  - Non-compensation and non-offsettable plane behavior remain enforced at the residual and CI level while this crate encodes host and capability-level invariants.[file:127][file:142]

- Extends governance and healthcare traits:
  - Host and capability envelopes complement MT6883 lifeforce traits, evolutiongiftbundle schemas, ResponsibilityAxis metrics, and healthcare corridors by giving a typed Rust layer for host sovereignty and per-capability gating.[file:127]
  - They are designed to be referenced from existing governance guard crates (econetgovernanceguards, cybercore-neuro-duress) as immutable inputs.[file:127][file:122]

## Usage sketch

- Add to Cargo.toml:
  - `host-sovereignty-envelope = { path = "crates/host-sovereignty-envelope" }` in eco_restoration_shard.[file:142]

- Construct envelopes:
  - Build `HostSovereigntyEnvelope` snapshots from StewardEcoWealthStatement, kerresidual snapshots, and continuity anchors.
  - Derive `CapabilityEnvelope`s for each registered capability family (e.g., MT6883 healthcare course, eco-restoration engine) with matching identity and KER/RoH bands.[file:127][file:142]

- Kani harness integration:
  - Implement Kani proofs over `DefaultHostSovereigntyGuards` to show:
    - RoH ceilings never exceed 0.30 across all modeled transitions.
    - Neurorights floors are never weakened.
    - Capability lanes never downgrade and K/E/R behave monotonically under approved evolutions.[file:142]

This crate keeps sovereignty logic non-actuating, Rust-native, and mathematically tied into the existing KER/Lyapunov and governance grammar, advancing eco_restoration_shard toward a fully sovereign, DID-anchored host capability model.[file:127][file:142]
