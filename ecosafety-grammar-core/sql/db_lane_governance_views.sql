-- ecosafety-grammar-core/sql/db_lane_governance_views.sql

CREATE VIEW IF NOT EXISTS v_lane_admissibility AS
WITH latest_window AS (
    SELECT
        s.shard_id,
        s.repo_name,
        s.node_id,
        s.lane,
        rv.r_energy,
        rv.r_hydraulics,
        rv.r_biology,
        rv.r_carbon,
        rv.r_materials,
        rv.r_biodiversity,
        rv.r_data,
        rv.r_topology,
        rv.V_t,
        rv.k_metric,
        rv.e_metric,
        rv.r_metric,
        s.policy_id,
        ROW_NUMBER() OVER (PARTITION BY s.shard_id ORDER BY s.t_end_utc DESC) AS rn
    FROM shardinstance s
    JOIN riskvector rv ON rv.shard_id = s.shard_id
)
SELECT
    lw.shard_id,
    lw.repo_name,
    lw.node_id,
    lw.lane,
    lw.k_metric,
    lw.e_metric,
    lw.r_metric,
    CASE
        WHEN lw.k_metric >= 0.90 AND lw.e_metric >= 0.90 AND lw.r_metric <= 0.13 THEN 1
        ELSE 0
    END AS ker_gate_ok,
    CASE
        WHEN lw.r_topology <= pw_topology.r_gold THEN 1
        ELSE 0
    END AS topology_gate_ok
FROM latest_window lw
JOIN planeweights pw_topology
  ON pw_topology.policy_id = lw.policy_id
 AND pw_topology.plane_id = 'TOPOLOGY'
WHERE lw.rn = 1;
