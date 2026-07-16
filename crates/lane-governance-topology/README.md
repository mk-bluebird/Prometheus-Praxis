# lane-governance-topology

`lane-governance-topology` is a Rust crate in the Prometheus-Praxis mono-repo that enforces lane governance in real time, using KER-aligned risk planes and Lyapunov residuals to promote or downgrade lanes based on topology streams.

It consumes `r_topology` samples, applies `LaneGovernanceTopology2026v1` rules, and emits lane events that the rest of the ecosystem uses for eco-wealth, deployment, and governance decisions.

---

## Goals

- Encode lane promotion and downgrade logic as a reusable Rust library.
- Consume streaming `r_topology` data (risk coordinates, residuals, lanes).
- Enforce HARD-band and corridor semantics from ALN specs.
- Provide a C/FFI surface for C++ engines and Lua/Java bridges.
- Integrate with CI to prevent unsafe lane changes from reaching production.

---

## Core Concepts

- **Lane**: Logical governance state for a steward/asset (`RESEARCH`, `PILOT`, `PROD`, `QUARANTINE`, etc.).
- **r_topology**: Stream of risk-topology samples with KER-aligned coordinates (R, Vt, lane, timestamp).
- **Lyapunov residual \(V_t\)**: Quadratic residual \(V_t = \sum_j w_j r_j^2\) used to gate promotion and enforce non-increasing risk bands.
- **HARD band**: Upper-risk corridor boundary; repeated breaches must trigger lane downgrade within bounded windows.
- **Lane events**: Emitted promotions/downgrades fed back into the eco-wealth kernel and governance particles.

All of the above stay consistent with the existing rx–Vt–KER grammar and EcoNet ALN patterns in Prometheus-Praxis. [file:1]

---

## Crate Layout

- `src/lib.rs`  
  Public Rust API for processing `r_topology` windows and emitting lane events.

- `src/ffi.rs`  
  FFI bindings for C++ and other languages that integrate the engine.

- `ker/lane_topology_engine.hpp` / `ker/lane_topology_engine.cpp`  
  C++ sliding-window engine for high-performance, vectorized processing, consumed via FFI. [file:1]

- `aln/LaneGovernanceTopology2026v1.aln`  
  ALN definition of lane governance rules (HARD bands, promotions, downgrades, invariants).

- `aln/LaneRollbackTest2026v1.aln`  
  Scenario file that simulates HARD-band breaches and expected downgrades within two windows; used to auto-generate integration tests. [file:1]

---

## Public API (Rust)

A typical API shape for this crate:

```rust
/// A single r_topology sample.
#[derive(Debug, Clone)]
pub struct RTopologySample {
    pub scenario_id: String,
    pub t_index: i32,
    pub timestamp_ms: i64,
    pub lane: String,
    pub r_metric: f32,
    pub vt: f32,
    pub hard_band: f32,
}

/// Kind of lane event.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LaneEventKind {
    Promote,
    Downgrade,
    Stay,
}

/// Lane event emitted by the engine.
#[derive(Debug, Clone)]
pub struct LaneEvent {
    pub scenario_id: String,
    pub sample_index: i32,
    pub from_lane: String,
    pub to_lane: String,
    pub kind: LaneEventKind,
    pub reason: String,
}

/// Process a sliding window of r_topology samples and emit lane events.
pub fn process_r_topology_window<F>(
    samples: &[RTopologySample],
    window_ms: i64,
    mut emit: F,
)
where
    F: FnMut(LaneEvent),
{
    // Implementation delegates to the C++ sliding-window engine via FFI,
    // and wraps callbacks into safe Rust LaneEvent values.
}
```

This API is implemented to match the ALN rules and the C++ engine behavior. [file:1]

---

## C++ Sliding-Window Engine

The crate relies on a C++ engine for high-performance processing:

```cpp
extern "C" {

struct r_topology_sample {
    const char* particle_id;
    const char* lane;
    float       k;
    float       e;
    float       r;
    float       residual;
    std::int64_t timestamp_ms;
};

struct lane_event {
    const char* particle_id;
    const char* from_lane;
    const char* to_lane;
    const char* reason;
};

typedef void (*lane_event_callback)(const lane_event* ev, void* user_data);

std::int32_t lane_topology_process_window(
    const r_topology_sample* samples,
    std::size_t len,
    std::int64_t window_ms,
    lane_event_callback cb,
    void* user_data
);

}
```

- Rust builds and links this C++ engine via a `build.rs` script using `cc`. [web:68]
- The engine applies `LaneGovernanceTopology2026v1` rules to raise promotions or downgrades.

---

## ALN Integration

### LaneGovernanceTopology2026v1

The ALN spec encodes:

- Fields:

  - `scenario_id`, `node_id`, `region_id`, `lane`, `r_metric`, `vt`, `hard_band`, etc.

- Invariants:

  - HARD-band semantics for R and Vt.  
  - Lane promotion and downgrade rules.  
  - Monotonicity invariants respecting non-compensable planes. [file:1]

This crate is expected to read ALN-derived configurations and align behavior accordingly.

### LaneRollbackTest2026v1

`LaneRollbackTest2026v1.aln` describes a test scenario:

- A sequence of `LaneRollbackSample2026v1` entries with:

  - Safe baseline windows.  
  - Two consecutive HARD-band breaches (R/Vt above hard).  
  - Expected downgrade within two windows, verified by the Rust test harness. [file:1]

A code generator converts this ALN scenario into Rust integration tests under `tests/`, ensuring lane rollback behavior is preserved.

---

## Testing

### Unit Tests

- Pure Rust tests:

  - Verify basic behavior of `process_r_topology_window` for simple scenarios.  
  - Check that single HARD-band violations contribute to event generation as expected.

Example:

```rust
#[test]
fn single_hard_band_breach_produces_downgrade() {
    let samples = vec![
        RTopologySample {
            scenario_id: "S1".into(),
            t_index: 0,
            timestamp_ms: 0,
            lane: "PROD".into(),
            r_metric: 0.2,
            vt: 0.1,
            hard_band: 0.8,
        },
        RTopologySample {
            scenario_id: "S1".into(),
            t_index: 1,
            timestamp_ms: 600_000,
            lane: "PROD".into(),
            r_metric: 0.9,
            vt: 0.95,
            hard_band: 0.8,
        },
    ];

    let mut events = Vec::new();
    process_r_topology_window(&samples, 600_000, |ev| events.push(ev));

    assert!(
        events.iter().any(|e| e.kind == LaneEventKind::Downgrade),
        "expected at least one downgrade event"
    );
}
```

### Integration Tests

- Generated from `LaneRollbackTest2026v1.aln`:

  - Assert that downgrade fires within two windows after a HARD-band breach. [file:1]

### CI

- `cargo test` is run in CI for this crate.  
- Additional checks may include:

  - Comparing residuals from this crate against eco-wealth kernel outputs.  
  - Ensuring lane events are consistent with ALN invariants for KER.

---

## Usage

### As a Rust Library

Add to your workspace `Cargo.toml`:

```toml
[dependencies]
lane-governance-topology = { path = "crates/lane-governance-topology" }
```

In your Rust code:

```rust
use lane_governance_topology::{RTopologySample, LaneEventKind, process_r_topology_window};

fn enforce_lanes(samples: Vec<RTopologySample>) {
    process_r_topology_window(&samples, 600_000, |event| {
        match event.kind {
            LaneEventKind::Promote => {
                // Update eco-wealth crate or governance shard.
            }
            LaneEventKind::Downgrade => {
                // Trigger lane downgrade in eco-wealth kernel.
            }
            LaneEventKind::Stay => {
                // No-op.
            }
        }
    });
}
```

### Via FFI

For C++ or other language consumers:

- Link to the compiled `lane-governance-topology` cdylib.  
- Use the C ABI from `ker/lane_topology_engine.hpp`.  
- Implement a callback to receive `lane_event` structures and propagate them into your host language.

---

## Non-Actuating Design

This crate is strictly non-actuating:

- It computes lane decisions and emits events.  
- It does not:

  - Open valves, toggle pumps, or send actuator commands.  
  - Directly modify field hardware or production corridors. [file:1]

All actuation must go through separately governed controllers that treat lane events as advisory inputs.

---

## KER / EcoNet Alignment

- **KER semantics**:

  - Uses risk planes (energy, hydraulics, carbon, biology, materials, biodiversity, data quality, topology). [file:1]
  - Residual \(V_t\) is consistent with the core eco-wealth kernel \(V_t = \sum_j w_j r_j^2\). [file:1]

- **EcoNet**:

  - Integrates with ALN particles (e.g., `LaneGovernanceTopology2026v1`, `LaneRollbackTest2026v1`).  
  - Reuses existing evidencehex, signinghex, and KER corridor grammars.

---

## License

This crate is dual-licensed:

- MIT  
- Apache-2.0  

See the root `LICENSE-MIT` and `LICENSE-APACHE` files in the Prometheus-Praxis repository.

---

## Attribution

- Consolidated repository:  
  `https://github.com/mk-bluebird/Prometheus-Praxis`

- Bostrom DID:  
  `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`

All lane-governance work and assets are anchored to this DID and corresponding ALN identities.
