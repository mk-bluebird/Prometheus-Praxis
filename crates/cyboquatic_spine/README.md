# cyboquatic_spine

`cyboquatic_spine` is a non‑actuating `cdylib` spine that exposes a small, stable surface for Cyboquatic blast‑radius analysis and eco‑workload bookkeeping inside the Prometheus‑Praxis mono‑repo.

It is designed for use from Rust, C, or other languages via FFI, and never talks to hardware or the network.

---

## Goals

- Provide a compact, C‑compatible API for:
  - Cyboquatic crate and node “blast‑radius” style impact fan‑out over ecosafety workloads.
  - Reading and updating ecosafety workload metadata stored in rusqlite databases.
- Keep all behavior deterministic and local‑only to match EcoNet governance requirements.
- Make it easy to plug ecosafety kernels into external tools without re‑implementing Rust types.

---

## High‑level design

- Built as a `cdylib` with a corresponding Rust `rlib` API in `src/lib.rs`.
- Uses:
  - `rusqlite` (bundled) for SQLite‑backed eco‑workload and index storage.
  - `serde` / `serde_json` for passing structured payloads as UTF‑8 JSON across FFI boundaries.
- Enforces a non‑actuating design:
  - No hardware control, no socket or HTTP calls.
  - Only local file/DB access performed through explicit functions.

---

## Typical use cases

- **EcoNet governance tools**
  - Ask the spine for which nodes, shards, or crates are in the blast‑radius of a proposed change.
  - Summarize eco‑workloads (e.g., “how many nodes in this corridor, by lane and KER band”) with a single FFI call.

- **External dashboards or console tools**
  - Link against the `cdylib` to query Cyboquatic workloads without re‑embedding Rust ecosafety logic.
  - Use JSON in/out to keep bindings thin and language‑agnostic.

---

## Crate layout

- `src/lib.rs`
  - Rust entry points and public types.
  - FFI‑safe wrappers (e.g., `extern "C"` functions) that accept:
    - Pointers to UTF‑8 strings (JSON payloads or file paths).
    - Pointers to output buffers or callback hooks.
  - Internal helpers for:
    - Opening/initializing SQLite databases.
    - Running prepared queries keyed to ecosafety shard schemas.
    - Computing blast‑radius style dependency / workload neighborhoods.

- `Cargo.toml`
  - Declares `crate-type = ["cdylib"]` and pins serde/serde_json/rusqlite versions consistent with the Prometheus‑Praxis workspace.

---

## Building

From the Prometheus‑Praxis workspace root:

```bash
cargo build -p cyboquatic_spine --release
```

The compiled shared library will be placed under `target/release` with a platform‑specific name, for example:

- Linux: `libcyboquatic_spine.so`
- macOS: `libcyboquatic_spine.dylib`
- Windows: `cyboquatic_spine.dll`

---

## FFI usage sketch

From C or C‑compatible languages you would typically:

- Load the shared library (`dlopen` / `LoadLibrary`).
- Resolve exported functions such as:

  - `cyboquatic_spine_blast_radius(json_request, json_response_out)`
  - `cyboquatic_spine_workload_summary(json_request, json_response_out)`

- Pass in a JSON request describing:
  - The crate or node of interest.
  - Workspace or shard paths.
- Receive a JSON response with:
  - Lists of affected nodes or shards.
  - Aggregate workload summaries.

Exact signatures live in `src/lib.rs` and are kept minimal for easy binding.

---

### Hydraulic breach queries (non‑actuating)

The `hydraulic_breach_queries.rs` module provides readonly, spatially‑aware diagnostics for canal nodes that experience hydraulic surcharge events within a Cyboquatic basin.[file:99]

- It uses the `rtree_canal_node` and `canal_node` SQLite tables to select candidate nodes within a latitude/longitude envelope around a breach coordinate `(lat_b, lon_b)` with window sizes `dlat` and `dlon`.[file:99]
- It joins these spatial candidates against `node_surcharge` to compute, per node, the maximum `surcharge_pa` observed in a specified UTC time window `[ts_window_start, ts_window_end]`, returning nodes whose `max_surcharge_pa` exceeds a threshold `X_pa`.[file:99]
- It provides a second query over `daily_surcharge` that, for the same spatial envelope and a single `breach_day`, returns `max_surcharge_pa` together with KER and Lyapunov residual fields `K`, `E`, `R`, and `Vt` for nodes above the same `X_pa` threshold.[file:99]
- All queries are strictly non‑actuating: they open the EcoNet/Cyboquatic spine in readonly mode and never write to SQLite, hardware, or control surfaces.[file:99]

#### Rust types and bundle

The module defines small Rust structs that match the query outputs, making them easy to surface as JSON through the Cyboquatic cdylib.[file:99]

- `HydraulicInstantHit` captures `node_id` and `max_surcharge_pa` for a single node over the breach time window.[file:99]
- `HydraulicDailyHit` captures `node_id`, `max_surcharge_pa`, `K`, `E`, `R`, and `Vt` for daily surcharge diagnostics around the breach day.[file:99]
- `HydraulicBreachParams` holds the input parameters: `lat_b`, `lon_b`, `dlat`, `dlon`, `ts_window_start`, `ts_window_end`, `X_pa`, and `breach_day`, keeping the API explicit and reproducible.[file:99]
- `HydraulicBreachBundle` aggregates both instant and daily hits, so higher‑level tooling (Lua, Kotlin, ALN, C) can request a single breach context blob per basin.[file:99]

#### Intended use in the Cyboquatic spine

These queries are designed to plug into the broader Cyboquatic EcoNet spine as evidence‑driven diagnostics.[file:99]

- They help identify canal nodes whose surcharge behavior may interact with Cyboquatic restoration surfaces, blast‑radius tables, and ecoper‑joule windows.[file:99]
- They feed KER and Lyapunov governance logic without ever controlling pumps, valves, or any physical actuators, preserving the RESEARCH/non‑actuating corridor for Cyboquatic machinery.[file:99]
- They can be wrapped by the existing cdylib JSON/FFI layer so AI‑driven tools and dashboards can inspect hydraulic breach contexts while staying within readonly, sovereignty‑respecting corridors.[file:99]

---

## Design constraints

- Non‑actuating: the library must never open device handles or send commands to field machinery.
- Local‑only: all inputs are local files, shard paths, or JSON payloads.
- Stable surface: FFI functions are versioned and documented so downstream tools can track changes cleanly.

---

## License

Dual‑licensed under MIT and Apache‑2.0, at your option.
