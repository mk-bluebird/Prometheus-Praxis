-- Filename: sql/blast_radius_schema.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS blast_radius (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id           TEXT    NOT NULL,
    breach_scenario   TEXT    NOT NULL,
    delta_head_m      REAL    NOT NULL,
    flow_m3_s         REAL    NOT NULL,
    surcharge_risk    REAL    NOT NULL,
    impact_B          REAL    NOT NULL,
    created_at        TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS blast_radius_affected_shards (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    blast_radius_id   INTEGER NOT NULL,
    affected_node_id  TEXT    NOT NULL,
    affected_shard_id TEXT    NOT NULL,
    FOREIGN KEY (blast_radius_id) REFERENCES blast_radius(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_blast_node_scenario
    ON blast_radius (node_id, breach_scenario);

CREATE INDEX IF NOT EXISTS idx_blast_impact
    ON blast_radius (impact_B);

CREATE INDEX IF NOT EXISTS idx_blast_created_at
    ON blast_radius (created_at);

CREATE INDEX IF NOT EXISTS idx_affected_shards_node
    ON blast_radius_affected_shards (affected_node_id);

CREATE INDEX IF NOT EXISTS idx_affected_shards_shard
    ON blast_radius_affected_shards (affected_shard_id);

CREATE INDEX IF NOT EXISTS idx_affected_shards_blast
    ON blast_radius_affected_shards (blast_radius_id);

-- Query: find high-impact breach scenarios for a node.
-- Parameters: :node_id, :impact_threshold
SELECT id,
       node_id,
       breach_scenario,
       delta_head_m,
       flow_m3_s,
       surcharge_risk,
       impact_B,
       created_at
FROM blast_radius
WHERE node_id = :node_id
  AND impact_B > :impact_threshold
ORDER BY impact_B DESC;

-- Query: retrieve affected shards for a given breach scenario.
-- Parameter: :blast_radius_id
SELECT b.id                AS blast_radius_id,
       b.node_id          AS source_node_id,
       b.breach_scenario  AS scenario,
       b.impact_B         AS impact_B,
       s.affected_node_id,
       s.affected_shard_id
FROM blast_radius AS b
JOIN blast_radius_affected_shards AS s
  ON b.id = s.blast_radius_id
WHERE b.id = :blast_radius_id
ORDER BY s.affected_node_id, s.affected_shard_id;
