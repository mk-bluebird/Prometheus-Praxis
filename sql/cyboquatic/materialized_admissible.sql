-- File: sql/cyboquatic/materialized_admissible.sql
CREATE VIEW IF NOT EXISTS v_cyboquatic_workload_admissible AS
SELECT
    frame_id,
    observed_utc,
    node_id,
    canal_node,
    energyreq_j,
    delta_vt,
    knowledge_factor,
    eco_impact_value,
    ker_k,
    ker_e,
    ker_r,
    fog_confidence
FROM cyboquatic_workload_frame
WHERE accepted = 1
  AND delta_vt <= 0.35
  AND knowledge_factor >= 0.75
  AND eco_impact_value >= 0.60
  AND fog_confidence >= 0.75
  AND ker_k * ker_e > ker_r;

CREATE TABLE IF NOT EXISTS m_cyboquatic_workload_admissible (
    frame_id INTEGER PRIMARY KEY,
    observed_utc TEXT NOT NULL,
    node_id TEXT NOT NULL,
    canal_node TEXT NOT NULL,
    energyreq_j REAL NOT NULL,
    delta_vt REAL NOT NULL,
    knowledge_factor REAL NOT NULL,
    eco_impact_value REAL NOT NULL,
    ker_k REAL NOT NULL,
    ker_e REAL NOT NULL,
    ker_r REAL NOT NULL,
    fog_confidence REAL NOT NULL,
    refreshed_utc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_m_cyboquatic_admissible_node_time
ON m_cyboquatic_workload_admissible(canal_node, observed_utc DESC);

ALTER TABLE cyboquatic_workload_frame
ADD COLUMN efficiency REAL CHECK (efficiency > 0.0 AND efficiency <= 1.0);

ALTER TABLE cyboquatic_workload_frame
ADD COLUMN renewable_fraction REAL CHECK (
    renewable_fraction >= 0.0 AND renewable_fraction <= 1.0
);
