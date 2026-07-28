# CYBOQUATICENGINEPLACEMENT.md

## Overview

Cyboquatic C++ engines under `src/cpp` are non-actuating numeric kernels that feed Prometheus-Praxis governance crates and EcoNet layers via Rust/Java FFI, ALN v2 particles, and SQLite DB spines. They never talk directly to hardware; all actuation is delegated to separately governed stacks.

This document describes how each cyboquatic engine file maps into EcoNet layers and Phoenix hex anchors, making discovery deterministic for AI-chat agents and contributors.

## Engine files and roles

### Workload engine

- Path: `src/cpp/cyboquatic_workload_engine.cpp`
- Role band: `ENGINE`, `RESEARCH`
- EcoNet layer:
  - `layername` = `CyboquaticWorkloadEngine`
  - `layertier` = `NUMERIC-ENGINE`
  - `languages` = `C++`, `Rust`
- Phoenix hex anchor:
  - Logical name: `PHXWORKLOADENGINE20260709`
  - Domain: `CYBOQUATIC`
  - Subdomain: `WORKLOADENERGYDV`
  - Default path: `src/cpp/cyboquatic_workload_engine.cpp`
- ALN binding:
  - Spec: `aln/alnCyboquaticWorkloadKernel2026v1.aln2`
  - Particle: `cyboquatic.workload.kernel`
  - Fields: `nodeId`, `windowStartUtc`, `windowEndUtc`, `energyReqJ`, `vtCurrent`, `vtNext`, `vtDelta`, `k`, `e`, `r`, `kerScore`, `lane`, `safeToPromote`, `evidenceHex`

The workload engine computes normalized risk coordinates and Lyapunov residuals for workload windows; Rust governance crates consume these outputs, map them into KER triads via ALN, and write them into `db/dbcyboquaticdailyprogress.sql`.

### Drainage-decay engine

- Path: `src/cpp/cyboquatic_drainagedecay_engine.cpp`
- Role band: `ENGINE`, `RESEARCH`
- EcoNet layer:
  - `layername` = `DrainageDecayEngine`
  - `layertier` = `NUMERIC-ENGINE`
  - `languages` = `C++`, `Rust`
- Phoenix hex anchor:
  - Logical name: `PHXDRAINAGEDECAYENGINE20260708`
  - Domain: `HYDRO`
  - Subdomain: `DRAINAGEDECAY`
  - Default path: `src/cpp/cyboquatic_drainagedecay_engine.cpp`
- ALN binding:
  - Spec: `aln/alnCyboquaticDrainageDecayKernel2026v1.aln2`
  - Particle: `cyboquatic.drainagedecay.kernel`
  - Fields: `frameId`, `canalNodeId`, `timestampUtc`, `bodMgL`, `tssMgL`, `cecCmolPerKg`, `frameEnergyJ`, `deltaVtMps`, `k`, `e`, `r`, `kerScore`, `fogRegionId`, `fogChannelId`, `evidenceHex`

The drainage-decay engine implements BOD/TSS/CEC frame logic and produces KER and residual slices consumed by ecosafety crates and written into `db/dbcyboquaticdrainagedecayindex.sql`.

### Blast-radius engine

- Path: `src/cpp/cyboquatic_blastradius_engine.cpp`
- Role band: `ENGINE`, `RESEARCH`
- EcoNet layer:
  - `layername` = `CyboquaticBlastRadiusEngine`
  - `layertier` = `NUMERIC-ENGINE`
  - `languages` = `C++`, `Rust`, `Java`
- Phoenix hex anchor:
  - Logical name: `PHXBLASTRADIUSCYBOQUATIC2026`
  - Domain: `GOV`
  - Subdomain: `CYBOQUATIC`
  - Default path: `src/cpp/cyboquatic_blastradius_engine.cpp`
- ALN binding:
  - Spec: `aln/alnCyboquaticBlastRadiusGovernance2026v1.aln2`
  - Particle: `cyboquatic.blastradius.governance`
  - Fields: `blastIndexId`, `nodeId`, `eventId`, `corridorId`, `laneId`, `radiusM`, `radiusNorm`, `k`, `e`, `r`, `kerScore`, `residualKer`, `rohCoordinate`, `radiusWithinLimit`, `kerWithinLimit`, `laneAdmissibleOk`, `safeToPromoteOk`, `evidenceHex`

The blast-radius engine provides surcharge breach diagnostics and KER/RoH flags that are persisted in `db/dbcyboquaticblastradiusindex.sql` and surfaced via EcoNet spines for governance decisions.

## econetrepoindex.sql integration

Each engine is registered in `.econet/econetrepoindex.sql` under the Prometheus-Praxis workspace with rows like:

- Repo: `prometheuspraxisai`
- Layers:
  - `CyboquaticWorkloadEngine` (NUMERIC-ENGINE, C++/Rust)
  - `DrainageDecayEngine` (NUMERIC-ENGINE, C++/Rust)
  - `CyboquaticBlastRadiusEngine` (NUMERIC-ENGINE, C++/Rust/Java)

These declarations tell AI-chat agents and tools:

- Which files implement numeric kernels.
- Which layers consume their outputs.
- That all engines are non-actuating and governed by KER/Lyapunov corridors.

## Placement rules for new cyboquatic engines

- Place new C++ numeric kernels under `src/cpp` with names `cyboquatic_<band>_engine.cpp`.
- Define matching ALN v2 particles under `aln/` and bind them to Bostrom DIDs and Phoenix anchors.
- Add EcoNet layer rows in `.econet/econetrepoindex.sql` with clear `roleband` and `layertier`.
- Ensure DB schemas and views in `db/` mirror ALN fields (node, window, K,E,R, Vt, lane, evidenceHex).

These rules keep cyboquatic engines discoverable, hex-anchored, and governance-aligned across the monorepo.
