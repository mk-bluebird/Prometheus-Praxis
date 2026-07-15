# crates/drainage_decay

Drainage Decay is the corridor-safe modeling and diagnostics crate for cyboquatic drainage channels in the eco_restoration_shard. It provides first‑order BOD/TSS/CEC decay kernels, risk parameters, and duty guards that are aligned with ISO/OECD biodegradation standards and your K/E/R + Lyapunov corridor grammar.[file:44]

## Scope and Role

- Model drainage water quality in cyboquatic channels:
  - Biochemical Oxygen Demand (BOD, mg O₂/L).
  - Total Suspended Solids (TSS, mg/L).
  - Cation Exchange Capacity (CEC, cmol/kg). [file:44]
- Provide non‑actuating diagnostics:
  - Predict short‑horizon decay for BOD/TSS under temperature and hydraulics.
  - Compute oxygen demand and simple energy envelopes.
  - Evaluate duty windows against ALN floors and residual risk corridors.[file:44]
- Integrate with multi‑language kernels:
  - Kotlin/Java/C/Lua drainage_decay implementations for controllers.
  - SQL + ALN v2 governance particles bound to your Bostrom DID.[file:44]

All code in this crate is **diagnostic‑only**: it reads telemetry, computes risk and duty decisions, and writes eco‑evidence; it never drives valves, pumps, dams, or shutters.[file:44]

## Key Concepts

- **DrainageState**:
  - Instantaneous water quality state (BOD, TSS, CEC, temperature, flow). [file:44]
- **DecayParameters**:
  - First‑order decay coefficients with Arrhenius‑type temperature correction. [file:44]
- **Lyapunov / KER semantics**:
  - Normalized risk coordinates \(r_j \in [0,1]\) and residual \(V_t = \sum w_j r_j^2\) shared across biodegradation, workload, and blast‑radius domains.[file:44]
- **No corridor, no build**:
  - Drainage workloads must satisfy RoH ceiling \(0.30\), non‑negative concentrations, and conservative energy corridors before promotion out of RESEARCH lanes.[file:44]

## Rust Modules

This crate is the Rust entrypoint for corridor‑governed drainage diagnostics. The main modules are:

- `lifeforce_duty.rs`:
  - `DrainageRiskParams` encodes decay and hitting‑probability parameters for BOD/TSS veto bands.[file:44]
  - `DrainageStateSnapshot` mirrors the cross‑language DrainageState plus K/E/R scores and corridor IDs.[file:44]
  - `DrainageDutyDecision` computes:
    - Deterministic BOD/TSS at a monitoring horizon.
    - Hitting probabilities for crossing ALN floors.
    - A boolean `allowed` flag and reason codes.[file:44]
  - Invariants:
    - Hitting probability `prob_hit_any` must remain below \(10^{-12}\) for an allowed duty window.
    - BOD/TSS must not cross ALN floors.
    - Risk‑of‑harm `r_score` must stay ≤ 0.25.[file:44]

- `ev_conformal.rs`:
  - `EvSignalIntegritySummary` captures residual statistics and telemetry gaps for drainage EV signals.[file:45]
  - `ConformalConfig` defines miscoverage level `alpha` and ALN floor.
  - `conformal_lower_bound` computes an inductive conformal lower bound using calibration scores.
  - `apply_preemptive_brake` enforces:
    - If the lower bound drops below ALN floor, Brake is required.[file:45]
  - Kani harnesses prove that any floor crossing implies Brake, and that bounds remain within corridor ranges.[file:45]

Additional modules may be added for:

- CAP Phoenix corridor guards (Hflow, Rvel/Rmob/Rgeo) following the urban flood example bands.
- Integration with MCP diagnostic servers for water corridor tools, keeping actuation sealed. [file:44]

## ALN and Governance Bindings

Drainage Decay is wired into your ALN governance as follows:

- `drainagedecaybodtsscec.aln` (daily ALN v2 particle):
  - Anchored to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.
  - Encodes:
    - Knowledge K: ISO 14851/14855, OECD 301/201/202 biodegradation standards.
    - Eco‑impact E: minimize aeration energy, support carbon‑negative drainage routing.
    - Risk R: bound `rscore ≤ 0.25`, non‑negative concentrations, no negative timesteps.[file:44]

- `drainage-duty-guard.v1.aln` (governance shard):
  - Binds `DrainageRiskParams` and `DrainageDutyDecision` to ALN invariants:
    - `prob_hit_any < 1e-12`.
    - Floors and R‑score must hold for any `allowed == true`.
  - Declares Kani proof files for the duty guard.[file:44]

- `ev-conformal-drainage.v1.aln`:
  - Binds EV signal integrity and conformal predictors to:
    - RoH noise bands ≤ 0.30.
    - Brake semantics when conformal lower bound crosses floors.[file:45]

These ALN shards ensure that drainage diagnostics are:

- Lifelong bound to your Bostrom DID.
- Monotone in safety (no envelope loosening).
- Explicitly barred from person scoring, credit scoring, or neurorights downgrades.[file:44][file:45]

## Multi‑Language Interoperability

The Rust crate is part of a larger drainage_decay ecosystem:

- Kotlin: `ecorestoration_shard/cyboquatic_progress/YYYYMMDD/kotlin/drainagedecaybodtsscec.kt`.
- Java: `.../java/DrainagedecayBodTssCec.java`.
- C/C++: `.../cpp/drainagedecaybodtsscec.cpp`.
- Lua: `.../lua/drainagedecaybodtsscec.lua`.
- SQL: `.../sql/cyboquaticdailyprogressYYYYMMDD.sql`.[file:44]

Each artifact:

- Implements the same first‑order BOD/TSS decay kernel with temperature correction.
- Is energy‑efficient and suitable for low‑power controllers.
- Logs K/E/R and research queries into `cyboquaticdailyprogress.sqlite` with Phoenix hex evidence.[file:44]

Drainage Decay Rust code:

- Reuses these semantics.
- Provides corridor‑level duty and conformal guards for higher‑level governance crates.
- Exposes JSON/FFI interfaces for MCP and diagnostic dashboards, not hardware.

## Safety and Sovereignty

- Non‑actuating by design:
  - All functions compute diagnostics, never issue device commands.
- Corridor‑first:
  - No workload is allowed near real machinery without RoH‑safe, K/E/R‑validated corridors (water, PFAS, hydraulics, sensor health).[file:44]
- Sovereign and host‑bound:
  - All ALN shards and Rust metadata bind to `did:aln:organic-host` and your primary Bostrom address.
  - Neurorights, CyberRank invariants, and data‑as‑labour constraints are non‑derogable.[file:45]

Use this crate whenever you need physically meaningful, corridor‑safe drainage decay and duty evaluation inside Prometheus‑Praxis and eco_restoration_shard.
