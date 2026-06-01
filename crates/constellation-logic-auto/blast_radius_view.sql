CREATE VIEW blast_radius_risk AS
WITH node_metrics AS (
    SELECT 
        artifact_id,
        COUNT(DISTINCT target_artifact_id) as out_degree,
        (SELECT COUNT(*) FROM blast_radius br2 
         WHERE br2.target_artifact_id = br1.artifact_id) as in_degree
    FROM blast_radius br1
    GROUP BY artifact_id
),
critical_paths AS (
    -- Find longest path using recursive CTE
    WITH RECURSIVE path_lengths AS (
        SELECT source_artifact_id, target_artifact_id, 1 as depth
        FROM blast_radius
        UNION ALL
        SELECT pl.source_artifact_id, br.target_artifact_id, pl.depth + 1
        FROM path_lengths pl
        JOIN blast_radius br ON pl.target_artifact_id = br.source_artifact_id
        WHERE pl.depth < 50
    )
    SELECT source_artifact_id as artifact_id, MAX(depth) as max_path_depth
    FROM path_lengths
    GROUP BY source_artifact_id
)
SELECT 
    nm.artifact_id,
    nm.in_degree as blast_radius,
    nm.out_degree,
    cp.max_path_depth,
    -- Topological risk formula
    (0.5 * CAST(nm.in_degree AS REAL) / (SELECT MAX(in_degree) FROM node_metrics) +
     0.3 * CAST(nm.out_degree AS REAL) / (SELECT MAX(out_degree) FROM node_metrics) +
     0.2 * CASE WHEN cp.max_path_depth = (SELECT MAX(max_path_depth) FROM critical_paths) 
               THEN 1.0 ELSE 0.0 END) as R_topology,
    CASE 
        WHEN nm.in_degree > 10 THEN 'RESEARCH'
        WHEN nm.in_degree > 5 THEN 'STAGING'
        ELSE 'PRODUCTION'
    END as recommended_lane
FROM node_metrics nm
LEFT JOIN critical_paths cp ON nm.artifact_id = cp.artifact_id;
