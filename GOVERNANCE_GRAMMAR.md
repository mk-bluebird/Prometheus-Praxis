<!-- File: GOVERNANCE_GRAMMAR.md
     Destination: repo root (e.g., /GOVERNANCE_GRAMMAR.md)
     License: MIT OR Apache-2.0 -->

# Prometheus‑Praxis Governance Grammar

This document defines the frozen governance grammar for the Prometheus‑Praxis ecosystem. It is the single reference for how telemetry, KER coordinates, Lyapunov residuals, operational lanes, and hex anchoring are represented and enforced across all subsystems.

All C++ headers and Rust crates in the repository must conform to this grammar when producing or consuming governance‑related data.

## Core Concepts

- Telemetry domains:
  - WastewaterPump
  - Shredder
  - Hammermill
  - Conveyance
  - Magnet
  - Unknown

- Non‑actuating guarantee:
  - C++ modules in `src/cpp/waste/*` must not perform hardware actuation.
  - They ingest physical sensor streams, normalize them into risk coordinates, and expose POD structures and risk slices.
  - Rust governance crates apply KER and Lyapunov logic and record evidence in databases and ALN particles.

## MachineTelemetry

`MachineTelemetry` is the unified telemetry surface for governance.

Fields:

- `machine_id` (string): unique identifier for the machine asset.
- `station_id` (string): identifier for the station or corridor.
- `domain` (enum TelemetryDomain): physical domain of the machine.
- `timestamp_utc` (string): ISO‑8601 timestamp.

Risk planes (all in `[0,1]`):

- `r_hydraulics`: normalized hydraulic risk (pumps).
- `r_energy`: normalized energy/carbon risk.
- `r_uncertainty`: telemetry health and uncertainty.
- `r_reliability`: reliability risk (vibration, temperature, starts/run‑hours).
- `r_extra_1`, `r_extra_2` (optional): domain‑specific additional planes.

Global scalar:

- `roh` (Risk‑of‑Harm): scalar operational ceiling in `[0,1]`.

Lyapunov fields:

- `vt_current`: current Lyapunov residual value.
- `vt_next`: next Lyapunov residual value.

Rules:

- All risk planes and `roh` must be clamped into `[0,1]` before reaching Rust.
- C++ kernels must ensure no NaNs or invalid ranges.

## KER Coordinates

`KerCoordinates` represent the Knowledge‑Ecoimpact‑Risk triad for a telemetry window.

Fields:

- `k_knowledge` (`K`): data quality, sensor fidelity, window completeness.
- `e_eco_impact` (`E`): ecological benefit or efficiency.
- `r_risk` (`R`): operational hazard and potential harm.

All three are normalized scalars in `[0,1]`.

Example scalar score:

- `KER Score = K * E - R`

This score can be used for ranking, but the raw K/E/R coordinates must be preserved for governance decisions.

## Lyapunov Residual

Lyapunov residuals track dynamic system drift and stability.

Definition:

- `V_t` is a quadratic function over risk coordinates:
  - `V_t = sum_i w_i * r_i^2`, where `r_i ∈ [0,1]` and `w_i` are non‑offsettable weights.
- `ΔV_t = V_t_next - V_t_current`.

In the SDK:

- `LyapunovResidual` contains:
  - `vt_current`
  - `vt_next`
  - `delta_vt = vt_next - vt_current`

Rules:

- Negative or small positive `delta_vt` indicates dissipating or bounded risk.
- Large positive `delta_vt` indicates accumulating risk and potential instability.

## Operational Lanes

Operational lanes are encoded in `ActionLane` and `LaneConfig`.

Lanes:

- `Research`
- `Pilot`
- `Production`

Blocked behavior:

- There is no explicit `Blocked` lane enum; blocked states are represented by `LaneAction::Halt` decisions.

Lane semantics:

- Research:
  - Wide risk bounds.
  - Lyapunov thresholds are permissive.
  - Exploration and manual overrides may be allowed.
- Pilot:
  - Intermediate bounds.
  - Lyapunov residuals are guarded.
  - Configuration is tuned for near‑real deployment testing.
- Production:
  - Strict bounds.
  - Small allowable `delta_vt`.
  - RoH ceilings are conservative.

Lane configuration:

- `LaneConfig` provides:
  - `roh_ceiling_global` (max allowable RoH).
  - `max_delta_vt` (max allowable `delta_vt`).
  - Lane‑specific minimum Knowledge and Eco‑impact and maximum Risk:
    - `k_min_research`, `k_min_pilot`, `k_min_prod`.
    - `e_min_research`, `e_min_pilot`, `e_min_prod`.
    - `r_max_research`, `r_max_pilot`, `r_max_prod`.

No code divergence:

- All lane semantics are implemented via `LaneConfig` thresholds and the unified `decide_lane` function.
- There must be no separate governance logic per lane.

## Governance Gates

`GovernanceGateConfig` encodes resolved metadata and gating flags.

Fields:

- `corridor_available` (bool):
  - Implementing the “no corridor → no build” rule.
- `manual_override_allowed` (bool):
  - Whether manual overrides are permitted for the current window and lane.
- `allow_research_exploration` (bool):
  - Whether Research lane can be treated more permissively when Lyapunov residuals exceed standard bands.

Rules:

- If `corridor_available` is false:
  - `decide_lane` must produce `LaneAction::Halt`, regardless of telemetry.
- If `roh` exceeds `roh_ceiling_global`:
  - `decide_lane` must produce `LaneAction::Halt`.
- If `delta_vt` exceeds `max_delta_vt`:
  - Research lane may Derate if exploration is allowed.
  - Pilot lane must at least Derate.
  - Production lane must Halt.

## Lane Decisions

`LaneDecision` is the canonical governance output for a telemetry window.

Fields:

- `lane` (ActionLane): effective lane at decision time.
- `action` (LaneAction): `Proceed`, `Derate`, or `Halt`.
- `reason_code` (string): short machine‑readable reason for the decision.
- `vt_current`, `vt_next` (Lyapunov values).
- `roh`, `k`, `e`, `r`: scalars used in the decision.

Rules:

- `Proceed`:
  - All governance gates passed.
  - K/E/R meet lane minima/maxima.
  - RoH and Lyapunov residual inside configured bounds.
- `Derate`:
  - A governance gate indicates elevated but bounded risk.
  - Lane thresholds for K/E/R are violated within tolerable bands.
- `Halt`:
  - Corridor missing, RoH ceiling breached, or Lyapunov residual too high.
  - Validation errors in telemetry or configuration.

## Hex Anchoring and Provenance

Evidence records (windows and decisions) must be bound to cryptographic identifiers.

Requirements:

- Each window’s decision must be:
  - Serialized into SQLite views (such as `v_shard_ker`, `v_shard_residual`) with full K/E/R, RoH, and Lyapunov data.
  - Emitted as ALN particles whose schema matches the SDK types.
- ALN particles must:
  - Carry hex strings representing authorship and corridor anchoring.
  - Reference Bostrom DIDs associated with the project.

Paths:

- SQLite:
  - Tables should mirror `MachineTelemetry` and `LaneDecision` fields.
  - `CHECK` constraints must enforce `[0,1]` ranges for risk planes and scalars.
- ALN:
  - Particle definitions should mirror the SDK structs and lane enums.
  - Code generation tooling is recommended to keep schemas synchronized.

## Subsystem Directory Alignment

Each subsystem directory under `src/cpp/waste/` must conform to the governance grammar.

- `src/cpp/waste/shredding/`:
  - Shredder and hammermill controllers produce:
    - Domain‑specific windows with throughput, energy, product size, moisture, holdup.
    - Normalized K/E/R coordinates and Lyapunov slices.
  - They must expose POD structs and C‑ABI functions that feed `CMachineTelemetry` and `CKerCoordinates`.

- `src/cpp/waste/conveyance/`:
  - Conveyor graphs and magnet modules produce:
    - Routing envelopes and risk coordinates.
    - KER and Lyapunov data for material paths.
  - They must conform to the same risk normalization and decision surface.

- `src/cpp/waste/wastewater/`:
  - Pump, sump, and station modules produce:
    - Normalized hydraulic, energy, uncertainty, and reliability planes.
    - Lyapunov residuals over these planes.
  - Deployment adapters must either:
    - Use POD FFI with `prometheus_praxis_decide_lane`.
    - Or serialize for human‑readable logging only.

## Implementation Expectations

- All Rust governance crates must include:
  - `#![forbid(unsafe_code)]` at the crate level.
  - Kani harnesses asserting lane invariants and gating rules.
- All C++ modules must:
  - Avoid hardware actuation.
  - Provide clean, deterministic numeric kernels.
  - Use POD structs and C‑ABI where performance requires binary FFI.

This grammar is intentionally strict. It is designed to ensure that all telemetry, risk evaluations, and governance decisions across the Prometheus‑Praxis ecosystem are consistent, verifiable, and safe by construction.
