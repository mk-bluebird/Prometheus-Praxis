<!-- filename: ecorestorationshard/cyboquatic_progress/20260728/README.md
     purpose: Daily shard README for domain (d) cyboquatic workload (energyreqJ, ΔVt)
-->

# Cyboquatic Progress Shard – 2026-07-28 (Domain d)

- Domain: d — Cyboquatic workload (energyreqJ, ΔVt) for Phoenix canal nodes.  
- Subtask ID: PHX-CANAL-WL-2026-07-28 (derived from date hash, aligned with canal workload lane).  
- Evidence hex: `0x20260728PHXWORKLOADENERGYDV` (bound to primary Bostrom DID).

## Files in this shard

- `cpp/cyboquatic_workload_energyreq.cpp`  
  - C++ non-actuating kernel for energyreqJ normalization and Lyapunov residual V_t over four planes.  
  - Produces diagnostic streams for `node_id`, `r_energy`, `vt_before`, `vt_after`, and `delta_vt`.

- `java/CyboquaticWorkloadTelemetry.java`  
  - Java helper to ensure `dailyprogress` schema and insert workload residual rows into `dbcyboquaticdailyprogress.sqlite`.  
  - Binds `domain_id = 'd'`, the date `20260728`, and `evidencehex = 0x20260728PHXWORKLOADENERGYDV`.

- `kotlin/CyboquaticWindowSummary.kt`  
  - Kotlin inspector computing window-level summaries and KER-like metrics `K`, `E`, `R` per `node_id` and day.  
  - Reads `dailyprogress` and renders AI-chat friendly text diagnostics.

- `lua/fog_router_workload.lua`  
  - Lua FOG-router predicates `is_safe_workload` and `classify_lane` for workload frames.  
  - Enforces non-regression (`delta_vt <= 0`), corridor-safe energy, and KER bands for RESEARCH/PILOT/PRODUCTION lanes.

- `sql/cyboquatic_dailyprogress_seed.sql`  
  - Seeds a reference `dailyprogress` row for `PHX-CANAL-NODE-01` on `20260728`.  
  - Includes KER metrics and canal node parameters `canal_velocity_ms`, `sensor_health_risk`.

- `sql/cyboquatic_ker_invariants.sql`  
  - Strict SQLite trigger `trg_dailyprogress_ker_invariants` enforcing KER bounds, non-regressive residual, and normalized canal node parameters.

- `aln/cyboquatic_workload_ker_particle_20260728.aln`  
  - ALN v2 governance particle describing KER thresholds, corridor bands, and lane rules for domain d at this date.  
  - Bound to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` and `0x20260728PHXWORKLOADENERGYDV`.

## Placement strategy

- Root: `ecorestoration_shard/cyboquatic_progress/20260728/`  
  - Mirrors Phoenix Hex registry conventions for cyboquatic daily progress.  
  - Subdirectories: `cpp`, `java`, `kotlin`, `lua`, `sql`, `aln` for language-specific artifacts.

- This shard is non-actuating:  
  - All components operate on telemetry and governance data only.  
  - No drivers, fieldbus calls, or direct machinery control are present.

## Eco-impact and invariants

- Energy-efficient and carbon-negative emphasis:  
  - Residual kernel tightens corridors by requiring `delta_vt <= 0`, making unsafe workloads non-representable in PRODUCTION lanes.  
  - KER metrics are derived from normalized residual evolution to favor ecorestorative workloads per Joule.

- Canal node parameters:  
  - `canal_velocity_ms` and `sensor_health_risk` are normalized to `[0,1]` and wired into the triggers for consistent FOG and KER behavior.  
  - This keeps workload modeling consistent with Phoenix canal hydraulics and sensor health corridors.
