# ARCHITECTURE.md – Hybrid ALN–Rust–Kani Pipeline Blueprint

- This snippet turns `ARCHITECTURE.md` into a living map from crates and `.aln` shards to a verifiable ALN→Rust→Kani pipeline, with `aln_shard` driving `#[kani::proof]` and `cargo kani` gating corridor changes. [file:14][file:26]

## 1. Spine Overview: From ALN Contracts to Kani-Gated Rust

- Core principle: “no schema, no build; no corridor, no build; no weakening, no merge” enforced at three layers. [file:4][file:25]  
- ALN layer:
  - `.aln` particles define corridors, risk coordinates \(r_j\), Lyapunov residual kernels \(V_t = \sum w_j r_j^2\), and K,E,R targets. [file:25][file:25]  
  - Examples: `DrainageRiskVector`, `WorkloadRiskVector`, `PlaneWeightsShard2026v1`, `LaneStatusShard2026v1`. [file:2][file:14]  
- Rust layer:
  - Crates like `kerresidual`, `econet-governance-spine`, `cyboquatic_drainage_decayYYYYMMDD`, `cyboquatic_workloadYYYYMMDD` implement ALN grammar as Rust structs and corridor kernels. [file:2][file:14][file:4]  
  - All safety-critical structs derive an `aln_shard` macro that binds them to a specific `.aln` contract. [file:26]  
- Kani layer:
  - Each crate carries `#[kani::proof]` harnesses proving corridor invariants and KER bounds. [file:1][file:2][file:25]  
  - `cargo kani` is wired into CI as a build-blocking step for all SPINE and ecosafety crates. [file:14][file:4]

## 2. Crate–Contract Map in the Pipeline

- Ecosafety math core:
  - `kerresidual` (band: Lyapunov Fused Risk) – implements normalized risk vectors and residual \(V_t\), used by all domain crates. [file:4][file:25]  
  - `advanced-safety-math`, `corridor-safety-core`, `eco-quadratic-oracle` – house canonical corridor kernels and fused risk math. [file:6][file:25]  
- EcoNet governance spine:
  - `econet-governance-spine` – readonly access to `planeweights`, `lanestatusshard`, `blastradiusindex`, `definitionregistry`, `mt6883registry`. [file:14][file:4]  
  - `dbdbeconetgovernancespine.sql` – schema for KER residuals, plane weights, lane status, blast radius. [file:14]  
- Daily cyboquatic crates:
  - `cyboquatic_drainage_decay20260708` – BOD/TSS/CEC drainage frame; ALN particle `DrainageDecayPhoenix20260708.aln`. [file:2]  
  - `cyboquatic_workload20260709` – energy tailwind + Lyapunov \(\Delta V_t\); ALN particle `WorkloadEnergyDeltaVtPhoenix20260709.aln`. [file:1]  
  - Future: `cyboquatic_progressYYYYMMDD` – daily crate band `cyboquatic_progress` keyed to Phoenix evidence hex and prior crate ID. [file:1][file:2]  
- ALN sovereignty layer:
  - `aln-core`, `aln-kernel`, `aln-registry` – define ALN grammar, particle registry, and policy contracts. [file:6]  
  - `DefinitionRegistry2026v1.aln` + `dbdbdefinitionregistry.sql` – canonical index mapping DR IDs to `.aln`, `.sql`, `.rs`, planes, and corridors. [file:14]  

## 3. `aln_shard` Procedural Macro: How ALN Drives Rust

- Role:
  - Turn `.aln` contracts into compile-time-enforced Rust structs with guaranteed schema and corridor binding. [file:26]  
- Invocation pattern:
  - Developer writes:
    - `#[derive(AlnShard)]`
    - `#[aln_contract("qpudatashards/DrainageDecayPhoenix20260708.aln", version = "2026v1")]`
    - On a Rust struct (e.g., `DrainageSample` or `WorkloadSample`). [file:2][file:1][file:26]  
- Compile-time responsibilities:
  - Parse target `.aln` particle:
    - Verify each mandatory column has a matching Rust field, with mapped type (e.g., `RiskCoord01` → `RiskCoord`, `Hex64Evidence` → `EvidenceHex`). [file:26]  
    - Reject builds on any missing field, type mismatch, or mandatory flag discrepancy (hard “no schema, no build”). [file:26]  
  - Normalization kernel emission:
    - For `is_risk_coord = true` rows, generate per-coordinate functions using canonical piecewise-affine corridor kernel from `ecosafety-core-v2`. [file:26]  
    - Bind safe/gold/hard bands from `.aln` to `CorridorBands` constants, ensuring exactly one source of truth. [file:25][file:26]  
  - Trait integration:
    - Auto-implement `RiskVector`, arranging risk fields in canonical order expected by `kerresidual`’s \(V_t\). [file:26][file:25]  
    - Auto-implement `KerDeployable`, `KerWindowUpdate` where specified, so domain crates plug directly into KER scoring. [file:26][file:25]  
  - Versioning:
    - Enforce explicit `aln_contract_version` attribute (e.g. `2026v1`), reject mixed major versions in one crate. [file:26]  
    - If a coordinate appears in multiple versions, apply the **tightest bands** at compile time and log band evolution into the struct for provenance. [file:26]  

## 4. Auto-Generated `#[kani::proof]` Harnesses

- Harness synthesis by `aln_shard`:
  - For every derived shard type, the macro emits one or more `#[kani::proof]` functions into a `verification` module. [file:1][file:2][file:25][file:26]  
- Typical harness patterns:
  - Corridor clamping and residual safety:
    - Assume raw coordinates within allowed input ranges (e.g. sensor ranges, corridor metadata). [file:2][file:1]  
    - Call generated normalization functions and `RiskVector::clamped()`.  
    - Assert:
      - All `r_j ∈ [0,1]`.  
      - Residual \(V_t ≥ 0\).  
      - `R` score ∈ \([0,1]\). [file:1][file:2][file:25]  
  - KerDeployable invariants:
    - Under ALN-specified lane thresholds (e.g., PROD `K ≥ 0.90, E ≥ 0.90, R ≤ 0.13`), assert:
      - `KerDeployable::is_deployable()` never returns true when any non-offsettable plane worsens or `ΔV_t > 0`. [file:14][file:25]  
    - Conversely, show that for inputs respecting all corridor constraints, KER gates behave monotonically (no silent weakening). [file:25]  
  - Tightest-band enforcement:
    - For coordinates with multiple band definitions across versions, prove:
      - Residual and KER computed under macro-selected bands are ≤ any residual under looser bands; i.e., tightening cannot reduce safety. [file:26][file:25]  
- Domain-specific harnesses:
  - Drainage-decay:
    - `riskclampingandresidualstability` already implemented in the 2026‑07‑08 crate, proving BOD/TSS/CEC risk clamping and non-negative \(V_t\). [file:2]  
  - Workload energy:
    - `workloadriskclampingandresidualsafety` harness ensures energy/hydraulic/uncertainty risks stay within \([0,1]\), residual ≥ 0, K,E,R ∈ \([0,1]\). [file:1]  
  - Future crates (e.g. `kerresidual-kani-tests`):
    - Centralize KER-invariant harnesses over shared engines, referenced by daily domain crates via `aln_shard`. [file:14][file:25]  

## 5. CI Flow: `cargo kani` as Corridor Gatekeeper

- CI entrypoint:
  - For all SPINE, ecosafety, and cyboquatic crates, CI runs:
    - `cargo build` (with `aln_shard` macro enforcing schema and band bindings). [file:26][file:14]  
    - `cargo kani` (or Kani runner) targeting:
      - Shared harness crate (e.g. `kerresidual-kani-tests`).  
      - Each domain crate’s generated `verification` module. [file:1][file:2][file:25]  
- Fail conditions:
  - Any `#[kani::proof]` harness fails (e.g., normalization can produce `r_j > 1`, `KerDeployable` can incorrectly accept a corridor-violating state). [file:25]  
  - Any tightening of corridor bands or plane weights in `.aln` that:
    - Increases \(V_t\) for admissible states without corresponding KER justification in `virtaupgradeledger`. [file:14][file:25]  
    - Weakens non-offsettable planes or raises RoH ceilings, violating “monotone evolution” and “no corridor widening” rules. [file:4][file:25]  
- Pass conditions:
  - All `aln_shard` bindings succeed; all types are schema-complete and corridor-bound. [file:26]  
  - All Kani proofs hold; all KER and Lyapunov invariants are preserved across crate changes. [file:1][file:2][file:25]  
  - DefinitionRegistry coverage:
    - CI helper `check-definition-registry` verifies every new `.aln`, `.sql`, `.rs` artifact under governance bands has a `definitionregistry` row and KER targets. [file:14]  
- Resulting architecture:
  - ALN contracts become executable policy:
    - `.aln` → `aln_shard` macro → Rust types + normalization kernels → auto-generated Kani proofs → `cargo kani` gating merges. [file:26][file:25][file:14]  
  - Any attempt to weaken corridors, relax KER thresholds, or introduce schema drift is stopped at compile-time or proof-time, before it can reach machinery or carbon-impact calculations. [file:4][file:25]  
