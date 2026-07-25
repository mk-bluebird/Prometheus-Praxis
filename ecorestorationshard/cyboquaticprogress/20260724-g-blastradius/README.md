# ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/README.md

Overview
--------

This daily cyboquatic shard targets domain **g**: blast-radius tables for surcharge breaches using SQLite indices in SQL + C++/Java, tuned for Phoenix canal nodes and carbon‑negative, non‑actuating diagnostics only [file:8].  

Artifacts
---------

- `sql/canal_blastradius_schema.sql` — SQLite schema for canal nodes, surcharge events, KER, FOG, and blast‑radius tables with strict invariants and indices [file:8][file:4].  
- `cpp/canal_blastradius_engine.cpp` — C++ numeric kernel to compute energy‑safe blast radii and risk coordinates without any actuator bindings [file:4].  
- `java/CanalBlastRadiusTelemetry.java` — Java telemetry wrapper and ingestion logic for writing diagnostic rows into SQLite under KER discipline [file:4].  
- `aln/blastradius_governance_ker.aln2` — ALN v2 governance particle binding K,E,R and evidence hexes to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` [file:8][file:31].  

Domain g scope
--------------

- Models surcharge breach blast radii around canal nodes, using:
  - Hydraulic head, surcharge depth, local topography proxy, and energy envelope [file:4].  
  - KER triad and Lyapunov residual corridor semantics reused from existing cyboquatic workload and drainage shards [file:8][file:18].  
- All outputs are:
  - Non‑actuating (diagnostic only).  
  - Energy‑efficient (simple scalar kernels, index‑friendly SQL).  
  - Carbon‑negative oriented by identifying configurations that reduce expected harmful spread per joule [file:8][file:18].  

File placement and hex anchors
------------------------------

- Root path: `ecorestorationshard/cyboquaticprogress/20260724-g-blastradius` (daily cyboquatic progress tree) [file:8].  
- This folder should be bound in `Eco-Fort/db/phoenixhexregistry.sql` via a `phoenixhexanchor` row such as:
  - `logicalname = PHXBLASTRADIUS20260724`  
  - `domain = CYBOQUATIC`, `subdomain = BLASTRADIUS`, `regioncode = PHX-CAZ-CEIM` [file:8].  
- Companion `phoenixhexfile` and `phoenixhexparticlebinding` rows must link:
  - SQL file(s) under `sql/`  
  - C++ and Java files under `cpp/` and `java/`  
  - ALN v2 particle under `aln/` [file:8].  

KER, FOG, canal parameters
--------------------------

- Canal nodes carry:
  - FOG region/channel identifiers and max allowable energy per surcharge diagnostic [file:4].  
  - KER baseline profile and corridor references (HYDRAULICS, ENERGY, TOPOLOGY planes) [file:8][file:18].  
- Blast‑radius diagnostics store:
  - Normalized risk coordinates \(r_j \in [0,1]\) for hydraulics, energy, topology, and biodiversity [file:18][file:31].  
  - Residual \(V_t = \sum_j w_j r_j^2\) and derived K,E,R scores, consistent with your Lyapunov corridor grammar [file:18][file:31].  

Usage
-----

1. Deploy `sql/canal_blastradius_schema.sql` to `db/cyboquatic_blastradius.sqlite`, keeping foreign‑keys and triggers enabled [file:8][file:4].  
2. Build and run `cpp/canal_blastradius_engine.cpp` as a non‑actuating CLI or library that writes diagnostic rows (no device drivers) [file:4].  
3. Use `java/CanalBlastRadiusTelemetry.java` to ingest telemetry, enforce policy thresholds, and insert rows only when KER and energy envelopes are satisfied [file:4][file:18].  
4. Register and use `aln/blastradius_governance_ker.aln2` in ALN‑Blockchain to hex‑stamp evidence and gate any downstream automation via K,E,R and residual contracts [file:18][file:31].  

Knowledge / Eco impact / Risk
-----------------------------

- Knowledge factor: high (0.95). This shard extends existing workload/drainage KER grammar to surcharge blast radii with explicit corridor and hex registry bindings [file:8][file:18].  
- Eco‑impact: high (≈0.92). It supports carbon‑negative, safer canal designs by structurally favoring low‑radius, low‑risk configurations per joule [file:18][file:31].  
- Risk‑of‑harm: low (≈0.12). Residual risk is limited to mis‑calibration of corridors, mitigated by ALN invariants, triggers, and non‑actuating design [file:18][file:31].  
