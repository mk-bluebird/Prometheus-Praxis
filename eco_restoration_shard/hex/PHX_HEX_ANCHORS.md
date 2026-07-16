<!-- filename: eco_restoration_shard/hex/PHX_HEX_ANCHORS.md
     destination: eco_restoration_shard/hex/PHX_HEX_ANCHORS.md
     repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

     Purpose:
       Human- and AI-readable manifest mirroring the SQLite registry,
       so collaborators and chat agents can quickly pick the correct
       anchor, paths, and particles without querying SQL directly.[file:36] -->

# Phoenix Hex Anchors Manifest

This manifest mirrors `Eco-Fort/db/phoenix_hex_registry.sql`.  
The SQLite registry is authoritative; this file is a convenience layer for humans and agents.[file:36]

## PHX_DRAINAGE_DECAY_20260708

- Logical name: `PHX_DRAINAGE_DECAY_20260708`.[file:2]
- Evidence hex: `0x20260708PHX3345NDrainageDecayBODTSSCEC`.[file:2]
- Domain / subdomain: `HYDRO / DRAINAGE_DECAY`.[file:2]
- Region: `PHX-CAZ-CEIM`.[file:36]
- Planes: `HYDRAULICS,ENERGY,DATA`.[file:2][file:31]
- Default path: `eco_restoration_shard/cyboquatic_progress/20260708`.[file:2]
- Files (examples, keep in sync with registry):
  - SQL: `eco_restoration_shard/cyboquatic_progress/20260708/sql/daily_drainage_decay_seed.sql`.[file:2]
  - ALN: `eco_restoration_shard/cyboquatic_progress/20260708/aln/DrainageDecayFrame20260708v1.aln`.[file:2][file:35]
  - RUST/CRATE: `eco_restoration_shard/cyboquatic_progress/20260708/crate/src/lib.rs`.[file:2]
- Particle bindings:
  - Particle: `DrainageDecayFrame20260708v1` (role: `DRAINAGE_DECAY`).[file:35]
  - Evidence table: `CyboquaticDrainageDecay20260708v1` (column: `evidencehex`).[file:2]

## PHX_WORKLOAD_ENERGY_DV_20260709

- Logical name: `PHX_WORKLOAD_ENERGY_DV_20260709`.[file:1]
- Evidence hex: `0x20260709PHX3345NWorkloadEnergyDeltaVt`.[file:1]
- Domain / subdomain: `CYBOQUATIC / WORKLOAD_ENERGY_DV`.[file:1]
- Region: `PHX-CAZ-CEIM`.[file:36]
- Planes: `ENERGY,HYDRAULICS,DATA`.[file:1][file:32]
- Default path: `eco_restoration_shard/cyboquatic_progress/20260709`.[file:1]
- Files (examples, keep in sync with registry):
  - SQL: `eco_restoration_shard/cyboquatic_progress/20260709/sql/daily_workload_energy_dv_seed.sql`.[file:1]
  - ALN: `eco_restoration_shard/cyboquatic_progress/20260709/aln/WorkloadEnergyDeltaVt20260709v1.aln`.[file:1][file:35]
  - RUST/CRATE: `eco_restoration_shard/cyboquatic_progress/20260709/crate/src/lib.rs`.[file:1]
- Particle bindings:
  - Particle: `WorkloadEnergyDeltaVt20260709v1` (role: `WORKLOAD`).[file:35]
  - Evidence table: `CyboquaticWorkloadEnergyDeltaVt20260709v1` (column: `evidencehex`).[file:1]

## Registry Core Anchor (to be added)

- Suggested logical name: `PHX_HEX_REGISTRY_CORE_2026`.[file:36]
- Suggested evidence hex: `0xPHXHEXREGISTRYCORE2026` (register in SQLite before use).[file:36]
- Domain / subdomain: `GOV / HEX_REGISTRY`.[file:36]
- Region: `PHX-CAZ-CEIM`.[file:36]
- Planes: `TOPOLOGY,GOV,DATA`.[file:36]
- Default path: `Eco-Fort/db`.[file:36]
- Files:
  - SQL: `Eco-Fort/db/phoenix_hex_registry.sql`.[file:36]

> When adding new anchors, always:
> - Insert into `phoenix_hex_anchor` in the registry.
> - Add at least one `phoenix_hex_file` binding with the correct `relpath`.
> - Optionally add a `phoenix_hex_particle_binding` row.
> - Mirror the anchor here for human/agent convenience.[file:36]
