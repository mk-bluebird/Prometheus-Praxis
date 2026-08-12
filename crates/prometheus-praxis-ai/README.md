<!-- File: crates/prometheus-praxis-ai/README.md -->
# prometheus-praxis-ai

`prometheus-praxis-ai` evaluates non-actuating ecological lane decisions for AI and industrial workload telemetry. It uses the shared `lanes/sdk.rs` grammar for normalized risk coordinates, K/E/R, risk-of-harm, and Lyapunov residual checks.

## Layout

- `src/lanes/sdk.rs`: frozen shared telemetry and governance grammar.
- `src/lib.rs`: public re-exports and stable cross-language action labels.
- `src/bin/ppx_ai_lane_check.rs`: small command-line evaluator.
- `Cargo.toml`: Rust 2024 package definition.

## Build

```sh
cargo build --release
```

## Run

```sh
cargo run --bin ppx-ai-lane-check -- \
  ai-node-01 canal-station-02 2026-08-11T22:36:00Z \
  pilot 0.10 0.12 0.05 0.04 0.10 0.08 0.09
```

The binary emits tab-separated fields suitable for a C++ adapter, Lua predicate, or SQLite ingestion layer. A `PROCEED` result is advisory only; physical-machine commands remain outside this crate.

## Integration Contract

C++ and Lua adapters should preserve these fields exactly:

```text
machine_id, station_id, timestamp_utc, r_hydraulics, r_energy,
r_uncertainty, r_reliability, roh, vt_current, vt_next,
k_knowledge, e_eco_impact, r_risk, lane, action, reason_code
```

Use `WORKLOAD_SCHEMA_ID` and the lane/action codes when storing or exchanging a decision. Numeric risk and K/E/R fields must remain within `[0,1]`.
