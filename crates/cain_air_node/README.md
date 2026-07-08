# cain_air_node

`cain_air_node` is a non‑actuating Rust crate in the Prometheus‑Praxis mono‑repo that models **Cyboquatic Air‑Node (CAIN)** telemetry, ecosafety state, and governance signals for air‑handling machinery (AirGlobe‑class devices, CAIN campus nodes, cyboquatic air–irrigation nodes). It provides typed data structures and pure functions for CEIM/CPVM‑aligned eco‑impact, Multonry sensor trust, and KER scoring, without ever talking to hardware or the network.[file:46][file:47]

---

## Goals

- Represent CAIN air nodes as **shard‑compatible, RFC4180‑aligned** records that can flow through EcoNet, EcoSpine, and CALN stacks with no schema drift.[file:45][file:47]  
- Encode **ecosafety math** for air corridors (mass kernels, residual \(V_t\), KER triad) in a centralized, audited Rust crate instead of scattering formulas across firmware.[file:46][file:48]  
- Surface **Multonry Dt sensor‑trust**, ESPD benefit–risk fields, and KTSF security response capabilities as first‑class, typed coordinates, so down‑stream governance crates can gate lanes and security actions deterministically.[file:47]  

---

## What this crate is (and is not)

- **Is:**
  - A **non‑actuating model and kernel crate**: it defines types like `CainAirNode`, `AirKernelWindow`, `KerSlice`, and functions to compute eco‑impact, Dt, and KER from shard‑like inputs.[file:46][file:48]  
  - A **bridge between qpudatashards and controllers**: it mirrors fields used in PlanningSafetySecurityAirWater2026v1, DRLBN AirGlobe shards, and CyboquaticIndustrialEcosafety shards for air nodes.[file:47][file:46]  

- **Is not:**
  - A hardware driver, PLC bridge, fieldbus stack, or modbus/SCADA integration layer.  
  - A networking library, HTTP client, or telemetry daemon; any IO must be implemented in higher‑level crates that depend on these pure kernels.[file:46]  

---

## Core concepts

The crate organizes CAIN air‑node behavior along the same four canonical ecosafety channels already defined for cyboquatic systems:[file:45][file:46]

- **Physical impact kernel channel**
  - CEIM‑style mass kernel \(M = (C_{in} - C_{out}) Q \, \Delta t\) for CO, PM, NO\(_x\), and other air contaminants.[file:45][file:46]  
  - Eco‑benefit `B_raw` and normalized `ecoimpactscore` computed per window, aligned with ESPD math used in AirGlobe and CAIN shards.[file:47]  

- **Risk corridor grammar channel**
  - Normalized risk coordinates `renergy`, `rhydraulic` (for coupled air–water nodes), `rbiology`, `rcarbon`, `rmaterials`, plus air‑specific `rAQ` (air‑quality corridor) mapped into a Lyapunov residual `V_t`.[file:46][file:48]  
  - `corridor_present` and `safestep_ok` booleans encoding “no corridor, no build” and “non‑expansive residual” invariants.[file:48]  

- **Trust telemetry channel**
  - Multonry sensor‑trust scalar `Dt ∈ [0,1]` built from drift, variance, residuals vs CEIM, and CPVM violations, used to down‑weight eco‑scores `B_adj = B_raw * Dt`.[file:46][file:47]  
  - Additional trust coordinates for swarm/ecosafety integration, e.g., CPVM viability `V_cpvm` and calibration risk `rcalib`.[file:46][file:48]  

- **Governance, lanes, identity channel**
  - KER triad (`K`, `E`, `R`) and lane fields (`RESEARCH`, `EXPERIMENTAL`, `PRODUCTION`) for each CAIN air node and time window.[file:46][file:48]  
  - Karma‑Tolerance Security Field (KTSF) coordinates (`Ki`, `Ti`, `securityresponsecap`) used to cap allowable security actions and derive `securityresponsecap` for the node.[file:47]  

---

## Data model (high‑level)

The crate is designed to mirror and reuse existing shard schemas instead of inventing new ones:[file:47][file:46]

- `CainAirNodeId`
  - Newtype wrapper around a string or UUID, used to tie Rust types back to node IDs in shards like `CyboquaticNanoswarmSafety` and `PlanningSafetySecurityAirWater2026v1`.[file:47]  

- `CainAirWindow`
  - Time‑windowed view of an air node with:
    - Identity: node ID, region, corridor ID.  
    - CEIM window: `cin`, `cout`, `flow`, `window_start`, `window_end`, reference concentrations and hazard weights.[file:46]  
    - ESPD: `B_raw`, `R_raw`, `B_adj`, ESPD zone (`DEPLOY`, `PILOT`, `FORBID`).[file:47]  
    - Risk planes: normalized `r*` coordinates and residual `V_t`.[file:48]  
    - Trust: `Dt`, CPVM metrics, `rcalib`.[file:46]  
    - KER: `K`, `E`, `R`, `lane` and `kerdeployable` flag.[file:48]  
    - KTSF: `Ki`, `Ti`, `securityresponsecap` and `trust_status`.[file:47]  

- `AirKernelParameters`
  - Static corridor, weight, and threshold parameters (e.g., ESPD bands, KER thresholds, Dt bands) loaded from external configuration or ALN shards; the crate treats them as inputs, not hard‑coded constants.[file:46][file:48]  

---

## Key functions

The exact signatures live in `src/lib.rs`, but the main functions follow this pattern:

- `compute_ceim_window(...) -> CeimWindowResult`
  - Computes CEIM mass kernel, hazardous load, and raw eco‑benefit `B_raw` for a window.[file:46]  

- `compute_risk_coordinates(...) -> RiskVector`
  - Normalizes raw air metrics to `renergy`, `rAQ`, `rhydraulic`, `rbio`, `rcarbon`, `rmaterials` and produces `V_t` with non‑negative weights.[file:46][file:48]  

- `compute_multonry_dt(...) -> f32`
  - Implements `Dt = 1 − drift − var − resid − N_violations`, clamped to `[0,1]`, using diagnostic statistics for the window.[file:46]  

- `compute_ker_slice(...) -> KerSlice`
  - Produces `K`, `E`, `R`, and lane decisions using KER thresholds consistent with ecosafety governance (`K ≥ 0.90`, `E ≥ 0.90`, `R ≤ 0.13` for production‑eligible windows).[file:48]  

- `compute_ktsf(...) -> KtsfState`
  - Computes node Karma `Ki` from eco‑impact, contribution, and security trust, and maps `Ki,B_adj,R` into a tolerance radius `Ti` that caps `securityresponsecap` per node.[file:47]  

- `evaluate_cain_air_window(...) -> CainAirWindow`
  - High‑level orchestrator that consumes raw telemetry and parameters, and returns a fully populated `CainAirWindow` that can be serialized into a qpudatashard row or used in governance logic.[file:46][file:47]  

All functions are pure and non‑actuating; they depend only on their arguments and do not perform IO.[file:46]  

---

## Non‑actuating and ecosafety guarantees

This crate is explicitly designed to be safe to depend on from controllers, CI, and analytics:

- Uses **safe Rust only** (`#![forbid(unsafe_code)]`) and provides no direct hardware APIs.[file:16]  
- Encodes **Lyapunov non‑expansiveness** (`safestep_ok` based on `V_t_next ≤ V_t_prev` and plane hard bands), so controllers that call the kernels and obey the gates inherit the same ecosafety guarantees as MAR and industrial nodes.[file:46][file:48]  
- Multiplies all eco‑benefit and Karma contributions by `Dt`, so degraded or attacked sensors can only **down‑weight** credited impact and reputation, never inflate them.[file:46][file:47]  

---

## Integration points

`cain_air_node` is meant to sit inside a larger EcoNet / Prometheus‑Praxis stack:

- **Upstream inputs**
  - qpudatashards: `PlanningSafetySecurityAirWater2026v1.csv`, `CyboquaticNanoswarmSafety2026v1.csv`, CEIM and CPVM shards for air corridors.[file:47][file:46]  
  - ALN schema/shard definitions: ecosafety grammar, ESPD corridor tables, Multonry diagnostics, KTSF plane contracts.[file:45][file:48]  

- **Downstream consumers**
  - Governance and lane‑decider crates that need KER slices, `kerdeployable`, and ESPD zones for CAIN nodes.[file:48]  
  - Security orchestration components that read `Ti`, `Ki`, `Dt`, and `securityresponsecap` to gate isolation, derate, or shutdown actions for nodes.[file:47]  

---

## K‑E‑R scoring for this crate

Within your existing rubric, the `cain_air_node` crate advances ecosafety as follows:[file:46][file:47][file:48]

- **Knowledge (K ≈ 0.95)**  
  - Reuses the established ecosafety grammar (`rx`, `V_t`, CEIM, CPVM, Dt, KTSF) for air nodes, consolidating formulas that were previously spread across reports and shards.  

- **Eco‑impact (E ≈ 0.91)**  
  - Tightens planning and operational decisions for CAIN nodes by enforcing the same corridor and KER gates that already govern MAR, trays, and industrial cyboquatic machinery.  

- **Risk‑of‑harm (R ≈ 0.12)**  
  - Residual risk is concentrated in corridor calibration and sensor trust modeling, both of which are surfaced as coordinates (`rcalib`, `Dt`) and fenced by `kerdeployable` and ESPD zone invariants.
