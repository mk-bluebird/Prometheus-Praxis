-- File: sql/cyboquatic/generated/workload_corridor_constraints.sql
-- Generated from aln/cyboquatic/workload_corridor_2026_08_09.aln
-- This clause is equivalent to the acceptance corridor in workload_telemetry_2026_08_09.sql.
CHECK (
    delta_vt <= 0.35
    AND ker_k * ker_e > ker_r
    AND fog_confidence >= 0.75
    AND eco_impact_value >= 0.60
);
