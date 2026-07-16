# File: aln/README.md
# Destination: Prometheus-Praxis/aln/README.md

# ALN Specs for KER Composition and Governance

This directory contains ALN control documents that define how KER (Knowledge, Eco-impact, Risk) evidence is represented, combined, and governed across the Prometheus-Praxis ecosafety spine.

All files in this directory are non-actuating governance artifacts: they define particles, functions, and invariants, but do not interact with hardware or actuators.

## Files

- `KERComposition2026v1.aln`  
  Defines the base KER particle schema and the compositional KER algebra:
  - `KERParticle2026v1` — a single KER-bounded evidence shard with `K`, `E`, `R`, `evidence_hex`, and `signing_hex`.
  - `KERComposition2026v1` — a composite KER particle that records the combination of two base particles.
  - `ker_oplus_geom_min_max` — the composition operator:
    - `K_combined = sqrt(K1 * K2)` (geometric mean),
    - `E_combined = min(E1, E2)`,
    - `R_combined = max(R1, R2)`.
  - Invariants:
    - `ker_combine_risk_cap` — if both parents satisfy `R <= theta`, the combined particle must satisfy the same cap.
    - `ker_combine_K_E_bounds` — `K_combined` lies between the parent Ks; `E_combined` is no better than either parent.
    - `ker_combine_R_monotone` — `R_combined` is at least as large as each parent’s `R`.
    - `ker_combine_provenance` — `evidence_hex` commits to parent IDs, their `evidence_hex` values, combined `K,E,R`, and the algebra `rule_id`.
    - `ker_combine_lane_safety` — production-lane composites must be built only from production-lane parents.

## Purpose

These ALN specs provide a formally defined, machine-checkable algebra for aggregating KER evidence:

- **Risk monotonicity** — risk of harm can never be reduced by composition; `R_combined` is always at least as large as the riskiest input.
- **Non-compensation** — eco-impact and knowledge cannot be inflated by combining strong and weak evidence:
  - Eco-impact is capped by the worst contributor (`E_combined = min(E1,E2)`).
  - Knowledge is aggregated conservatively using the geometric mean.
- **Provenance** — every composite particle is hex-stamped (`evidence_hex`) and signed (`signing_hex`) under a DID whose private key is confined to a secure enclave or TEE layer.

The invariants ensure that any attempt to produce an inconsistent or unsafe composition (e.g., lowering `R`, inflating `E`, or tampering with `evidence_hex`) fails validation at ALN-level and in CI.

## Integration in the Ecosystem

- **Rust / SQLite side**  
  - Rust crates ingest `KERParticle2026v1` rows (from CSV shards or SQLite views) and implement `ker_oplus_geom_min_max` as a pure function consistent with this ALN spec.
  - Composite KER rows written into governance shards must:
    - Use canonical member ordering for IDs (`members` field).
    - Recompute `evidence_hex` exactly as defined in `KERComposition2026v1.aln`.
    - Store `signing_hex` produced by a secure enclave or TEE-bound signer.

- **CI / Governance**  
  - CI jobs load `KERComposition2026v1.aln` to:
    - Verify that any new or modified composite rows respect the `K`, `E`, `R` invariants.
    - Reject builds where `R_combined` exceeds the corridor cap `theta` while parents claim to be safe.
    - Ensure `evidence_hex` matches the parent hashes and composition rule, preserving provenance.

- **Lane semantics**  
  - Lane tags (`RESEARCH`, `PILOT`, `PROD`) are attached to both base and composite particles.
  - `ker_combine_lane_safety` ensures that production composites cannot be formed from research-only evidence, keeping experimental shards from silently influencing production governance.

## Naming and Discoverability

- All ALN KER specs live under `aln/` with names of the form:
  - `KER*.aln` — KER-related particles and invariants.
  - `EcoCore*.aln` — corridor and residual envelopes.
  - `KnowledgeFactorKernel*.aln` — knowledge kernels for K factors.

This file naming convention keeps KER governance artifacts easy to discover and index via the EcoKnowledgeShardIndex (EKSI) and related SQLite indexes.

## Extension Guidelines

When extending this directory:

- Add new ALN files with explicit `version` suffixes (e.g., `KERComposition2030v1.aln`) rather than modifying existing specs in place.
- Maintain backward compatibility by:
  - Keeping old `rule_id` values stable.
  - Introducing new rules via new `rule_id`s and new composition functions.
- Update CI configurations to:
  - Recognize new composition rules.
  - Enforce invariants for all active versions.

This directory is the canonical, repo-local source of truth for how KER evidence is structured, combined, and constrained across the Prometheus-Praxis ecosafety ecosystem.
