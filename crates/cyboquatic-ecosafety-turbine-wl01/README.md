# cyboquatic-ecosafety-turbine-wl01

Rust ecosafety bindings for PHX‑CANAL‑NODE‑WL‑01 microturbines on Phoenix canals, wired into the shared `rx, Vt, KER` spine so every turbine workload is corridor‑checked and safestep‑gated before it can be routed or deployed.[file:33][file:31]

---

## Purpose

- Provide turbine‑specific math objects and risk aggregation for:
  - Hydraulic envelope: \(Q\), HLR, rsurcharge, cavitation, overpressure, hydraulic efficiency.[file:31][file:33]
  - Fish shear: blade tip speed \(v_\text{tip}\), shear stress \(\tau\), pressure drop \(\Delta P\), lethality L, and r_fishshear.[file:33]
  - Ramp and turbulence: ramp rate \(\mathrm{d}u/\mathrm{d}t\), turbulence intensity \(I\), rramp, rturbulence, rhabitat.[file:33]
  - Biodiversity and pathogen: rbiodiversity from connectivity–complexity–colonization, rpathogen from biology plane.[file:31][file:33]
- Lift those coordinates into the global `RiskVector`, Lyapunov residual \(V_t\), and `KerWindow` scoring for PHX‑CANAL‑NODE‑WL‑01.[file:33]

---

## Scope and non‑actuating guarantees

- This crate:
  - Only reads shard‑level physical and normalized risk fields for turbine workloads.[file:33]
  - Computes aggregate risk coordinates, residuals, and KER windows with pure functions.[file:33]
  - Exposes helpers to plug turbine workloads into the existing `SafeStepGate` and production KER band.[file:33]
- It never:
  - Talks to actuators (no valves, pumps, setpoints, nanocontrol surfaces).
  - Bypasses ALN contracts or corridor invariants defined in qpudatashards.[file:33]
  - Introduces new control semantics outside the shared ecosafety grammar.

---

## Key types

- `TurbineShard`
  - Represents one PHX‑CANAL‑NODE‑WL‑01 turbine shard row (canal + turbine fields).[file:33][file:31]
  - Includes:
    - Hydraulic plane: `qm3s`, `hlrmperh`, `rsurcharge`, `rcavitation`, `roverpressure`, `renergy_hydraulic`.[file:31][file:33]
    - Shear plane: `headm`, `vtip_ms`, `shear_tau_pa`, `delta_p_pa`, `lethality_index`, `rfishshear`.[file:33]
    - Habitat dynamics: `ramp_rate_du_dt`, `turbulence_I`, `rramp`, `rturbulence`, `rhabitat`.[file:33]
    - Biodiversity and pathogen: `rbiodiversity`, `rpathogen`.[file:31][file:33]
    - Global planes: `renergy`, `rcarbon`, `rmaterials`, `rsigma`, `vt`.[file:33]

- External types reused:
  - From `cyboquatic-ecosafety-core`:
    - `RiskCoord`, `RiskVector`, `LyapunovWeights`, `Residual`, `KerWindow`, `SafeStepGate`, `SafeStepDecision`.[file:33]
  - From `cyboquatic-ecosafety-hydroturbine`:
    - `HydraulicRisk`, `HydraulicWeights`, `HabitatDynamics`, `HabitatWeights`, `FishShearInputs`, `LethalityCorridor`, `lethality_index`.[file:33]

---

## Core functions

- `aggregate_hydraulics(shard: &TurbineShard, w: HydraulicWeights) -> RiskCoord`
  - Aggregates rsurcharge, rcavitation, roverpressure, and renergy_hydraulic into a single hydraulic coordinate via a weighted quadratic sum.[file:33]
  - Feeds the hydraulics plane in `RiskVector.rhydraulics`.

- `aggregate_habitat(shard: &TurbineShard, w: HabitatWeights) -> RiskCoord`
  - Aggregates rramp and rturbulence into `rhabitat`.[file:33]
  - Feeds the biology plane (habitat component) in `RiskVector.rbiology`.

- `compute_fish_shear(shard: &TurbineShard, k_v, k_tau, k_dp: f64, corridor: LethalityCorridor) -> RiskCoord`
  - Computes lethality \(L(v_\text{tip}, \tau, \Delta P)\) from physical inputs and maps it into normalized r_fishshear via corridor bands (safe, gold, hard).[file:33]

- `build_risk_vector(shard: &TurbineShard, rhydraulics: RiskCoord, rbiology: RiskCoord) -> RiskVector`
  - Builds a full `RiskVector` for the turbine workload:
    - `renergy` from CEIM energy‑per‑mass kernels.[file:33]
    - `rhydraulics` from `aggregate_hydraulics`.
    - `rbiology` from habitat and pathogen contributions.
    - `rcarbon`, `rmaterials`, `rbiodiversity`, `rsigma` from shard fields.[file:33]

- `evaluate_turbine_step(shard_next: &TurbineShard, weights: LyapunovWeights, vt_current: Residual, rhydraulics: RiskCoord, rbiology: RiskCoord) -> (Residual, SafeStepDecision)`
  - Uses `SafeStepGate` to enforce:
    - \(V_{t+1} \le V_t\) outside the safe interior.[file:33]
    - All risk coordinates in [0,1].
  - Returns updated residual and `SafeStepDecision::Accept`/`Reject` for a proposed turbine workload step.[file:33]

- `ker_window_for_turbine(residual_series: &[Residual], max_risks: &[f64]) -> Option<KerWindow>`
  - Computes KER window scores (K, E, R) for turbine workloads:
    - `K` fraction of safestep‑satisfying steps.
    - `E` eco‑impact (1 − mean max risk).
    - `R` max risk across the window.[file:33]
  - Used to enforce production band \(K \ge 0.90, E \ge 0.90, R \le 0.13\).[file:33]

---

## Usage patterns

- In controllers and CI:
  - Parse PHX‑CANAL‑NODE‑WL‑01 turbine shard CSV into `TurbineShard` instances (using serde in production).[file:33][file:31]
  - For each row:
    - Compute `rhydraulics = aggregate_hydraulics(&shard, hydraulic_weights)`.
    - Compute `rbiology = aggregate_habitat(&shard, habitat_weights)`.
    - Optionally recompute `rfishshear = compute_fish_shear(&shard, k_v, k_tau, k_dp, lethality_corridor)` and assert corridor membership.[file:33]
    - Build `RiskVector` and run `evaluate_turbine_step` to enforce safestep.
  - After a series:
    - Call `ker_window_for_turbine` to check if the turbine workload series meets production KER thresholds before routing or deployment.[file:33]

- With ALN shards:
  - The crate is referenced in `CyboquaticPhoenixTurbineWL01_2026v1.aln` using:
    - `derivedinrust cyboquatic-ecosafety-hydroturbine::aggregate_hydraulics` for rhydraulics.[file:33]
    - `derivedinrust cyboquatic-ecosafety-hydroturbine::aggregate_habitat` for rbiology.[file:33]
    - `derivedinrust cyboquatic-ecosafety-core::KerWindow::fromresidualseries` for kwindow, ewindow, rwindow.[file:33]
  - CI and controllers treat these Rust functions as the binding layer between physical turbine metrics and ALN corridor invariants.

---

## Design constraints and scoring

- Design constraints:
  - Rust edition 2024, `rust-version = "1.85"`.
  - `#![forbid(unsafe_code)]` across all modules.
  - No optional Kani; future formal verification hooks must use `kani 0.67` exactly when integrated.[file:33]
  - No use of blacklisted hashes or cryptographic primitives (no blake*, no SHA3‑256); provenance remains ALN + hex‑stamped with Bostrom DID.[file:33]

- Ecoscores for this crate:
  - Knowledgefactor: 0.95 — direct specialization of existing `rx, Vt, KER` semantics to turbine hydraulics, shear, habitat, biodiversity, and pathogen planes.[file:33][file:31]
  - Ecoimpact: 0.92 — structurally blocks turbine workloads that would harm fish, habitat, or pathogen risk, even if they improve energy efficiency.[file:33][file:31]
  - Risk‑of‑harm: 0.12 — residual risk confined to corridor calibration and sensor/model error; all actuation remains fenced by RustALN and KER gates.[file:33]

---

## Repository layout

- Recommended paths inside `Prometheus-Praxis` mono‑repo:

  - `crates/cyboquatic-ecosafety-core/`
    - Shared spine types: `RiskCoord`, `RiskVector`, `LyapunovWeights`, `Residual`, `KerWindow`, `SafeStepGate`.[file:33]
  - `crates/cyboquatic-ecosafety-hydroturbine/`
    - Turbine‑level math for hydraulics, fish shear, and habitat (already defined).[file:33]
  - `crates/cyboquatic-ecosafety-turbine-wl01/`
    - This crate: shard struct, binding functions, safestep and KER helpers for PHX‑CANAL‑NODE‑WL‑01.[file:33][file:31]
  - `qpudatashards/CyboquaticPhoenixTurbineWL01_2026v1.aln`
    - ALN manifest that declares turbine shard schema, corridor tables, and invariants.[file:33][file:31]
  - `ci/cyboquatic_turbine_wl01_check/`
    - CI utility that reads turbine shard CSV, calls this crate, and enforces no‑corridor‑no‑build + safestep + KER production band.[file:33]

---

## Contribution guidelines

- When extending this crate:
  - Add only new risk coordinates or aggregation functions that can be tied to Phoenix pilot evidence and ALN corridor tables.[file:33][file:31]
  - Keep all actuation logic in separate, more tightly governed crates that already implement PilotGate and evolution gates.
  - Update ecoscores (knowledgefactor, ecoimpact, residualrisk) and attach evidence hashes whenever corridors are tightened or calibrated with new data.[file:33]

- All new code must:
  - Compile stand‑alone (no CI/CD required to run the core logic).
  - Remain non‑malicious, corridor‑tightening, and eco‑positive, aligning with Ecological restoration, Cyboquatic systems, and carbon‑negative machinery goals.[file:33][file:31]
