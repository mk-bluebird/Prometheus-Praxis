# cyboquatic-ecosafety

Cyboquatic ecosafety core for Phoenix-class nodes, aligned with the 2026 rx/Vt/KER grammar and EcoNet / ALNv2 corridors.[web:149]

This crate provides a non-actuating ecosafety kernel for canal, sewer, and FOG-routing workloads, with explicit Lyapunov residuals, KER windows, and governance-aware risk planes. It is designed to be embedded in the Prometheus-Praxis mono-repo and to stay under ALN and Bostrom DID control.

---

## Goals

- Enforce ecosafety invariants for cyboquatic nodes (canals, laterals, sewers) using normalized risk coordinates and Lyapunov residuals.[file:8]
- Provide KER-aware windows (`K`, `E`, `R`) that quantify knowledge-factor, eco-impact, and risk-of-harm over rolling horizons.[file:8]
- Expose non-actuating diagnostics for routing, lane assignment, and biodegradable substrate decisions, without ever directly touching hardware.[file:8]
- Bind ecosafety behavior to ALNv2 shard schemas and Bostrom / ALN identities so governance can be cryptographically audited.[file:1]

---

## Key concepts

- **RiskCoord**: Normalized scalar in \([0, 1]\) representing a single ecosafety coordinate (e.g., surcharge, saturation, biodiversity).[file:8]
- **RiskVector**: Aggregated risk coordinates across planes (CEC, saturation, surcharge, biodiversity, Vt, governance).[file:8]
- **LyapunovResidual**: Scalar \(V_t = \sum_j w_j r_j^2\) used as the ecosafety channel; control is admissible only when \(V_{t+1} \le V_t\) outside a small safe interior.[file:8]
- **KERWindow**: Rolling window over \(K\), \(E\), \(R\) for a node, tracking deployability via `ker_deployable()`.[file:1]
- **FogGuard**: Non-actuating guard that decides `Allow` / `Stop` for FOG routing and sewer actuation proposals based on KER and residuals.[file:8]
- **ALNv2 Shards**: Schema-bound particles that mirror SQL rows and Rust structs, binding evidence hexes, KER windows, and governance invariants.[file:1]

---

## Directory layout

- `crates/cyboquatic-ecosafety/src/lib.rs`  
  Crate root, re-exports core types and helpers and wires ecosafety primitives together.[file:8]

- Core math and frames:
  - `frame.rs` — Frame traits, composite frame, and frame context for ecosafety diagnostics.[file:8]
  - `window.rs` — `EcosafetyStatus`, trend enums, and `WindowManager` for KER history and status.[file:8]
  - `lyapunov_regime.rs` — Lyapunov stability diagnostics, residual history, and Vt regime analysis.[file:8]
  - `covariance.rs` — Covariance-based ecosafety frame and Lyapunov distance over risk vectors.[file:8]
  - `risk.rs` — `RiskCoord`, `RiskVector`, `LyapunovResidual`, `LyapunovWeights`, and `KERWindow` representation.[file:8]

- Governance and provenance:
  - `aln_schema.rs` — Embedded ALNv2 ecosafety envelope schema parsing and shard field definitions.[file:1]
  - `shard_schema.rs` — SQL/ALN shard schema model mapping particles to table layouts.[file:1]
  - `shard_update_validator.rs` — Shard update validator for ecosafety shards, enforcing corridor and Lyapunov invariants.[file:1]
  - `provenance.rs`, `provenancedetail.rs`, `provenancerecord.rs`, `provenanceexport.rs` — Provenance tracking, detail records, and CSV export helpers.[file:1]
  - `governance_checker.rs` — Governance checker tagging shard updates with sovereignty and consent hints.[file:1]

- Ecosafety pipelines and nodes:
  - `pipeline3.rs` — Three-stage pipeline (Integrity → Covariance → Biodiversity) with provenance outputs.[file:8]
  - `node.rs` — Node-level ecosafety envelope and integration with KER and risk vectors.[file:8]
  - `aggregators.rs` — Aggregation primitives for ecosafety risk across nodes and corridors.[file:8]

- Fog guard and safety shields:
  - `fog_guard.rs` — Fog guard configs, bands, thresholds, and verdicts for routing and actuation gating.[file:8]
  - `fog_guard_kani.rs` — Kani harnesses / models for FogGuard behavior under KER and residual constraints.[file:8]
  - `safe_flag.rs`, `safe_flag_kani.rs` — SAFE_FLAG governance signal model and Kani verification.[file:8]
  - `safety_shield.rs`, `safety_shield_kani.rs` — Safety shield logic and verification for ecosafety envelopes.[file:8]

- Integrity and error handling:
  - `integrity.rs` — Integrity diagnostics and frames for adversarial or malformed inputs.[file:8]
  - `integrityframe.rs` — Integrity frame wiring into the ecosafety pipeline.[file:8]
  - `error.rs` — Common error types for ecosafety operations.[file:8]
  - `frame_timeout.rs` — Timeout handling for ecosafety frames and pipeline stages.[file:8]

- Shards and stores:
  - `shard.rs` — Shard representation for ecosafety data.[file:1]
  - `shard_store.rs` — Storage primitives for ecosafety shards.[file:1]

- Biodiversity and mesocosm:
  - `biodiversity_mesocosm.rs` — Mesocosm diagnostics, mesocosm risk frame, and shard row types for biodiversity experiments.[file:8]
  - `canal_risk_plane.rs` — Canal biodiversity metrics and canal risk-plane mapping (H→r_biodiversity, BOD/TSS/thermal risk, sensitivities).[file:8]

- Privacy and JNI:
  - `privacy.rs` — Additive-sharing and differential-privacy primitives for ecosafety statistics aggregation.[file:8]
  - `jni_bindings.rs` — JNI bindings for host integration where needed.[file:8]

- Misc:
  - `ecosafetycovarianceframe.rs` — Additional covariance frame wiring.[file:8]
  - `types.rs` — Schema-bound ecosystem types mirroring ALN SQL records.[file:8]
  - `config.rs` — Ecosafety configuration types.[file:8]
  - `fog_guard.rs` family — see above.[file:8]
  - `node.rs` family — see above.[file:8]

---

## Embedded ALN spec

The crate embeds the ecosafety envelope ALN specification:

```rust
pub const ECOSAFETY_ALN_SPEC: &str =
    include_str!("../specs/CyboquaticEcosafetyEnvelopePhoenix2026v1.aln");
```

This string must match the ALN file in the Prometheus-Praxis repository and is treated as the canonical schema for ecosafety envelopes in Phoenix MAR basins.[file:1]

---

## Public API surface

The crate root (`lib.rs`) re-exports the main building blocks:

- SAFE_FLAG and governance:
  - `SafeFlagModel`, `SafeFlagState` from `safe_flag`.
  - `GovernanceChecker`, `GovernanceTag` from `governance_checker`.[file:8]

- Privacy aggregation:
  - `AggregatedShares`, `RiskShare`, `LocalRiskStats`, `GlobalRiskStats`,
    `DpConfig`, `DpGlobalRiskStats`, `LaplaceSampler`,
    `apply_dp_to_global_stats`, `make_risk_shares`, `reconstruct_global_stats`.[file:8]

- Config and frames:
  - `EcosafetyConfig` from `config_reexport`.
  - `CompositeFrame`, `Frame`, `FrameContext`, `FrameError` from `frame`.[file:8]

- Windowing and Lyapunov:
  - `EcosafetyStatus`, `EcosafetyStatusHistory`, `EcosafetyTrend`, `WindowManager` from `window`.[file:8]
  - `LyapunovStabilityDiagnostics`, `LyapunovStabilityFrame`, `VtHistory` from `lyapunov_regime`.[file:8]

- Risk primitives:
  - `RiskCoord`, `RiskVector`, `LyapunovResidual`, `LyapunovWeights`, `KERWindow` from `risk`.[file:8]

- Covariance and integrity:
  - `CovarianceOutput`, `CovarianceSample`, `EcosafetyCovarianceConfig`,
    `EcosafetyCovarianceFrame` (as `CoreCovarianceFrame`), `LyapunovDistance` from `covariance`.[file:8]
  - `IntegrityCheckFrame`, `IntegrityDiagnostics` from `integrity`.[file:8]

- ALN schema and shards:
  - `parse_ecosafety_envelope_schema`, `validate_update` (as `validate_shard_update`),
    `ShardField`, `ShardFieldKind`, `ShardSchema` (as `AlnShardSchema`),
    `ShardUpdate`, `ShardValidationError` from `aln_schema`.[file:1]
  - `ShardSchema` from `shard_schema`.[file:1]
  - `validate_update` from `shard_update_validator`.[file:1]

- Provenance:
  - `Provenance`, `ProvenanceStep` from `provenance`.
  - `ProvenanceDetail` from `provenancedetail`.
  - `EcosafetyProvenanceRecord` from `provenancerecord`.
  - `pipeline_output_to_provenance_records`, `provenance_record_to_csv_row` from `provenanceexport`.[file:1]

- Ecosafety pipeline:
  - `buildecosafetypipeline3`, `EcosafetyPipeline3`, `EcosafetyPipelineOutput` from `pipeline3`.[file:8]

- Types:
  - `CyboNodeEcosafetyEnvelope`, `NodeRiskSample` from `types`.[file:8]

- FOG guard:
  - `FogGuard`, `FogGuardBands`, `FogGuardConfig`, `FogGuardInput`,
    `FogGuardKerThresholds`, `FogGuardVerdict` from `fog_guard`.[file:8]

- Helpers:
  - `fog_guard_input_from_envelope`, `safestep`, `safestep_smoke_test`.[file:8]

---

## Safe-step gating

The helper `safestep` is the canonical entry point for routing and actuation guards:

```rust
pub fn safestep(
    envelope: &CyboNodeEcosafetyEnvelope,
    corridor_present: bool,
    cfg: Option<FogGuardConfig>,
) -> FogGuardVerdict
```

- Uses `FogGuardConfig::default` if no config is provided.[file:8]
- Computes a `FogGuardInput` from the ecosafety envelope (`risk`, `residual`, KER triad, corridor presence).[file:8]
- Returns `FogGuardVerdict::Allow` or `FogGuardVerdict::Stop` depending on KER deployability and residual behavior.[file:8]

This function is non-actuating: it only evaluates math and governance and must be called before any physical actuation proposals.

---

## Canal biodiversity and risk planes

`canal_risk_plane.rs` adds canal-specific biodiversity metrics and risk-plane mappings:

- `CanalBiodiversityMetrics` — Shannon index \(H\), reference bands, curvature parameter, BOD/TSS/temperature, evenness, richness.[file:8]
- `CanalHSensitivity` — local derivatives of \(H\) with respect to BOD, TSS, temperature.[file:8]
- `CanalRiskWeights` — canal-level weights for biodiversity, BOD, TSS, thermal risk.[file:8]
- `CanalRiskPlane` — canal risk coordinates and sensitivities plus segment-level Lyapunov potential.[file:8]
- Functions:
  - `compute_r_biodiversity` — H→r_biodiversity mapping with optional evenness fusion.[file:8]
  - `compute_dr_bio_dh` — derivative ∂r_bio/∂H for sensitivity propagation.[file:8]
  - `normalize_water_quality` — affine normalization of BOD/TSS/temperature into \([0, 1]\).[file:8]
  - `compute_canal_risk_plane` — full canal risk-plane computation from metrics, sensitivities, weights, and corridor bands.[file:8]
  - `embed_canal_risk_into_global` — embedding of canal risk into global `RiskVector`.[file:8]
  - `canal_residual_from_plane` — Lyapunov residual contribution from canal risk plane.[file:8]

These types are intended to align with ALNv2 designs and Perkūnos-Nexus / Organichain shards for canal biodiversity and risk mapping.

---

## Non-actuating and governance constraints

- The crate is compiled with `#![forbid(unsafe_code)]` and denies common error-prone patterns (`unwrap`, `expect`, `panic`, `todo`, `unimplemented`, disallowed methods), matching the ecosafety governance expectations.[file:8]
- All public functions are non-actuating: they compute diagnostics, risk coordinates, and residuals but never drive hardware, relays, or pumps.[file:8]
- ALN-bound functions (`parse_ecosafety_envelope_schema`, shard validators) enforce corridor invariants and Lyapunov monotonicity, supporting ZK and Merkle-based governance particles in ALNv2.[file:1]

---

## Dependencies and versions

- Rust edition: 2024 (configured in the workspace).
- `rust-version = "1.85"` in `Cargo.toml`.
- `serde = "1.0.203"` and `serde_json = "1.0.120"` for serialization and ALNv2 bindings.[file:1]
- Kani verifier is required at the workspace level (e.g., `kani-verifier = "0.67"`), and Kani harnesses live in `*_kani.rs` modules; they are never optional.[file:8]

---

## Usage

Basic usage pattern inside the Prometheus-Praxis workspace:

- Construct or load a `CyboNodeEcosafetyEnvelope` from ALNv2 / SQL shards.[file:1]
- Evaluate a safestep verdict:

```rust
use cyboquatic_ecosafety::{FogGuardConfig, safestep};

let envelope = /* load or construct ecosafety envelope */;
let verdict = safestep(&envelope, /* corridor_present */ true, None);

match verdict {
    cyboquatic_ecosafety::FogGuardVerdict::Allow => {
        // Routing or actuation proposal is ecosafety-admissible (non-actuating decision).
    }
    cyboquatic_ecosafety::FogGuardVerdict::Stop => {
        // Proposal violates KER or residual constraints and must be rejected.
    }
}
```

- For canal-specific work, compute biodiversity and canal risk planes:

```rust
use cyboquatic_ecosafety::{
    CanalBiodiversityMetrics, CanalHSensitivity, CanalRiskWeights,
    compute_canal_risk_plane, Scalar,
};

let metrics = CanalBiodiversityMetrics {
    segment_id: "PHX-CANAL-SEG-017".to_string(),
    shannon_index_h: Scalar(2.5),
    h_min_ref: Scalar(0.5),
    h_max_ref: Scalar(3.0),
    curvature_alpha: Scalar(2.0),
    bod: Scalar(3.0),
    tss: Scalar(10.0),
    temperature_c: Scalar(24.0),
    evenness_j: Scalar(-1.0),
    richness_s: Scalar(-1.0),
};

let sens = CanalHSensitivity {
    d_h_dbod: Scalar(-0.05),
    d_h_dtss: Scalar(-0.02),
    d_h_dtemp: Scalar(0.01),
};

let weights = CanalRiskWeights {
    w_bio: Scalar(2.0),
    w_bod: Scalar(1.0),
    w_tss: Scalar(1.0),
    w_thermal: Scalar(0.5),
};

let plane = compute_canal_risk_plane(
    &metrics,
    &sens,
    &weights,
    Scalar(1.0),
    Scalar(10.0),
    Scalar(2.0),
    Scalar(20.0),
    Scalar(10.0),
    Scalar(30.0),
);

// plane.r_biodiversity, plane.v_segment, and sensitivities can feed KER windows and ALNv2 shards.
```

---

## License

Dual-licensed under MIT OR Apache-2.0, consistent with the Prometheus-Praxis mono-repo policy.[web:149]
