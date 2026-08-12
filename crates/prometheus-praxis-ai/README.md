<!-- File: crates/prometheus-praxis-ai/README.md -->
# prometheus-praxis-ai

`prometheus-praxis-ai` is a non-actuating lane-evaluation crate for ecological AI workloads and industrial telemetry. It evaluates K/E/R coordinates, Lyapunov residuals, corridor availability, and risk-of-harm limits, then emits a stable advisory decision: `PROCEED`, `DERATE`, or `HALT`.

The crate is the governance grammar and decision layer. Device control, pump control, accelerator scheduling, and other physical actuation remain outside this package. External engines must treat decisions as auditable workload-admission signals.

## Scope

The crate supports a common telemetry surface for:

- AI inference and batch workloads.
- Wastewater pumps and filtration stations.
- Shredders and material-recovery equipment.
- Hammermills and biodegradable-material processing.
- Future eco-restoration equipment whose adapters can normalize measurements into the shared telemetry grammar.

The primary public contract is `src/lanes/sdk.rs`. All adapters preserve its machine identity, station identity, timestamp, normalized risk planes, residual values, K/E/R values, lane, action, and reason code.

## Repository Layout

```text
Prometheus-Praxis/
├── crates/
│   └── prometheus-praxis-ai/
│       ├── Cargo.toml
│       ├── README.md
│       └── src/
│           ├── lib.rs
│           ├── lanes/
│           │   └── sdk.rs
│           └── bin/
│               └── ppx_ai_lane_check.rs
├── cpp/
│   ├── eco_restoration/
│   │   └── ppx_ai_workload_lane_engine.cpp
│   └── tools/
│       └── ppx_ai_workload_bridge.cpp
├── lua/
│   └── ppx_ai_workload/
│       └── lane_predicate.lua
└── sql/
    └── ppx_ai_workload/
        └── workload_telemetry.sql
```

## Build

Rust 1.85 or later is required.

```sh
cargo build --release
cargo test
```

Run the command-line evaluator:

```sh
cargo run --bin ppx-ai-lane-check -- \
  ai-node-01 canal-station-02 2026-08-11T22:40:00Z \
  pilot 0.10 0.12 0.05 0.04 0.10 0.08 0.09
```

The command emits a tab-separated decision record. Exit status `0` means the evaluator completed successfully; the action field remains authoritative for workload admission.

## Cross-Language Contract

All language adapters exchange a flat record with explicit units and normalized values.

```text
schema_id
machine_id
station_id
timestamp_utc
domain
r_hydraulics
r_energy
r_uncertainty
r_reliability
r_extra_1
r_extra_2
roh
vt_current
vt_next
delta_vt
k_knowledge
e_eco_impact
r_risk
lane
action
reason_code
```

Rules:

- `r_*`, `roh`, `k_knowledge`, `e_eco_impact`, and `r_risk` are dimensionless values in `[0,1]`.
- `vt_current`, `vt_next`, and `delta_vt` are non-negative residual values, except `delta_vt` may be negative when the residual improves.
- Timestamps use UTC ISO-8601 text such as `2026-08-11T22:40:00Z`.
- `lane` is one of `RESEARCH`, `PILOT`, or `PRODUCTION`.
- `action` is one of `PROCEED`, `DERATE`, or `HALT`.
- `reason_code` is stable machine-readable text and must be retained by storage adapters.
- Missing required fields invalidate the record. Adapters must not manufacture sensor measurements.

## C++ Engine Target

Place the C++ numeric kernel and bridge under:

```text
cpp/eco_restoration/ppx_ai_workload_lane_engine.cpp
cpp/tools/ppx_ai_workload_bridge.cpp
```

The C++ engine owns high-throughput numeric work, including accelerator-energy estimation, carbon-intensity calculations, hydraulic and thermal risk normalization, and device-side telemetry validation.

The bridge exposes a C-compatible function boundary so Rust may call a compiled C++ engine through an explicit adapter layer. The boundary accepts primitive values only: UTF-8 identifiers, normalized doubles, and output pointers for `action`, `delta_vt`, `K`, `E`, and `R`.

Recommended C++ responsibilities:

- Convert raw device readings to normalized risk coordinates.
- Preserve unit conversion provenance in emitted telemetry.
- Reject invalid numeric input including non-finite measurements.
- Avoid network calls and physical-machine commands.
- Write a complete decision record for Rust, Lua, or SQLite consumers.

## Lua Adapter Target

Place Lua routing predicates under:

```text
lua/ppx_ai_workload/lane_predicate.lua
lua/ppx_ai_workload/telemetry_normalizer.lua
```

Lua is appropriate for constrained edge environments where a local agent must parse frames, validate required fields, and select a safe local workload queue. Lua predicates do not replace the Rust or C++ evaluator; they consume its emitted decision record.

A Lua predicate should:

- Require `schema_id`, `action`, `reason_code`, `lane`, K/E/R, and `delta_vt`.
- Permit only records whose normalized fields remain in `[0,1]`.
- Send `PROCEED` records to an approved low-impact queue.
- Send `DERATE` records to a reduced-resource queue.
- Preserve `HALT` records in local telemetry storage for operator review.
- Treat malformed, incomplete, or out-of-range records as invalid telemetry.

## SQLite Target

Place the database schema and queries under:

```text
sql/ppx_ai_workload/workload_telemetry.sql
sql/ppx_ai_workload/workload_views.sql
sql/ppx_ai_workload/workload_indexes.sql
```

SQLite is the durable local audit layer. It stores source telemetry, computed K/E/R, lane configuration identifiers, residual values, and decisions. Application code may insert records only after its own validation and SQLite constraints both accept the frame.

Required storage invariants:

```text
k_knowledge BETWEEN 0 AND 1
e_eco_impact BETWEEN 0 AND 1
r_risk BETWEEN 0 AND 1
roh BETWEEN 0 AND 1
r_hydraulics BETWEEN 0 AND 1
r_energy BETWEEN 0 AND 1
r_uncertainty BETWEEN 0 AND 1
r_reliability BETWEEN 0 AND 1
action IN ('PROCEED', 'DERATE', 'HALT')
lane IN ('RESEARCH', 'PILOT', 'PRODUCTION')
```

Recommended indexes:

```text
(machine_id, timestamp_utc DESC)
(station_id, timestamp_utc DESC)
(action, timestamp_utc DESC)
(lane, timestamp_utc DESC)
```

## Integration Sequence

1. A device adapter collects raw power, duration, thermal, hydraulic, and local environmental measurements.
2. The C++ kernel normalizes measurements into risk coordinates and computes energy and impact metrics.
3. The Rust crate validates telemetry, applies lane thresholds, and emits a non-actuating decision record.
4. The SQLite adapter stores the source frame and decision under database constraints.
5. A Lua predicate reads the decision record and chooses an approved local workload queue.
6. Operators and ecological-restoration systems inspect stored records to improve energy efficiency, reduce heat and water impacts, and prioritize workloads with measurable ecological value.

## Ecological Operating Principle

A workload is acceptable only when its evidence quality and ecological benefit meet lane-specific thresholds while risk-of-harm and residual growth remain within the configured corridor. This design favors energy-efficient computation scheduled around renewable availability and measurable restoration value, while retaining a complete local audit trail for future analysis.
