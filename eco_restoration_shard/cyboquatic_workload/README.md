# Cyboquatic Workload Shard (energyreqJ, ΔVt)
==========================================

This folder contains a daily cyboquatic workload shard for canal-bound industrial
machinery in the Phoenix hex registry, focused on energy-efficient, carbon-negative
operation and telemetry of:

- energyreqJ: Per-cycle mechanical/thermal energy requirement in Joules.
- ΔVt: Lyapunov-style residual change in workload risk coordinates over time.

Contents:

1. C++ core model and CLI:
   - cpp/eco_restoration/cyboquatic_workload_model.cpp
     * Models workload cycles for canal machinery (pumps, mixers) with
       energyreqJ estimates and ΔVt computation based on risk planes
       (hydraulics, energy, topology, biodiversity).
     * Provides a CLI to read/write SQLite telemetry rows, enforcing KER-style
       invariants (K knowledge-factor, E eco-impact, R residual risk).

2. Java telemetry bindings:
   - java/cyboquatic/TelemetryClient.java
     * Simple JDBC-style client for inserting and querying workload telemetry
       rows, compatible with the cyboquatic_workload schema.
       (No external frameworks required beyond a standard SQLite JDBC driver.)

3. Kotlin FOG predicates:
   - kotlin/cyboquatic/FogRouter.kt
     * Evaluates FOG-router predicates (Fat, Oil, Grease) plus unmodeled media
       flags against workload telemetry, ensuring that flows with high FOG or
       unknown contaminants are routed to safer processing corridors.

4. Lua lightweight router:
   - lua/cyboquatic/fog_router.lua
     * Lua script for embedded controllers to evaluate FOG predicates and
       recommend safe routing actions without direct actuation (advisory only).

5. SQL schema and invariants:
   - sql/cyboquatic_workload_schema.sql
     * SQLite schema for:
       - workload_cycle (per-cycle telemetry, energyreqJ, ΔVt)
       - canal_node (Phoenix canal node metadata with hex_id, KER parameters)
       - ker_window (aggregated K, E, R scores per hex and window)
       - fog_flow (FOG and unmodeled media metrics)
     * Enforces strict invariants:
       - KER consistency per cycle (CHECK + triggers)
       - FOG and Canal node parameters (CHECK constraints on ranges)
       - Owner DID binding to bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
       - Non-actuating, telemetry-only semantics.

6. ALN v2 governance particle:
   - aln/cyboquatic_workload_ker.aln
     * ALN v2 governance particle binding canal workload cycles to
       DID bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7 with an explicit
       K,E,R triad for each canal node and workload corridor.

Usage:

- Deploy the SQL schema on a local SQLite database (eco_restoration_shard.db).
- Use the C++ CLI to insert and query workload cycles and compute ΔVt.
- Use Java/Kotlin/Lua artifacts for integration with existing telemetry stacks.
- Use ALN v2 governance particle to formally specify allowable corridors and
  windows for energyreqJ and ΔVt per Phoenix hex.

Energy and Eco-Impact:

- Models emphasize energy-efficient, carbon-negative operation by:
  - Penalizing high energyreqJ per useful flow and encouraging low-impact cycles.
  - Aggregating eco-impact scores E with ready-biodegradability factors and
    drainage safety bands.
  - Preventing unsafe ΔVt increases via KER invariants and corridor predicates.

All artifacts avoid disallowed primitives (hash algorithms, digital twins, etc.)
and are suitable for real-world, non-actuating telemetry and advisory control.
