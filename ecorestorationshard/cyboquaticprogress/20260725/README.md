# 2026-07-25 Cyboquatic Progress Shard (Domain g: Surcharge Blast-Radius)

This folder hosts the non-actuating, daily cyboquatic artifacts for **2026-07-25**, targeting domain **(g) blast-radius tables for surcharge breaches** with SQLite indices and diagnostic C++/Java machinery.  
All artifacts are hex-anchored, DID-bound to `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`, and follow the Phoenix Hex Registry placement rules. [file:11]

## Layout

- `ecorestorationshard/cyboquaticprogress/20260725/cpp/`
  - C++ diagnostic kernel: blast-radius estimation from surcharge events (non-actuating).
- `ecorestorationshard/cyboquaticprogress/20260725/java/`
  - Java telemetry and SQLite integration for blast-radius frames.
- `ecorestorationshard/cyboquaticprogress/20260725/sql/`
  - SQLite schema, indices, and seed rows for `blast_surcharge_radius` and `dailyprogress`.
- `ecorestorationshard/cyboquaticprogress/20260725/aln/`
  - ALN v2 governance particle binding K,E,R and hex evidence to this shard.
- `ecorestorationshard/hex/PHXCYBOHEXANCHORS.md`
  - Manifest entry for the new blast-radius anchor (mirrors `Eco-Fort/db/phoenixhexregistry.sql`). [file:11]

## Domain and Subtask

- Domain: `CYBOQUATIC`
- Subdomain: `BLASTRADIUS_SURCHARGE`
- Date: `20260725`
- Subtask ID (conceptual hash of date): `PHX-CANAL-BR-2026-07-25`
  - Used in SQL `dailyprogress` and ALN particle for cross-day chaining. [file:11]

## Invariants

- Non-actuating:
  - No pump, gate, or actuator calls.
  - All code is diagnostic-only (computes radii, risks, and residuals).
- Lyapunov / KER:
  - Residual \( V_t = \sum_j w_j r_j^2 \) over planes:
    - `r_energy`, `r_hydraulics`, `r_bio`, `r_tox`, `r_uncertainty`, `r_topology`. [file:2]
  - K,E,R triad:
    - K, E, R in \([0,1]\), `kerscore = K + E - R` in \([0,1]\). [file:2]
  - Always-improve:
    - New blast-radius diagnostics must **not increase** residual beyond configured noise bounds before promotion to stronger lanes. [file:7]

## Energy Efficiency and Carbon-Negative Emphasis

- SQL indices are designed for:
  - Time-bounded range scans (`timestamputc`) per FOG region and canal node. [file:2]
  - Minimal page reads for daily and per-region aggregation queries. [file:2]
- C++ kernel:
  - Computes blast-radius using local metrics only (no external IO).
  - Normalizes radii relative to corridor-safe baselines to identify **carbon-negative** mitigation (reduced spread per Joule) when integrated with workload energetics. [file:2]
- Java telemetry:
  - Binds diagnostics to hex anchors and K,E,R without redundant duplication, reducing data movement and storage overhead. [file:11]

## How This Shard Integrates

- Hex Registry:
  - A new `phoenixhexanchor` row for logical name `PHXBLASTRADIUS_SURCHARGE20260725`.
  - `defaultrelpath = 'ecorestorationshard/cyboquaticprogress/20260725'`. [file:11]
- Daily Ledger:
  - Extends `dbcyboquaticdailyprogress.sqlite` with a blast-radius day for 2026-07-25. [file:11]
- Cross-Domain Coupling:
  - Blast-radius risk coordinates feed:
    - Drainage-decay corridors (BOD/TSS/CEC).
    - Workload energetics (energy cost of mitigation).
    - AI node energetics (placement of analytic workloads in tailwind corridors). [file:2][file:7]
