-- File: sql/cyboquatic/verify_workload_corridor_alignment.sql
SELECT CASE
    WHEN sql LIKE '%delta_vt <= 0.35%'
     AND sql LIKE '%ker_k * ker_e > ker_r%'
     AND sql LIKE '%fog_confidence >= 0.75%'
     AND sql LIKE '%eco_impact_value >= 0.60%'
    THEN 'ALIGNED'
    ELSE 'MISMATCH'
END AS aln_sql_corridor_alignment
FROM sqlite_schema
WHERE type = 'table'
  AND name = 'cyboquatic_workload_frame';
