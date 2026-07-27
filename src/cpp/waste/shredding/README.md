# Waste Shredding Governance CPP

This directory contains non‑actuating C++ governance adapters for the waste shredding corridor in Prometheus‑Praxis. All code operates on telemetry only and feeds KER and blast‑radius governance, never direct actuators.[file:6][file:8]

## Scope and role

- Bind shredding and screening telemetry into the EcoNet governance spine as KER triplets, Lyapunov residual \(V_t\), RoH scalar, and non‑offsettable plane IDs.[file:6][file:8]
- Call into non‑actuating Rust FFI (blastradius‑cross‑spine style) to obtain canonical KER, RoH, and blast‑radius radii, using CPP structs as diagnostic carriers.[file:6][file:7]
- Provide read‑only snapshots and diagnostics for governance tools, lane admissibility checks, and EcoNet SQL views (`v_shard_residual`, `v_shard_ker`), with no actuator surfaces.[file:6][file:8]

## Files

- `shredding_governance_adapter.hpp`  
  - Defines `ShredderTelemetry`, `ShreddingKerSnapshot`, and `ShreddingKerAdapter`.[file:6]  
  - Encodes KER triplets, Lyapunov residual `vt`, RoH scalar, waste/topology plane IDs, and lane tags (`RESEARCH`, `PILOT`, `PRODUCTION`, `BLOCKED`).[file:6][file:8]  
  - Exposes pure methods `computeKerSnapshot(...)` that bind shredding/screening telemetry into governance snapshots.[file:6]

- `shredding_governance_adapter.cpp`  
  - Implements `ShreddingKerAdapter` by composing CPP telemetry into `EcoNetKerInput` and calling the Rust FFI `econet_governance_compute_shredding_ker`.[file:6][file:7]  
  - Normalizes local KER hints, residual slices, RoH ceilings, topology risk, and blast‑radius radii before delegating final scoring to the EcoNet governance spine.[file:6][file:7][file:8]  
  - Provides internal helpers for lane mapping, blast‑radius heuristics, and topology risk proxies; all results are diagnostics for SQL/ALN layers.[file:6][file:8]

## Governance invariants

- Non‑actuating by design: no fieldbus drivers, no pump or gate control, no hardware IO; telemetry‑only numerics.[file:6][file:8]  
- KER axes `k`, `e`, `r` are clamped into \([0,1]\) and derived from corridor‑aligned risk coordinates and residual slices, consistent with Cyboquatic workload and drainage kernels.[file:6][file:8]  
- Lyapunov residual \(V_t\) is non‑negative and composed from localized risk coordinates; RoH scalars and blast‑radius radii follow the EcoNet blastradius governance patterns.[file:6][file:7][file:8]

## Integration points

- Rust FFI  
  - Rust crates under `workspace/crates` provide `econet_governance_compute_shredding_ker` and related kernels, implemented with Rust 2024, `rust-version = "1.85"`, and mandatory `kani-verifier = 0.67` in keeping with Prometheus‑Praxis policies.[file:6][file:8]  
  - CPP structs here are POD and C‑ABI friendly, enabling safe Rust wrappers to treat shredding windows as governance inputs without duplicating math.[file:6][file:7]

- ALN v2 and SQL  
  - ALN particles (e.g., workload and drainage kernels) define canonical KER fields and residual invariants; shredding snapshots are mapped into these particles and into SQLite views like `v_shard_ker` and `v_shard_residual`.[file:6][file:8]  
  - Phoenix hex anchors and DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` bind these CPP files and their Rust counterparts into the Eco‑Fort registry for provenance and corridor enforcement.[file:6][file:8]

## Usage guidelines

- Use these adapters from Rust or C++ diagnostic harnesses to compute shredding KER snapshots that will be written into governance databases and ALN shards; do not add actuator logic here.[file:6][file:8]  
- When extending this directory:
  - Preserve non‑actuating constraints and governance‑first design.[file:6]  
  - Align any new telemetry fields with existing ALN particles and SQLite schemas (waste, topology, blast‑radius, KER) and update hex‑anchor bindings accordingly.[file:6][file:8]
