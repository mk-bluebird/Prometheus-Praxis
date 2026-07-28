<!-- filename: ecorestorationshard/cyboquaticprogress/20260727/README.md
     purpose: Root-level description for 20260727 cyboquatic workload energetics shard
     domain: (d) Cyboquatic workload energetics
     anchor: PHXWORKLOADENERGYDV20260727 / 0x20260727PHX3345NWorkloadEnergyDeltaVt -->

# 2026-07-27 Cyboquatic Workload Energetics Shard

This directory is a daily, non-actuating cyboquaticprogress shard for 2026-07-27, targeting domain (d) Cyboquatic workload energetics (energyreqJ, ΔVt) under Phoenix corridors and Eco-Fort grammar.[file:13][file:9]

- `cpp/cyboquatic_workload_energyreq.cpp` computes normalized risk coordinates (r_energy, r_hydraulics, r_uncertainty), Lyapunov residual `vt_after`, and `delta_vt` for canal workloads using quadratic weights consistent with prior workload bands.[file:9]
- `java/CyboquaticWorkloadTelemetry.java` ensures the `dailyprogress` SQLite schema and inserts workloads slices with Phoenix evidence hex and prior-anchor linkage into `db_cyboquaticdailyprogress.sqlite`.[file:13]
- `kotlin/CyboquaticWorkloadSummary.kt` summarizes daily workloads into AI-chat-friendly JSON, exposing counts and averages while preserving hex-stamped evidence strings.[file:13]
- `lua/fog_router_workload.lua` defines FOG-router predicates for lane decisions (`FORBID`, `RESEARCH`, `PILOT`, `PROD`) based on risk planes and `delta_vt`, maintaining non-actuating governance for diagnostics.[file:2]
- `sql/cyboquatic_workload_dailyprogress_seed.sql` expands the `dailyprogress` shard with a concrete seed row for 2026-07-27 and indices aligned with the Phoenix hex registry placement strategy.[file:13]
- `aln/WorkloadEnergyDeltaVt20260727v1.aln` binds the workload slice to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`, enforcing K,E,R bounds and an always-improve contract `delta_vt <= 0` under the `PHXWORKLOADENERGYDV20260727` anchor.[file:2][file:13]

Placement follows the Phoenix Hex Anchors Registry: the logical name `PHXWORKLOADENERGYDV20260727` is mapped to `ecorestorationshard/cyboquaticprogress/20260727` as `defaultrelpath`, with this README.md providing discoverable context for Prometheus-Praxis agents and collaborators.[file:13]
