# sacrifice-zone-envelope

`crates/sacrifice-zone-envelope` is a pure, non‑actuating Rust 2024 crate that mirrors the `specs/aln/SacrificeZoneSpec.v1.aln` shard. It provides type‑safe envelopes and guard predicates for Sacrifice Zones in Prometheus‑Praxis and eco_restoration_shard.

A **Sacrifice Zone** is a structurally uninhabitable, heavily contaminated region (e.g. nuclear exclusion rubble, sealed toxic dumps, cyboquatic toxic corridors) where nanoswarm and cyboquatic routing must obey strict KER, RoH, and biosphere invariants.

This crate:

- Encodes Sacrifice Zone envelopes as Rust structs.
- Enforces structural invariants at construction time.
- Exposes pure guard functions for KER and RoH checks.
- Provides eligibility and monotone‑safety predicates that can be wired into governance, nanoswarm planners, and observability layers.

It is designed for:

- `eco_restoration_shard` governance and guard crates.
- Kani verification harnesses.
- Read‑only AI‑chat tooling that needs deterministic, machine‑checkable safety predicates.

## Design goals

- **Pure logic only**  
  No IO, networking, or hardware access. All functions are side‑effect free and deterministic.

- **ALNv2 mirror**  
  The `SacrificeZoneEnvelope` struct mirrors rows from `specs/aln/SacrificeZoneSpec.v1.aln`. Any loader that parses ALN must pass through `SacrificeZoneEnvelope::new`, ensuring invariants are checked at the boundary.

- **KER / RoH safety**  
  Guard methods make it easy to:
  - Confirm KER snapshots stay within zone‑specific thresholds.
  - Enforce a hard global RoH ceiling (`RoH <= 0.30`).
  - Prove monotone safety across envelope upgrades.

- **Biosphere and pollinator hard‑stops**  
  The crate includes `lifeless_classification` and `eligible_for_nanoswarm` predicates to ensure nanoswarm planning cannot treat zones with active pollinators or wildlife as lifeless.

- **Kani‑ready**  
  The module structure and pure functions are suitable for Kani harnesses that prove:
  - KER and RoH guards never allow out‑of‑envelope states.
  - Monotone upgrades never relax safety or reduce contamination metrics without explicit evidence.

## Types and APIs

### Core enums

- `ExclusionStatus`
  - `None`
  - `HumanExclusion`
  - `IndustrialExclusion`
  - `FullExclusion`

- `PrimaryClass`
  - `Radiation`
  - `HeavyMetals`
  - `PersistentOrganics`
  - `Hydrocarbons`
  - `Microplastics`
  - `AirPollution`
  - `MultiModal`

- `ActivityLevel`
  - `NoneDetected`
  - `Sparse`
  - `Moderate`
  - `High`

- `Lane`
  - `Research`
  - `Pilot`
  - `Production`
  - `CityCritical`

### Snapshots

- `KerSnapshot`
  - Fields: `k`, `e`, `r` (all `f32`, normalized 0..1).
- `RohSnapshot`
  - Field: `roh` (`f32`, normalized 0..1).

These are lightweight mirrors for governance and observability planes that track Knowledge, EcoImpact, Risk, and RoH values for actions referencing a Sacrifice Zone.

### SacrificeZoneEnvelope

```rust
pub struct SacrificeZoneEnvelope {
    pub zone_id: String,
    pub region_id: String,
    pub authority_id: String,
    pub geometry_ref: String,
    pub exclusion_status: ExclusionStatus,
    pub evidence_bundle_id: String,
    pub proof_hash_hex: String,
    pub primary_class: PrimaryClass,
    pub secondary_classes_json: String,
    pub contamination_radiation: f32,
    pub contamination_heavy_metals: f32,
    pub contamination_organics: f32,
    pub contamination_microplastics: f32,
    pub contamination_air: f32,
    pub contamination_other: f32,
    pub pollinator_activity: ActivityLevel,
    pub wildlife_activity: ActivityLevel,
    pub biosignal_proof_id: String,
    pub lane: Lane,
    pub roh_ceiling: f32,
    pub kmin: f32,
    pub emin: f32,
    pub rmax: f32,
    pub neurorights_envelope_id: String,
    pub sovereignty_tags_json: String,
    pub notes: String,
}
```

#### Constructor

- `SacrificeZoneEnvelope::new(...) -> Result<Self, String>`

Enforces:

- Non‑empty `zone_id`, `geometry_ref`, `proof_hash_hex`.
- `roh_ceiling ∈ [0, RoH_global_max]` with `RoH_global_max = 0.30`.
- `kmin, emin ∈ [0,1]`.
- `rmax ∈ [0, roh_ceiling]`.
- All contamination indices in `[0,1]`.

Any violation yields an error string, making misconfigurations fail fast at initialization.

#### Guards

- `fn ker_within(&self, ker: KerSnapshot) -> bool`
  - `k >= kmin`, `e >= emin`, `r <= rmax`.

- `fn roh_within(&self, roh: RohSnapshot) -> bool`
  - `roh <= roh_ceiling` and `roh <= ROH_GLOBAL_MAX`.

- `fn lifeless_classification(&self) -> bool`
  - `exclusion_status` is at least `HumanExclusion`.
  - `pollinator_activity == NoneDetected`.
  - `wildlife_activity == NoneDetected`.

- `fn eligible_for_nanoswarm(&self) -> bool`
  - `lifeless_classification()` is `true`.
  - `lane` is `Research` or `Pilot`.

These functions deliberately **do not actuate anything**; they simply encode safety logic that other crates can compose.

#### Monotone upgrades

- `fn monotone_upgrade(&self, prev: &Self) -> bool`

Checks:

- Same `zone_id`.
- `roh_ceiling_new <= roh_ceiling_old`.
- `rmax_new <= rmax_old`.
- `kmin_new >= kmin_old`, `emin_new >= emin_old`.
- All contamination indices `>=` previous values (no silent “washing”).

Sovereignty and neurorights flags are treated as protection bits; additional enforcement is expected in higher‑level governance kernels that parse `sovereignty_tags_json` and `neurorights_envelope_id`.

## Integration patterns

### Governance guards

- Governance and rights‑risk kernels can call:
  - `ker_within` and `roh_within` before approving macro actions.
  - `eligible_for_nanoswarm` to filter candidate zones for nanoswarm mission planning.
  - `monotone_upgrade` in migration validators to ensure ALN shard updates are strictly safer or equal.

### Observability

- Observability and metrics crates can:
  - Track which zones are currently `eligible_for_nanoswarm`.
  - Export booleans and indices as Prometheus metrics for dashboards.
  - Provide explainable surfaces for AI‑chat tools without granting actuation control.

### Kani verification

- Kani harnesses can:
  - Prove that any accepted `KerSnapshot` / `RohSnapshot` combination respects envelope bounds.
  - Prove monotone upgrade properties across automatically generated ALN migrations.

## Versioning and safety

- Rust edition: 2024
- `rust-version = "1.85"`
- `#![forbid(unsafe_code)]`
- No hidden control panels or actuation surfaces: this crate is strictly a logic layer, designed to be used under Cybercore and eco_restoration_shard governance constraints.
