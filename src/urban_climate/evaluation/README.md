# Urban Climate Evaluation Kernel

This crate implements the **urban climate model evaluation kernel** for Prometheus‑Praxis / Aletheion, grounded in:

- The governance spine and multi‑plane Lyapunov/KER framework (`2b586028-a87b-4194-93b8-f5794c194e8a.md`).[8]
- Ecological corridor and scheduling shards (soil diagnostics, biodiversity floors, safe‑heat/labor windows) in `e4ba68f5-edd6-438f-8dff-822a2527b867.md`.[48]

It provides:

- A Rust representation of `UrbanClimateModelEvaluation2026v1.aln`.
- Guard functions enforcing minimum KER/RoH/Biodiversity/Eco‑planes/SLA/Neurorights scores.
- Kani harnesses proving that deployment eligibility respects those invariants.

## Layout

```text
src/urban_climate/evaluation/
  Cargo.toml                       # crate manifest
  README.md                        # this file
  src/
    lib.rs                         # crate entry; re-exports evaluation APIs
    urban_climate_model_evaluation.rs      # core structs, guards, ALN integration
    urban_climate_model_evaluation_kani.rs # Kani harnesses
  aln/
    UrbanClimateModelEvaluation2026v1.aln # evaluation shard
```

## ALN shard

The shard `aln/UrbanClimateModelEvaluation2026v1.aln` defines:

- `UrbanClimateModelComponent` with scores (0..5) for:
  - ker-score
  - roh-score
  - biodiversity-score
  - ecological-planes-score
  - urban-corridors-score
  - streaming-sla-score
  - neurorights-score
- `UrbanClimateModelEvaluationEnvelope` grouping components per region.
- `MinScoresInvariant` and `EnforceUrbanClimateModelEligibility` to decide which components may be deployed.

This shard is parsed via `alncore::parse_aln_str` and used as source‑of‑truth in Rust.[8]

## Rust API

Main types and functions:

```rust
use urban_climate_evaluation::{
    UrbanClimateModelComponent,
    UrbanClimateModelEvaluationEnvelope,
    EligibilityDecision,
    EligibilityVerdict,
    enforce_urban_climate_model_eligibility,
};

let env = UrbanClimateModelEvaluationEnvelope {
    envelope_id: "PhoenixUrbanModels2026".to_string(),
    region_context: "PhoenixMetro2026Arid".to_string(),
    components: vec![/* filled from ALN or config */],
    created_at_utc: "2026-08-02T00:00:00Z".to_string(),
};

let decisions = enforce_urban_climate_model_eligibility(&env);
// Only components with scores >= minima are marked Eligible.
```

## Wiring into Prometheus‑Praxis

1. **Add crate to workspace**

In `Prometheus-Praxis/Cargo.toml` (if using workspace):

```toml
[workspace]
members = [
    "src/urban_climate/evaluation",
    # other crates...
]
```

2. **Depend on `urban_climate_evaluation` from orchestrators**

In a smart‑city orchestrator crate’s `Cargo.toml`:

```toml
[dependencies]
urban_climate_evaluation = { path = "../urban_climate/evaluation" }
alncore                  = { path = "../../alncore" }
```

Then, before deploying any model:

```rust
use urban_climate_evaluation::{UrbanClimateModelEvaluationEnvelope, enforce_urban_climate_model_eligibility};

fn check_models_before_deploy(env: &UrbanClimateModelEvaluationEnvelope) -> bool {
    let decisions = enforce_urban_climate_model_eligibility(env);
    decisions.iter().all(|d| d.verdict == EligibilityVerdict::Eligible)
}
```

3. **Integrate ALN shard**

At build or deploy time:

- Load `aln/UrbanClimateModelEvaluation2026v1.aln`.
- Parse with `alncore::parse_aln_str`.
- Map parsed objects into `UrbanClimateModelComponent` instances.

This keeps ALN as the **source‑of‑truth** while Rust provides enforcement.

## Kani verification

Run Kani against `urban_climate_model_evaluation_kani.rs` to prove:

- Any component below thresholds is always `Ineligible`.
- Any component meeting thresholds is always `Eligible`.

Example (from repo root if you use Kani per crate):

```bash
cd src/urban_climate/evaluation
kani src/urban_climate_model_evaluation_kani.rs
```

This aligns with the governance framework’s pattern of using Kani to prove invariants over decision kernels (Lyapunov guards, RoH ceilings, treaty gates).[8]

---
