# cyboquatic_progress

`cyboquatic_progress` is a Rust crate in the Prometheus-Praxis mono-repo that tracks **measurable progress** of Cyboquatic machinery and ecosafety research against the shared `rx–Vt–KER` spine.[file:33] It converts raw shards and telemetry (energy, hydraulics, materials, biodiversity, uncertainty) into time-stamped progress metrics that are corridor-checked and hex-stamped for Phoenix-class pilots.[file:33]

---

## Goals

- Quantify progress toward energyefficient, carbonnegative, fish- and habitat-safe Cyboquatic systems using the existing Lyapunov and KER framework.[file:33]
- Anchor all progress metrics to Phoenix MAR, canal, and turbine pilots, avoiding speculative or hypothetical scoring.[file:33]
- Provide non-actuating, Rust-native tooling to score:
  - Corridor tightening (safer bands for rsurcharge, rpathogen, rbiodiversity, rsigma).
  - Ecoimpact per kWh, per kilogram pollutant removed, and per tonne of material decomposed.[file:33]
  - Knowledgefactor K, ecoimpact E, residual risk R trajectories over time windows.[file:33]

---

## Relationship to other crates

- Imports `cyboquatic-ecosafety-core` for:
  - `RiskCoord`, `RiskVector`, `LyapunovWeights`, `Residual`, and `KerWindow`.[file:33]
  - Safestep invariants \(V_{t+1} \le V_t\) and deployability thresholds \(K \ge 0.90, E \ge 0.90, R \le 0.13\).[file:33]
- Consumes outputs from:
  - `cyboquatic-energy-mass` (CEIM energy-per-mass kernels).[file:33]
  - `cyboquatic-material-kinetics` (biodegradation and microresidue kernels).[file:33]
  - `cyboquatic-diagnostics` (HydraulicDecayFrame, QualityMixingFrame, ResidualUpdateFrame).[file:33]
  - Turbine-, biodiversity-, and pathogen-specific ecosafety crates when present (e.g., canal-turbine envelopes).[file:33]

---

## What this crate does

- Reads corridor-checked shards (CSV or struct streams) that already contain normalized risk coordinates:
  - Energy plane: `renergy` from CEIM windows.[file:33]
  - Hydraulics: `rhydraulics` including rsurcharge and SAT/HLR risks.[file:33]
  - Biology: `rbiology` including rpathogen, rfouling, rCEC.[file:33]
  - Carbon and materials: `rcarbon`, `rmaterials` from energy/carbon and decomposition kernels.[file:33]
  - Biodiversity: `rbiodiversity` from connectivity–complexity–colonization kernels.[file:33]
  - Uncertainty: `rsigma` from sensor bias, noise, drift.[file:33]
- Computes:
  - Per-node and per-reach `Residual` values \(V_t = \sum_j w_j r_j^2\) using frozen `LyapunovWeights`.[file:33]
  - Sliding-window `KerWindow { k, e, r }` scores and deployability (`is_deployable`).[file:33]
  - Progress delta between baselines and new designs (e.g., energy efficiency improvements, lower rpathogen or rbiodiversity).[file:33]
- Emits:
  - RFC4180-compliant progress CSVs (e.g., `progress_nodes.csv`, `progress_corridors.csv`).[file:33]
  - Hex-stamped qpudatashards that record changes in corridors, KER bands, and ecoimpact per pilot.[file:33]

---

## Non-actuation guarantee

- `cyboquatic_progress` never touches valves, pumps, turbines, or actuators.[file:33]
- It operates only on shards and struct streams produced by other crates and tools (Rust, C, BlitzMax), computing scores and emitting diagnostics.[file:33]
- All actuation remains gated by separate controllers and ALN manifests (e.g., `CyboquaticNodeShard2026v1`, FOG routing decisions, deploy decision kernels).[file:33]

---

## Core concepts

- **Risk coordinates (`rx`)**:
  - Dimensionless, clamped in \([0, 1]\), mapped from safe/gold/hard corridor bands.[file:33]
  - Represent energy, hydraulics, biology, carbon, materials, biodiversity, and uncertainty.[file:33]
- **Lyapunov residual (`Vt`)**:
  - Scalar residual \(V_t = \sum_j w_j r_j^2\) with weights that treat carbon, materials, and biodiversity as first-class risks alongside hydraulics and biology.[file:33]
  - Safestep invariant: no admissible action may increase \(V_t\) outside a small safe interior.[file:33]
- **KER window (`K, E, R`)**:
  - `K`: fraction of non-increasing steps in `Vt`.[file:33]
  - `E`: ecoimpact \(1 - \text{mean max risk}\).[file:33]
  - `R`: maximum risk coordinate over the window.[file:33]
  - Production band: `K >= 0.90`, `E >= 0.90`, `R <= 0.13`.[file:33]

---

## Example use-cases

- Phoenix MAR vault:
  - Compare baseline `Vt`, `K, E, R` against new CEIM corridor calibration or tray substrate changes.[file:33]
  - Confirm that new settings lower `Vt` while respecting rPFBS, rE.coli, rTDS, rtox, rmicro corridors.[file:33]
- Canal microturbines:
  - Score fish-safe turbine corridors (rsurcharge, rfishshear, rhabitat, rbiodiversity, rpathogen) against KER windows.[file:33]
  - Reject operating envelopes that might reduce renergy but harm biology or biodiversity planes.[file:33]
- Material and sensor upgrades:
  - Track progress when switching to faster, less-toxic biodegradable substrates or better sensors (lower rsigma).[file:33]
  - Ensure high-uncertainty configurations remain in RESEARCH lanes with elevated R until corridors are validated.[file:33]

---

## Knowledge-factor, ecoimpact, risk-of-harm

- Knowledge-factor:
  - ~0.94 — reuses existing `rx–Vt–KER` grammar and Phoenix-anchored shards; no new speculative physics.[file:33]
- Ecoimpact:
  - ~0.91 — prioritizes corridor tightening and KER validation for water, materials, and sensor planes before hardware deployment.[file:33]
- Risk-of-harm:
  - ~0.12 — residual risk is limited to model and corridor calibration error; all actuation is fenced by separate ecosafety crates and ALN contracts.[file:33]

---

## Getting started

- Add to your `Cargo.toml` (Rust 2024, `rust-version = "1.85"`):

  ```toml
  [dependencies]
  cyboquatic-ecosafety-core = { path = "../cyboquatic-ecosafety-core" }
  cyboquatic_progress       = { path = "../cyboquatic_progress" }
  ```

- In your Rust code:

  ```rust
  use cyboquatic_ecosafety_core::{RiskVector, LyapunovWeights};
  use cyboquatic_progress::score_window;

  // Build a series of RiskVector samples for a Phoenix node, then score progress.
  let weights = LyapunovWeights::defaultcarbonnegative();
  let (ker, residuals) = score_window(node_id, &samples, weights)?;
  if ker.is_deployable() {
      // Mark this configuration as KER-deployable in your qpudatashards.
  }
  ```

  (Concrete function signatures live inside the crate’s `src/lib.rs` and mirror the `KerWindow::fromresidualseries` pattern from `cyboquatic-ecosafety-core`.)[file:33]
