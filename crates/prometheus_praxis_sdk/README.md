# Prometheus‑Praxis SDK

`prometheus_praxis_sdk` is the unified governance SDK for the Prometheus‑Praxis ecosystem. It defines the frozen grammar for:

- Telemetry surfaces (`MachineTelemetry`)
- KER coordinates (`KerCoordinates`)
- Lyapunov residuals (`LyapunovResidual`)
- Lane configuration and governance gates (`LaneConfig`, `GovernanceGateConfig`)
- Lane decisions (`LaneDecision`)
- C‑ABI FFI bindings for high‑frequency integration with C++ numeric kernels

This crate is strictly non‑actuating. It evaluates telemetry and produces governance decisions and evidence. Hardware actuation must be implemented in separate stacks, under explicit governance control.

## Crate goals

- Provide a single, shared SDK for all domains (wastewater, shredding, hammermill, conveyance, magnet).
- Encode lane semantics (Research, Pilot, Production) via configuration only.
- Enforce memory safety with `#![forbid(unsafe_code)]`.
- Support formal verification of lane invariants with Kani.
- Offer C‑ABI FFI so C++ pipelines can pass telemetry and KER data without JSON overhead.

## Directory layout

- `src/lib.rs`  
  Crate root; exposes `lanes`, `ffi`, and `kani_harnesses` modules.

- `src/lanes.rs`  
  Core governance types and logic:
  - `TelemetryDomain`
  - `MachineTelemetry`
  - `KerCoordinates`
  - `LyapunovResidual`
  - `ActionLane`
  - `LaneConfig`
  - `GovernanceGateConfig`
  - `LaneAction`
  - `LaneDecision`
  - `decide_lane` (unified governance gate)

- `src/ffi.rs`  
  C‑ABI FFI bindings:
  - `CMachineTelemetry`
  - `CKerCoordinates`
  - `CLaneConfig`
  - `CGovernanceGateConfig`
  - `CLaneDecision`
  - `prometheus_praxis_decide_lane` (extern "C")

- `src/kani_harnesses.rs`  
  Kani proof harnesses asserting governance invariants:
  - `kani_production_lane_never_halts_when_inside_bounds`
  - `kani_no_corridor_always_halts`

## Key concepts

### TelemetryDomain

`TelemetryDomain` is an enum identifying the physical subsystem:

- `WastewaterPump`
- `Shredder`
- `Hammermill`
- `Conveyance`
- `Magnet`
- `Unknown`

Domain‑specific C++ modules are responsible for mapping their telemetry into domain‑appropriate risk planes and Lyapunov slices before feeding `MachineTelemetry`.

### MachineTelemetry

`MachineTelemetry` is the unified telemetry surface consumed by lane governance.

Fields:

- `machine_id`: unique machine asset ID.
- `station_id`: station or corridor identifier.
- `domain`: `TelemetryDomain` value.
- `timestamp_utc`: ISO‑8601 timestamp string.

Risk planes (unit interval):

- `r_hydraulics`
- `r_energy`
- `r_uncertainty`
- `r_reliability`
- `r_extra_1`, `r_extra_2` (optional domain‑specific planes)

Global scalar:

- `roh`: Risk‑of‑Harm scalar in `[0,1]`.

Lyapunov fields:

- `vt_current`
- `vt_next`

All planes and scalars must be clamped into `[0,1]` (where applicable) by C++ kernels before crossing into Rust.

### KerCoordinates

`KerCoordinates` represent the Knowledge‑Ecoimpact‑Risk triad for a telemetry window:

- `k_knowledge`
- `e_eco_impact`
- `r_risk`

Each component is a `Decimal` in `[0,1]`. Domain‑specific KER kernels compute these from `MachineTelemetry` and additional context.

### LyapunovResidual

`LyapunovResidual` captures stability information:

- `vt_current`
- `vt_next`
- `delta_vt = vt_next - vt_current`

Lyapunov residuals are quadratic forms over risk planes. Negative or small positive `delta_vt` indicates acceptable drift; large positive `delta_vt` triggers lane gates.

### ActionLane and LaneConfig

`ActionLane` encodes operational lanes:

- `Research`
- `Pilot`
- `Production`

`LaneConfig` sets lane semantics via thresholds:

- `roh_ceiling_global`: maximum allowable RoH.
- `max_delta_vt`: maximum allowable Lyapunov residual increase.
- Lane‑specific thresholds:
  - `k_min_research`, `k_min_pilot`, `k_min_prod`
  - `e_min_research`, `e_min_pilot`, `e_min_prod`
  - `r_max_research`, `r_max_pilot`, `r_max_prod`

All thresholds are `Decimal` values in `[0,1]`. There are no lane‑specific code branches; all differences arise from `LaneConfig`.

### GovernanceGateConfig

`GovernanceGateConfig` encodes resolved metadata and gating flags:

- `corridor_available`: implements “no corridor → no build”.
- `manual_override_allowed`: whether manual overrides are permissible.
- `allow_research_exploration`: whether Research lane may be treated more permissively when Lyapunov residuals exceed standard bands.

### LaneAction and LaneDecision

`LaneAction`:

- `Proceed`
- `Derate`
- `Halt`

`LaneDecision`:

- `lane`: effective `ActionLane`.
- `action`: `LaneAction`.
- `reason_code`: machine‑readable reason string.
- `vt_current`, `vt_next`: Lyapunov fields.
- `roh`, `k`, `e`, `r`: scalars used in the decision.

`LaneDecision` is the canonical governance output for a telemetry window. It is persisted in SQLite views and emitted as ALN particles.

### decide_lane

`decide_lane` is the core governance function. It takes:

- `&MachineTelemetry`
- `&KerCoordinates`
- `&LaneConfig`
- `&GovernanceGateConfig`

It returns a `LaneDecision` based on:

1. Corridor availability (`corridor_available`).
2. RoH ceiling (`roh_ceiling_global`).
3. Lyapunov residual (`max_delta_vt` and `delta_vt`).
4. K/E/R lane thresholds.

No domain‑specific branching occurs; all semantics are driven by configuration and normalized data.

## C‑ABI FFI

The `ffi` module provides C‑compatible structures and an extern function for C++ integration.

### C structs

All C structs are `#[repr(C)]` and designed for direct mapping to C++ POD types:

- `CMachineTelemetry`
- `CKerCoordinates`
- `CLaneConfig`
- `CGovernanceGateConfig`
- `CLaneDecision`

These match corresponding C++ structs used in `src/cpp/waste/*` modules.

### prometheus_praxis_decide_lane

Signature:

```c
int prometheus_praxis_decide_lane(
    const CMachineTelemetry* telemetry,
    const CKerCoordinates* ker,
    const CLaneConfig* lane_cfg,
    const CGovernanceGateConfig* gate_cfg,
    CLaneDecision* out_decision
);
```

Behavior:

- Returns `0` on success, non‑zero on null pointers or conversion errors.
- Writes lane, action, reason code, Lyapunov fields, RoH, and K/E/R into `out_decision`.
- Does not perform any actuation.

C++ usage:

1. Populate `CMachineTelemetry` and `CKerCoordinates` from numeric kernels.
2. Load `CLaneConfig` and `CGovernanceGateConfig` from EcoNet configuration tables.
3. Call `prometheus_praxis_decide_lane`.
4. Persist `CLaneDecision` in SQLite and emit ALN particles.

## Kani harnesses

The `kani_harnesses` module defines formal verification checks for lane invariants.

Current harnesses:

- `kani_production_lane_never_halts_when_inside_bounds`  
  Asserts that if:
  - `K`, `E` meet Production minima,
  - `R` is below Production maximum,
  - `delta_vt` is within `max_delta_vt`,
  - `roh` is below `roh_ceiling_global`,
  then `LaneAction` is not `Halt` for Production lane.

- `kani_no_corridor_always_halts`  
  Asserts that if:
  - `corridor_available` is false,
  then `LaneAction` must be `Halt`, regardless of other values.

Running Kani:

- Ensure `kani-verifier = 0.67.0` is installed.
- Run:

```bash
cargo kani --package prometheus_praxis_sdk
```

Kani will compile the crate and run the proof harnesses defined in `src/kani_harnesses.rs`.

## Building and using the crate

### Build

From the repository root:

```bash
cd crates/prometheus_praxis_sdk
cargo build --release
```

This produces:

- A Rust library (`libprometheus_praxis_sdk.rlib`).
- A C‑ABI compatible shared library (`libprometheus_praxis_sdk.so` or platform equivalent).

### Integrating in Rust

Add to Cargo.toml of another crate:

```toml
[dependencies]
prometheus_praxis_sdk = { path = "crates/prometheus_praxis_sdk" }
```

Use in code:

```rust
use prometheus_praxis_sdk::lanes::{
    MachineTelemetry,
    TelemetryDomain,
    KerCoordinates,
    LaneConfig,
    GovernanceGateConfig,
    ActionLane,
    decide_lane,
};
use rust_decimal::Decimal;

fn example_decision() {
    let mt = MachineTelemetry {
        machine_id: "pump_001".into(),
        station_id: "station_a".into(),
        domain: TelemetryDomain::WastewaterPump,
        timestamp_utc: "2026-07-27T03:00:00Z".into(),
        r_hydraulics: Decimal::new(30, 2),
        r_energy: Decimal::new(20, 2),
        r_uncertainty: Decimal::new(10, 2),
        r_reliability: Decimal::new(10, 2),
        r_extra_1: None,
        r_extra_2: None,
        roh: Decimal::new(50, 2),
        vt_current: Decimal::new(50, 2),
        vt_next: Decimal::new(52, 2),
    };

    let ker = KerCoordinates {
        k_knowledge: Decimal::new(80, 2),
        e_eco_impact: Decimal::new(80, 2),
        r_risk: Decimal::new(30, 2),
    };

    let lane_cfg = LaneConfig {
        lane: ActionLane::Production,
        roh_ceiling_global: Decimal::new(80, 2),
        max_delta_vt: Decimal::new(5, 2),
        k_min_research: Decimal::new(0, 2),
        k_min_pilot: Decimal::new(50, 2),
        k_min_prod: Decimal::new(70, 2),
        e_min_research: Decimal::new(0, 2),
        e_min_pilot: Decimal::new(50, 2),
        e_min_prod: Decimal::new(70, 2),
        r_max_research: Decimal::new(100, 2),
        r_max_pilot: Decimal::new(70, 2),
        r_max_prod: Decimal::new(40, 2),
    };

    let gate_cfg = GovernanceGateConfig {
        corridor_available: true,
        manual_override_allowed: false,
        allow_research_exploration: false,
    };

    let decision = decide_lane(&mt, &ker, &lane_cfg, &gate_cfg);

    println!("Lane decision: {:?}, reason: {}", decision.action, decision.reason_code);
}
```

## Integrating with C++

C++ numeric kernels can include the generated header for C‑ABI structs and functions (e.g., `prometheus_praxis_sdk_ffi.h`) that mirror `CMachineTelemetry`, `CKerCoordinates`, `CLaneConfig`, `CGovernanceGateConfig`, and `CLaneDecision`.

Example (conceptual):

```cpp
CMachineTelemetry mt{};
CKerCoordinates ker{};
CLaneConfig lane_cfg{};
CGovernanceGateConfig gate_cfg{};
CLaneDecision decision{};

// populate mt, ker, lane_cfg, gate_cfg from C++ telemetry

int rc = prometheus_praxis_decide_lane(&mt, &ker, &lane_cfg, &gate_cfg, &decision);
if (rc == 0) {
    // Persist decision and evidence into SQLite and ALN.
}
```

The header and binding files should be maintained in the C++ side of the repository, aligned with `ffi.rs`.

## Alignment with GOVERNANCE_GRAMMAR.md

This crate is designed to adhere to the governance grammar defined in `GOVERNANCE_GRAMMAR.md` at the repository root. Any changes to:

- Telemetry surfaces
- KER coordinates
- Lyapunov residual semantics
- Lanes and gates

must be reflected both in this crate and in the grammar document to maintain consistency across C++, Rust, SQLite, and ALN schemas.

## License

This crate is dual‑licensed under:

- MIT
- Apache‑2.0

You may use it under either license.
