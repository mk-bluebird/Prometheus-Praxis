# Hex Restoration Commitment Entity and Lua Corridor Planning

## Hex Restoration Commitment (ALN v2 + SQL)

- `aln_v2/hex_restoration_commitment.aln` defines the `HexRestorationCommitment` entity:
  - Fields:
    - `h3_index`: hex-cell identifier.
    - `did`: governance DID (bound to `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`).
    - `interventions`: description of cyboquatic interventions (canopy, cool pavement, aeration).
    - `target_ker_e`: carbon-negative eco-impact target (≤ 0).
    - `target_lst_drop_k`: target LST reduction (K).
    - `start_date_utc`, `end_date_utc`: commitment period.
  - Invariants:
    - DID binding.
    - Carbon-negative target.
    - Progress linkage to `HexThermalRecoveryTelemetry` (backed by `hex_thermal_recovery`).

- `sql/hex_restoration_commitment.sql` serialises this entity into `hex_restoration_commitment` and defines `hex_restoration_progress` view:
  - Joins commitments with `hex_thermal_recovery` and `hex_lst_baseline`.
  - Computes average afternoon LST drop over the commitment period.
  - Classifies progress as `ON_TARGET` or `BELOW_TARGET`, supporting governance reporting and eco-restoration tracking.

## Lua Hexagonal Grid Traversal for Restorative Corridors

- `lua/hex_corridor_planning.lua` implements a corridor planner:
  - Uses a Lua `hex_state` table keyed by H3 indices with:
    - `green_fraction`, `deltaT` (LST anomaly), `cost_per_kJ`, `visited`.
  - Neighbourhood exploration uses H3 adjacency functions (`h3_neighbors`), conceptually ported from H3 to Lua.
  - Marginal benefit for a hex is defined as:
    - `benefit = deltaT / cost_per_kJ`, representing LST reduction per kJ of restorative effort.
  - Traversal:
    - Depth-first/stack-based exploration starting from a given hex.
    - Visits neighbours while marginal benefit ≥ threshold.
    - Accumulates restoration cost and records `(h3_index, benefit, cumulative_cost)` per step.
  - Results are written to a SQLite `corridor_plan` table via `sqlite3` commands, enabling downstream visualisation and optimisation.

Technical justification: The Hex Restoration Commitment entity binds spatial hex anchors to explicit cyboquatic interventions and carbon-negative targets, with ALN invariants and SQL views providing a verifiable, queryable progress reporting layer. The Lua corridor planner traverses the hexagonal grid using H3 adjacency, computing marginal benefits and cumulative costs to design restorative corridors that stop when energy-efficiency (LST reduction per kJ) drops below a governance-defined threshold, creating actionable corridor plans stored in SQL for further analysis and deployment.
