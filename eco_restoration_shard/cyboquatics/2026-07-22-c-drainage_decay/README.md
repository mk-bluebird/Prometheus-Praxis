# eco_restoration_shard/cyboquatics/2026-07-22-c-drainage_decay/README.md

## Overview

This shard defines a cyboquatic `drainagedecay` telemetry frame set for biodegradable, carbon-negative drainage infrastructures deployed in mixed urban–riparian canals.  
The focus for 2026-07-22 is domain (e): `drainagedecay` frames (BOD, TSS, CEC) in Kotlin/Java + SQL, with strict KER (Knowledge, Eco-impact, Risk) and FOG (Field-Operational-Grid) invariants on every canal node.

All artifacts are:

- Language-complete (Kotlin, Java, SQL, ALN v2) without Rust or Python.
- Energy-efficient and deployable on low-power edge machinery.
- Anchored to the Prometheus-Praxis mono-repo structure using hex-anchored file paths that can be discovered via `phoenix_hex_registry.md`.

## Files

- `kotlin/DrainageDecayFrame.kt` – Kotlin data model and validation logic for canal drainage frames.
- `java/DrainageDecayIngestor.java` – Java ingestion and rule-engine for telemetry from field controllers.
- `sql/drainagedecay_schema.sql` – SQL schema for frames, canal nodes, KER, and FOG invariants with indices.
- `aln/drainagedecay_ker_hex.aln2` – ALN v2 spec binding KER triads and governance particles to `drainagedecay` channels.

## Domain (e) Scope

- Parameters:
  - BOD (Biochemical Oxygen Demand, mg/L)
  - TSS (Total Suspended Solids, mg/L)
  - CEC (Cation Exchange Capacity, cmol(+)/kg)
- Constraints:
  - All frames must be linked to a canal node with geo-ids and energy constraints.
  - KER scores are mandatory and must be within normalized ranges [0, 1].
  - FOG dimensions define operational capacity and energy envelopes for each node.

## Invariants

- No frame is stored without:
  - Referential integrity to a canal node.
  - Fully-populated KER and FOG entries.
  - Energy envelope metadata that can be used to maintain carbon-negative operation (e.g., max energy per frame, allowable duty cycle).
- SQL CHECK constraints enforce allowable ranges for BOD, TSS, and CEC to avoid equipment misuse and ecological harm.
- Kotlin and Java layers both implement the same validation logic for redundancy and hardware safety.

## KER / Eco Impact Scoring

For every frame:

- `k_knowledge_factor` in [0.0, 1.0] quantifies how informative the measurement is (0 = trivial, 1 = maximally informative within current model).
- `e_eco_impact` in [0.0, 1.0] quantifies potential positive impact (0 = neutral, 1 = strongly positive) based on remediation targets.
- `r_risk_factor` in [0.0, 1.0] quantifies potential harm (0 = no harm, 1 = critical risk).

A canonical `ker_score` is computed as:

- `ker_score = k_knowledge_factor * (e_eco_impact - r_risk_factor)`

Nodes and fleets must be configured to reject frames whose `ker_score` is negative or below an operator-defined threshold.

## Energy Efficiency

- Frame-level energy budget fields (`frame_energy_j`, `delta_vt_mps`) define the energy and velocity transitions required.
- Kotlin and Java logic ensure:
  - Duty cycles stay within safe limits.
  - Frames that would exceed energy budgets are rejected or down-sampled.
  - All controllers can run on constrained CPUs without requiring heavy dependencies.

## DID / Governance

- This shard is bound to the Bostrom DID:
  - `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- The ALN file defines governance particles that:
  - Hex-stamp each frame to a governance scope.
  - Require KER scoring for any downstream actuation logic.
  - Ensure derivatives respect data sovereignty and augmented-citizen rights.

## Usage

1. Deploy the SQL schema to a PostgreSQL or SQLite backend that respects foreign-key constraints and energy limits.
2. Compile and deploy the Kotlin and Java modules on edge machinery and supervisory controllers.
3. Use the ALN v2 file to:
   - Define on-chain / off-chain governance rules.
   - Anchor frame hashes to Bostrom identities.
   - Verify KER compliance before enacting any drainage or remediation command.

## Knowledge / Risk Discipline

- Knowledge factor: High. This shard directly supports optimization of drainage operations using core ecological parameters.
- Eco-impact: High positive potential when used to guide low-energy, biodegradable materials and configurations.
- Risk: Mitigated by:
  - Hard constraints on harmful ranges (upper BOD/TSS thresholds).
  - Mandatory risk factor encoding and rejection of negative KER scores.
  - Explicit avoidance of any blacklisted algorithms or digital twin paradigms.
