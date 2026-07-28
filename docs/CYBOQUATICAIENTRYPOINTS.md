# filename: docs/CYBOQUATICAIENTRYPOINTS.md

## Overview

This document describes safe AI-chat entrypoints for cyboquatic diagnostics in Prometheus-Praxis. It identifies non-actuating functions, ALN shards, and DB views that AI agents can consume to reason about ecological restoration without touching hardware or violating governance invariants.

## Safe entrypoint categories

### ALN particles

- Workload:
  - `aln/alnCyboquaticWorkloadKernel2026v1.aln2`
- Drainage-decay:
  - `aln/alnCyboquaticDrainageDecayKernel2026v1.aln2`
- Blast-radius:
  - `aln/alnCyboquaticBlastRadiusGovernance2026v1.aln2`
- Energy/ecoperJoule/restoration:
  - `aln/alnCyboquaticEnergyEcoperJouleRestoration2026v1.aln2`

AI agents may read these ALN files to understand field semantics, K,E,R corridors, and Lyapunov constraints.

### DB views and tables

- `db/dbcyboquaticdailyprogress.sql` (table `cyboquatic_daily_progress`)
- `db/dbcyboquaticdrainagedecayindex.sql` (table `cyboquatic_drainagedecay_index`)
- `db/dbcyboquaticblastradiusindex.sql` (table `cyboquatic_blast_radius_index`, view `v_cyboquatic_blast_radius_facade`)
- `db/dbcyboquaticenergyecoperjoulerestoration.sql` (table `cyboquatic_energy_ecoperjoule_restoration`)

AI agents may query these tables and views via read-only SQLite connections to inspect diagnostic frames and governance flags.

### C++/Rust helpers

- `src/cpp/cyboquatic_workload_engine.cpp`
- `src/cpp/cyboquatic_drainagedecay_engine.cpp`
- `src/cpp/cyboquatic_blastradius_engine.cpp`
- `crates/prometheus-praxis-lyapunov-guard/src/lib.rs` (functions `lyapunov_non_increasing`, `ker_band_and_consistency`)
- `crates/aln-cyboquatic-codegen` (codegen helpers for ALN-derived structs and schemas)

AI agents must treat these as numeric and proof helpers only; they must not be wired directly to actuators or hardware APIs.

## AI-chat usage guidelines

- Read-only:
  - Only read ALN files, DB schemas, and diagnostic data.
  - Do not generate or propose code that opens sockets, device handles, or embeds hardware drivers.
- Governance-aligned:
  - Respect K,E,R ranges and Lyapunov constraints when suggesting changes.
  - Do not widen corridors or relax RoH ceilings without explicit, documented governance justification.
- Hex-anchored:
  - Preserve `evidence_hex` and DID fields when transforming or aggregating data.
  - Use hex anchors to trace provenance and maintain data sovereignty.

## Example AI-chat workflows

1. Frame analysis:
   - Load workload, drainage, blast-radius, and energy/ecoperJoule frames for a single node and day.
   - Compute derived metrics (e.g., average KER, worst Lyapunov residual, carbon-negative compliance).
   - Suggest documentation updates or new ALN rules, not direct actuation paths.

2. Codegen inspection:
   - Read ALN particles and generated C++/SQL files.
   - Verify field mappings, K,E,R constraints, and trigger logic.
   - Propose improvements to documentation, naming, or invariant coverage.

3. Governance reinforcement:
   - Use Lyapunov guard functions and SQL triggers as examples when designing new eco-restoration bands.
   - Ensure any new bands follow the same non-actuating, hex-anchored, KER-aligned pattern.

These entrypoints provide high-yield, safe surfaces for AI-assisted research and diagnostics in cyboquatic pipelines, without compromising EcoFort/Phoenix governance semantics or actuation boundaries.
