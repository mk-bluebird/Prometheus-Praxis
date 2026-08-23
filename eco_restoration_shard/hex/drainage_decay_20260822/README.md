<!-- Repository: mk-bluebird/Prometheus-Praxis -->
<!-- Filename: eco_restoration_shard/hex/drainage_decay_20260822/README.md -->
<!-- Destination: eco_restoration_shard/hex/drainage_decay_20260822/ -->

# Drainage Decay Frame Set — 2026-08-22

This artifact set implements date-domain `(e)`: `drainagedecay` frames for biochemical oxygen demand (BOD), total suspended solids (TSS), cation-exchange capacity (CEC), machine energy demand (`energyreqJ`), and supply-voltage change (`deltaVt`).

## Files

- `../../../cpp/eco_restoration/DrainageDecay20260822.cpp`: C++17-compatible deterministic command-line projector
- `../../../java/eco_restoration/DrainageDecay20260822.java`: Java 17-compatible deterministic command-line projector
- `../../../kotlin/eco_restoration/DrainageDecay20260822.kt`: Kotlin/JVM deterministic command-line projector
- `../../../lua/eco_restoration/drainage_decay_20260822.lua`: Lua 5.4 module for local controllers and report pipelines
- `../../../sql/eco_restoration/drainage_decay_20260822.sql`: SQLite schema, indices, strict validation trigger, KER/FOG/Canal-node constraints
- `../../../aln/eco_restoration/drainage_decay_20260822.aln`: ALN v2-oriented invariant and decision-predicate specification

## Operational Boundary

The calculations are decision-support estimates, not substitutes for calibrated field sampling, regulatory approval, equipment interlocks, or operator judgment. Unmodeled or contained FOG media are restricted from restoration status by the SQL and ALN predicates.

## Metrics

- `knowledge_factor` measures measurement completeness and voltage stability on a 0–1 scale.
- `eco_impact_value` measures BOD/TSS reduction, CEC recovery, and energy restraint on a 0–1 scale.
- `harm_risk` estimates the likelihood that water-quality, energy, or stability constraints require intervention, on a 0–1 scale.
