# Hex Thermal Recovery Telemetry Schema

This schema defines the `hex_thermal_recovery` SQLite table that links H3 hex indices to cyboquatic workload telemetry, capturing morning and afternoon land-surface temperatures, surface albedo, and cooling-degree-hours for urban heat-island recovery analysis.

## Tables

- `cyboquatic_workload_telemetry`  
  Baseline telemetry table for cyboquatic workloads (flow, head, energy, ΔVt).

- `hex_cell_catalog`  
  Catalog mapping H3 indices to basin identifiers and geographic centers.

- `hex_thermal_recovery`  
  Heat-island recovery telemetry per hex cell, including morning LST, afternoon LST, albedo, and cooling-degree-hours, with a physics-based CHECK constraint limiting CDH to ≤ 650 K·h.

## Spatial Linking

Hex cells are linked to cyboquatic telemetry via `h3_index` and `basin_id`, enabling cross-queries such as:

- Evaluating how cyboquatic workloads affect thermal recovery in specific H3 cells.
- Aggregating cooling-degree-hours per basin over time for carbon-negative planning.

## ker_e Stochastic Process and Governance

The ALN v2 shard `ker_e_stochastic_process.aln` models `ker_e` as a cumulative random-walk process driven by net carbon flux samples under log-normal flow, and defines a CUSUM-based governance trigger when `ker_e` drifts away from the carbon-negative regime. This aligns telemetry schema design with long-horizon eco-impact monitoring.
