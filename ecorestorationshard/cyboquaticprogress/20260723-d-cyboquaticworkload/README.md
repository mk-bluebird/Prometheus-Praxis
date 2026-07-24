# ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/README.md

Overview
- Domain for 2026-07-23: d — cyboquatic workload metrics (`energyreqJ`, `ΔVt`) with SQL telemetry.[file:6][file:2]
- This shard defines non-actuating C++, Java, Kotlin, Lua, SQL, and ALN v2 artifacts for canal and AI-node workloads, all tied to KER (Knowledge, Eco-impact, Risk) and Lyapunov residual corridors.[file:6][file:2]
- All files are energy-efficient, carbon-negative in intent, and deployable on real telemetry machinery (controllers, edge nodes, schedulers) without direct actuator control.[file:6]

Files
- `cpp/CyboWorkloadKernel.cpp` — C++ numeric kernel for workload risk coordinates and residual slice.
- `java/CyboWorkloadTelemetry.java` — Java telemetry ingest and SQL binding.
- `kotlin/CyboWorkloadWindow.kt` — Kotlin data model and KER normalization utilities.
- `lua/cybo_workload_router.lua` — Lua FOG-router predicates for workload routing lanes.
- `sql/cyboquatic_workload_schema.sql` — SQL schema and indices for workload frames and residual telemetry.
- `aln/workload_energyreq_vt_20260723.aln2` — ALN v2 governance particle binding K,E,R and Lyapunov residual for this domain.

Placement
- Root: `ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/` (discoverable daily shard).[file:6]
- Subdirs: `cpp`, `java`, `kotlin`, `lua`, `sql`, `aln` follow the cyboquaticprogress convention.[file:6]
- Anchoring: a Phoenix hex anchor (e.g., logical name `PHXWORKLOADENERGYDV20260723`) should be registered in `Eco-Fort/db/phoenixhexregistry.sql` and mirrored in `ecorestorationshard/hex/PHXHEXANCHORS.md` after CI computes real file hashes.[file:6][file:2]

Invariants
- Non-actuating: no fieldbus drivers, no direct control of pumps, gates, or actuators — all artifacts operate on telemetry and planning only.[file:6][file:2]
- Strict KER: K,E,R ∈ [0,1], residual \(V_t\) monotone non-increasing under admissible workload changes.[file:6]
- SQL CHECKs and triggers enforce safe ranges for workload energy, duty cycle, and residual bands; Lua predicates never route unsafe frames into production lanes.[file:2]
- All governance binding uses DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` as the root signing identity.[file:2]
