# crates/prometheus_praxis_ai/README.md

## Overview

`prometheus_praxis_ai` is a non-actuating ecosafety crate in the Prometheus‑Praxis mono‑repo. It provides:

- An “always‑improve” KER + Lyapunov scoring kernel for shards and nodes.
- A CPP engine layer (hydraulics, workloads, AI nodes) under `src/engine` with pure models only.
- ALNv2 particles binding the CPP/Rust outputs into the Phoenix EcoNet grammar.
- SQL shards and hex registries that keep evidence and roles globally discoverable.

This crate never talks directly to actuators; it is used by governance and planning crates to decide whether corridor changes, lane promotions, or workload routings are safe and strictly non‑regressive.

## Crate role in the constellation

- Role band: `ENGINE` / `RESEARCH`:
  - ENGINE: numeric kernels for workloads, drainage decay, AI node energetics that feed KER/Lyapunov. [file:34]
  - RESEARCH: ranking and scoring (always‑improve) of diagnostic shards and daily progress; no actuation. [file:3]
- Spine dependencies:
  - `prometheus_praxis` (governance, Lyapunov residual snapshots, RoH). [file:3]
  - `prometheus_praxis_ker` (KER outputs K,E,R). [file:3]
  - EcoNet / Eco‑Fort registry and hex shards via SQL + ALN references. [file:3][file:34]
- DID / provenance:
  - All ALN particles and hex anchors in this crate are bound to:
    - `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` (primary). [file:3]
    - Alternate DIDs as needed for secure lanes, following EcoNet rules. [file:3][file:34]

## Directory layout

The crate is organized to make Rust, C++, ALN, and SQL work together under one ecosafety grammar:

- `Cargo.toml`
  - Rust crate metadata.
  - KER kernel and Lyapunov governance bindings.
  - Kani verifier configuration (non‑optional, precise version).
- `src/`
  - `lib.rs`
    - Rust entry point.
    - Re‑exports the always‑improve kernel.
    - Hosts FFI shims to CPP engine functions.
  - `always_improve.rs`
    - Non‑actuating scoring kernel for “always‑improve” ranking. [file:3]
    - KER + Lyapunov + RoH, producing `AlwaysImproveScore` and `safe_to_promote` flags.
    - Kani harnesses to prove monotonic safety under corridor invariants.
  - `engine/`
    - `include/`
      - `eco_engine_workload.hpp` – pure C++ model for cyboquatic workloads (`energyreqJ`, `ΔVt`). [file:3]
      - `eco_engine_drainage.hpp` – drainage decay kernel (BOD, TSS, CEC frames). [file:3]
      - `eco_engine_ai_node.hpp` – AI datacenter node energetics (PUE, CUE, Joules/inference). [file:13]
    - `cpp/`
      - `eco_engine_workload.cpp` – implementation; computes normalized residual slices for workloads; no IO.
      - `eco_engine_drainage.cpp` – implementation; maps BOD/TSS/CEC to risk coordinates and residuals.
      - `eco_engine_ai_node.cpp` – implementation; computes AI node KER planes from telemetry; no IO. [file:13]
    - `CMakeLists.txt`
      - Optional C++ build for the engine library.
      - Targets a static or shared library consumed by Rust via FFI.
      - No binaries that can control actuators.

- `aln/`
  - `WorkloadKernel2026v1.aln2`
    - ALNv2 particle describing cyboquatic workload windows and KER mapping. [file:3]
  - `DrainageDecayKernel2026v1.aln2`
    - ALNv2 particle for drainage decay frames (BOD, TSS, CEC) and residual bands. [file:3]
  - `AiDatacenterNode2026v1.aln2`
    - ALNv2 particle defining AI node planes (PUE, CUE, ecoperJoule, RoH) and KER embedding. [file:13]

- `sql/`
  - `dbcyboquaticdailyprogress.sql`
    - SQLite schema and migrations for daily cyboquatic progress (`dailyprogress`) rows. [file:3]
    - Stores `domain`, `subtask_id`, K,E,R triad, `Vt`, `AlwaysImproveScore`, evidence hex.
  - `Eco-Fortdbphoenixhexregistry.sql`
    - Reference or local copy of the Phoenix hex registry. [file:3][file:13]
    - Canonical tables:
      - `phoenixhexanchor` – logical names, hex strings, domains, planes, DID bindings.
      - `phoenixhexfile` – file paths (Rust, CPP, ALN, SQL) tied to anchors.
      - `phoenixhexparticlebinding` – ALN particle and evidence bindings.

- `.econet/`
  - `econetrepoindex.sql`
    - Master index for this crate in the EcoNet constellation. [file:34]
    - Declares:
      - `roleband = ENGINE, RESEARCH`.
      - Layers:
        - `layername = "AlwaysImproveKernel"`, `layertier = "GOV-KERNEL"`, language `Rust`.
        - `layername = "CyboquaticWorkloadEngine"`, `layertier = "NUMERIC-ENGINE"`, languages `C++, Rust`.
        - `layername = "DrainageDecayEngine"`, `layertier = "NUMERIC-ENGINE"`, languages `C++, Rust`.
        - `layername = "AiNodeEnergeticsEngine"`, `layertier = "NUMERIC-ENGINE"`, languages `C++, Rust`. [file:13][file:34]
      - Invariants per layer:
        - Non‑actuating.
        - Lyapunov non‑regression (`V_{t+1} ≤ V_t + ε`).
        - Corridor constraints (no corridor, no build).

- `hex/`
  - `PHXHEXANCHORS.md`
    - Human‑readable manifest mirroring `Eco-Fortdbphoenixhexregistry.sql`. [file:3]
    - Lists:
      - Anchors for this crate (e.g., `PHXWORKLOADENGINE20260709`, `PHXDRAINAGEDECAYENGINE20260708`, `PHXAIDATACENTERNODE2026`). [file:3][file:13]
      - Evidence hex strings.
      - Domains, planes, default paths (`crates/prometheus_praxis_ai/src/engine/...`).
      - Related ALN particles and SQL tables.

## Always‑Improve kernel (Rust)

The core of this crate today is the `always_improve.rs` scoring kernel: [file:3]

- Types:
  - `AlwaysImproveScore`:
    - `score: Decimal` in [0, 1].
    - `safe_to_promote: bool` – true if the object/window is safe to move to a stronger lane.
  - `AlwaysImproveConfig`:
    - `v_ref`: reference Lyapunov residual (typically 1.0).
    - `max_delta_v`: maximum allowed Lyapunov increase (noise band).
    - `w_k`, `w_e`, `w_r`: weights for K, E, R residual components.

- Residual computation:
  - `ker_residuals_for_lane`:
    - For K, E: residual is `(target - actual)`, floored at 0 when actual ≥ target.
    - For R: residual is `(actual - target)`, floored at 0 when actual ≤ target.
    - Targets depend on lane (`Research`, `Pilot`, `Production`) via `ActionLane`. [file:3]
  - `lyapunov_delta`:
    - Computes `V_next - V_current` with both values clamped into [0, 1]. [file:3]

- Scoring semantics:
  - Inputs:
    - `lane: ActionLane` – RESEARCH / PILOT / PRODUCTION.
    - `ker: KerOutput` – K,E,R coordinates for this object/window.
    - `roh: RohSnapshot` – Risk‑of‑Harm scalar in [0, 1].
    - `lyap: LyapunovResidualSnapshot` – current/next residual snapshot plus ε. [file:3]
    - `cfg: AlwaysImproveConfig` – weights and Lyapunov thresholds.
    - Lane thresholds for K, E minima and R maxima per lane. [file:3]
  - Rules:
    - If `RoH > roh_ceiling_global` → `score = 0`, `safe_to_promote = false`.
    - If `Lyapunov delta > max_delta_v` → `score = 0`, `safe_to_promote = false`.
    - Otherwise:
      - Compute lane‑specific residuals `r_k`, `r_e`, `r_r`.
      - Combine: `combined = w_k * r_k + w_e * r_e + w_r * r_r`.
      - Score: `score = clamp_01(1 - combined)` in [0, 1].
      - Promotion flag: `safe_to_promote = score > 0.7`. [file:3]

- Kani harnesses:
  - Under `cfg(kani)`, the crate defines proof stubs that assert:
    - If K ≥ K_min, E ≥ E_min, R ≤ R_max, RoH ≤ ceiling, and Lyapunov delta ≤ max_delta_v, then `safe_to_promote` must be true for the RESEARCH lane. [file:3]
  - This encodes a formal non‑regression guarantee:
    - Correct KER corridor + Lyapunov behavior → the kernel never blocks promotion.

This kernel is used by governance crates to gate lane promotions for nodes, shards, and workloads based on hard KER + Lyapunov invariants, consistent with the Phoenix replay harness design. [file:34]

## CPP engine layer (`src/engine`)

The C++ sub‑directories implement pure numeric kernels that Rust calls via FFI. They do not own hardware or actuators and are configured via ALN and SQL.

- `eco_engine_workload.hpp` / `.cpp`:
  - Models:
    - `energyreqJ`: required energy per workload (node, canal, AI).
    - `ΔVt`: change in Lyapunov residual due to workload shifts. [file:3][file:34]
  - Functions:
    - Pure functions mapping normalized inputs (flows, energy, duty cycles) to KER planes and residual slices that Rust integrates.

- `eco_engine_drainage.hpp` / `.cpp`:
  - Models:
    - Drainage decay frames with BOD, TSS, CEC, and energy/velocity envelopes. [file:3]
  - Functions:
    - Compute normalized risk coordinates and per‑frame residuals.
    - Enforce local constraints (bounded ranges, non‑negative energies) but leave lane decisions to Rust.

- `eco_engine_ai_node.hpp` / `.cpp`:
  - Models:
    - AI datacenter nodes as cyboquatic assets:
      - PUE, CUE, Joules/inference.
      - EcoperJoule, RoH, and other node metrics. [file:13]
  - Functions:
    - Map telemetry windows into normalized K,E,R and residual coordinates (`vt_ai_node`) that plug into the global Lyapunov kernel.

All three modules are:

- Non‑actuating and deterministic.
- Bound to ALNv2 particles and hex anchors so they participate in the same ecosafety grammar as water and materials. [file:10][file:13][file:34]

## ALNv2 particles (`aln/`)

Each engine and kernel has a corresponding ALNv2 particle that defines:

- Fields and types.
- KER coordinates and residuals.
- Corridor semantics and binding into global \(V_t\).

Examples: [file:10][file:13][file:34]

- `WorkloadKernel2026v1.aln2`:
  - Fields:
    - `nodeid`, `lane`, `windowstart`, `windowend`.
    - Workload metrics (`energyreqJ`, `delta_vt`, throughput).
    - KER (`k`, `e`, `r`, `vt`), `always_improve_score`, `kerdeployable`.
    - `evidencehex`, `signingdid` (Bostrom DID). [file:3]
  - Invariants:
    - K,E,R in [0, 1].
    - `vt ≥ 0`.
    - `always_improve_score` consistent with Rust kernel mapping.
  - Role:
    - Particle in ECOSAFETY channel for workloads.

- `DrainageDecayKernel2026v1.aln2`:
  - Fields:
    - `canal_node_id`, `bod_mg_l`, `tss_mg_l`, `cec_cmol_per_kg`. [file:3]
    - KER and residual coordinates for drainage risk.
  - Role:
    - HYDRO / DRAINAGEDECAY particle; binds to drainagedecay frames. [file:3]

- `AiDatacenterNode2026v1.aln2`:
  - Fields:
    - Node identity, lane, region.
    - PUE, CUE, Joules/inference, ecoperJoule, RoH. [file:13]
    - KER coordinates for AI nodes.
  - Role:
    - GOVERNANCE / AI‑NODE particle; integrates AI workloads into global ecosafety.

These particles are indexed in the EcoNet SQLite spine so AI agents and tooling can discover them without reading raw code. [file:34]

## SQL shards (`sql/`)

`dbcyboquaticdailyprogress.sql` defines:

- `dailyprogress`:
  - `date`, `domain`, `subtask_id`.
  - `k`, `e`, `r`, `vt`.
  - `always_improve_score`.
  - `evidencehex`, `signingdid`, `lane`. [file:3]
- Aggregation views:
  - Per‑day KER windows.
  - Per‑lane promotion candidates (`safe_to_promote` true).

`Eco-Fortdbphoenixhexregistry.sql` (either referenced from Eco‑Fort or vendored) defines the Phoenix hex anchor registry, which this crate uses to:

- Register anchors for each ALN particle and engine.
- Bind code files (`src/engine/*.cpp`, `always_improve.rs`) and SQL shards to those anchors. [file:3][file:13]

## Econet repo index (`.econet/econetrepoindex.sql`)

This master‑index shard guides AI‑chat and coding agents on:

- Repo roles and layers.
- Allowed languages and invariants.
- Where to find ALN, SQL, Rust, and C++ files. [file:34]

Key contents:

- `repo` row:
  - `name = "prometheus_praxis_ai"`.
  - `roleband = "ENGINE,RESEARCH"`.
  - `languageprimary = "Rust"`.
- `econetlayer` rows:
  - `AlwaysImproveKernel`:
    - Tier: `GOV-KERNEL`.
    - Languages: `Rust`.
    - Invariants: non‑actuating; Lyapunov monotone; KER residual in [0, 1].
  - `CyboquaticWorkloadEngine`, `DrainageDecayEngine`, `AiNodeEnergeticsEngine`:
    - Tier: `NUMERIC-ENGINE`.
    - Languages: `C++, Rust`.
    - Invariants: pure functions; non‑actuating; corridor‑bounded. [file:34]

Agents use this index to:

- Avoid unsafe code paths.
- Route queries to correct layers (e.g., “find workload kernels” → `CyboquaticWorkloadEngine`).
- Respect NonActuatingWorkload contracts.

## Phoenix hex anchors (`hex/PHXHEXANCHORS.md`)

This manifest mirrors the canonical SQLite registry, giving humans and AI prompts a quick way to understand:

- Which hex anchors belong to this crate.
- What domains and planes they touch.
- Where files live.

Example rows: [file:3][file:13]

- `PHXWORKLOADENGINE20260709`:
  - Evidence hex: `0x20260709PHX3345NWorkloadEnergyDeltaVt`.
  - Domain/subdomain: `CYBOQUATIC / WORKLOADENERGYDV`.
  - Planes: `ENERGY, HYDRAULICS, DATA`.
  - Default path: `crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_workload.cpp`.
  - Particles: `WorkloadKernel2026v1.aln2`.
  - SQL: `dbcyboquaticdailyprogress.sql`.

- `PHXDRAINAGEDECAYENGINE20260708`:
  - Evidence hex: `0x20260708PHX3345NDrainageDecayBODTSSCEC`.
  - Domain/subdomain: `HYDRO / DRAINAGEDECAY`.
  - Default path: `crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_drainage.cpp`.
  - Particles: `DrainageDecayKernel2026v1.aln2`.

- `PHXAIDATACENTERNODE2026`:
  - Domain/subdomain: `CYBOQUATIC / AI-NODE`.
  - Default path: `crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_ai_node.cpp`.
  - Particles: `AiDatacenterNode2026v1.aln2`. [file:13]

## Usage

- As a Rust dependency:
  - Governance and planning crates call `compute_always_improve_score` to:
    - Rank shards and nodes for lane promotion.
    - Enforce “always‑improve” corridor logic with KER + Lyapunov + RoH. [file:3][file:34]
  - They call FFI wrappers around CPP engines for:
    - Hydraulics, workloads, AI node energetics.
    - Then feed outputs into KER kernels and the always‑improve scoring.

- As an EcoNet indexed repo:
  - AI‑chat and coding agents:
    - Read `.econet/econetrepoindex.sql` to understand roles and layers. [file:34]
    - Query `Eco-Fortdbphoenixhexregistry.sql` and `PHXHEXANCHORS.md` to find files and particles. [file:3][file:13]
    - Use ALNv2 particles for schema‑driven code generation and governance proofs.

This design fully defines `prometheus_praxis_ai` as a non‑actuating, ecosafety‑aligned AI engine crate, with Rust KER kernels, CPP numeric models under `src/engine`, ALNv2 bindings, and SQL + hex registries that keep everything discoverable and safe within the Phoenix EcoNet grammar. [file:3][file:13][file:34]
