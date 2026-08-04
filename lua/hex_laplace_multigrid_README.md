# Discrete Laplace Hex-Anchor Solver and Cold-Survival Corridor Schema

## Hex-Anchor Consistency as Discrete Laplace/Poisson Equation

- In the Lua shard `lua/hex_laplace_multigrid.lua`, we model hex-anchor consistency via a discrete Poisson problem:
  - For each H3 hex-cell `i` with green_fraction `g_i` and LST anomaly `ΔT_i`, equilibrium zero thermal gradient leads to:
    - `deg(i) * g_i - Σ_{j∈N(i)} g_j = -f_i`, where `f_i = f(ΔT_i)`.
  - This is the discrete Laplace equation `L g = -f` on the hex adjacency graph, with `L` the graph Laplacian.
- Data structures:
  - `HexGrid.cells[h3_index] = { g, deltaT, neighbors = {h3_j1, ...} }` encodes adjacency and state for each hex.
- Solver:
  - Gauss-Seidel relaxation updates `g_i` using neighbor values and source term `f_i`.
  - A simple multigrid-like V-cycle (`v_cycle`) performs pre-relaxation, residual computation, direct correction, and post-relaxation; in a full implementation, coarse grids would aggregate hex-cells (lower H3 resolution) with restriction and prolongation.
- This discrete Laplace formulation ensures hex-anchor consistency and provides a scalable Lua-based solver for spatial green-fraction optimisation under thermal constraints.

## Cold-Survival Corridor Monitoring Schema

- The SQL shard `sql/cold_survival_corridor.sql` defines:
  - `cold_survival_corridor` table with fields:
    - `canal_segment`, `timestamp_s`, `lyapunov_exponent`, `cold_survival_flag`,
      `pfas_half_life_days`, and governance signature `did`.
  - An index on `(canal_segment, timestamp_s)` for efficient time-series queries.
- The view `cold_survival_heat_stress`:
  - Joins `cold_survival_corridor` with `hex_thermal_recovery` on `basin_id`/`canal_segment`.
  - Filters for `cold_survival_flag = 1` and high `cooling_degree_hours` (e.g., > 500 K·h).
  - Identifies canal segments where PFAS cold-survival coincides with strong heat-island stress, highlighting areas where cyboquatic interventions must balance pollutant stability with thermal mitigation.

Technical justification: The discrete Laplace/Poisson formulation on H3 adjacency provides a rigorous hex-anchor optimisation framework, solvable in Lua with multigrid-style cycles for energy-efficient green-fraction allocation. The cold-survival corridor schema tightly integrates Lyapunov-based PFAS dynamics with hex thermal recovery metrics, allowing governance and operations to detect corridors that are chemically stable yet thermally stressed, enabling targeted eco-restoration strategies.
