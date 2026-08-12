-- File: sql/ppx_ai_workload/governance_and_surcharge_blast_radius.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ppx_governance_particle (
    owner_did TEXT PRIMARY KEY
        CHECK(owner_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
    policy_id TEXT NOT NULL UNIQUE,
    k_min REAL NOT NULL CHECK(k_min BETWEEN 0.0 AND 1.0),
    e_min REAL NOT NULL CHECK(e_min BETWEEN 0.0 AND 1.0),
    r_max REAL NOT NULL CHECK(r_max BETWEEN 0.0 AND 1.0),
    roh_max REAL NOT NULL CHECK(roh_max BETWEEN 0.0 AND 1.0),
    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
) STRICT;

CREATE TABLE IF NOT EXISTS ppx_telemetry_signature (
    signature_id TEXT PRIMARY KEY CHECK(length(signature_id) BETWEEN 8 AND 256),
    owner_did TEXT NOT NULL
        REFERENCES ppx_governance_particle(owner_did),
    verified_utc TEXT NOT NULL,
    verification_state TEXT NOT NULL
        CHECK(verification_state = 'VERIFIED'),
    verifier_id TEXT NOT NULL CHECK(length(verifier_id) BETWEEN 1 AND 128)
) STRICT;

CREATE TABLE IF NOT EXISTS ppx_hex_anchor (
    hex_anchor INTEGER PRIMARY KEY CHECK(hex_anchor >= 0),
    latitude_deg REAL NOT NULL CHECK(latitude_deg BETWEEN -90.0 AND 90.0),
    longitude_deg REAL NOT NULL CHECK(longitude_deg BETWEEN -180.0 AND 180.0),
    x_m REAL NOT NULL,
    y_m REAL NOT NULL,
    hierarchy_level INTEGER NOT NULL CHECK(hierarchy_level BETWEEN 0 AND 63),
    owner_did TEXT NOT NULL
        REFERENCES ppx_governance_particle(owner_did)
) STRICT;

CREATE VIRTUAL TABLE IF NOT EXISTS ppx_hex_anchor_rtree
USING rtree(hex_anchor, min_x, max_x, min_y, max_y);

CREATE TRIGGER IF NOT EXISTS ppx_hex_anchor_rtree_insert
AFTER INSERT ON ppx_hex_anchor
FOR EACH ROW
BEGIN
    INSERT OR REPLACE INTO ppx_hex_anchor_rtree
    VALUES(NEW.hex_anchor, NEW.x_m, NEW.x_m, NEW.y_m, NEW.y_m);
END;

CREATE TRIGGER IF NOT EXISTS ppx_hex_anchor_rtree_update
AFTER UPDATE OF x_m, y_m ON ppx_hex_anchor
FOR EACH ROW
BEGIN
    INSERT OR REPLACE INTO ppx_hex_anchor_rtree
    VALUES(NEW.hex_anchor, NEW.x_m, NEW.x_m, NEW.y_m, NEW.y_m);
END;

CREATE TRIGGER IF NOT EXISTS ppx_hex_anchor_rtree_delete
AFTER DELETE ON ppx_hex_anchor
FOR EACH ROW
BEGIN
    DELETE FROM ppx_hex_anchor_rtree WHERE hex_anchor = OLD.hex_anchor;
END;

CREATE TABLE IF NOT EXISTS ppx_governed_telemetry (
    frame_id TEXT PRIMARY KEY CHECK(length(frame_id) BETWEEN 1 AND 128),
    owner_did TEXT NOT NULL
        REFERENCES ppx_governance_particle(owner_did),
    policy_id TEXT NOT NULL,
    signature_id TEXT NOT NULL
        REFERENCES ppx_telemetry_signature(signature_id),
    hex_anchor INTEGER NOT NULL
        REFERENCES ppx_hex_anchor(hex_anchor),
    lane TEXT NOT NULL
        CHECK(lane IN ('RESEARCH', 'PILOT', 'PRODUCTION')),
    action TEXT NOT NULL
        CHECK(action IN ('PROCEED', 'DERATE', 'HALT')),
    k_knowledge REAL NOT NULL CHECK(k_knowledge BETWEEN 0.0 AND 1.0),
    e_eco_impact REAL NOT NULL CHECK(e_eco_impact BETWEEN 0.0 AND 1.0),
    r_risk REAL NOT NULL CHECK(r_risk BETWEEN 0.0 AND 1.0),
    roh REAL NOT NULL CHECK(roh BETWEEN 0.0 AND 1.0),
    delta_vt REAL NOT NULL,
    observed_utc TEXT NOT NULL,
    CHECK(r_risk >= delta_vt)
) STRICT;

CREATE INDEX IF NOT EXISTS idx_ppx_governed_telemetry_hex_time
ON ppx_governed_telemetry(hex_anchor, observed_utc DESC);

CREATE INDEX IF NOT EXISTS idx_ppx_governed_telemetry_policy_time
ON ppx_governed_telemetry(policy_id, observed_utc DESC);

CREATE TRIGGER IF NOT EXISTS ppx_governed_telemetry_guard_insert
BEFORE INSERT ON ppx_governed_telemetry
FOR EACH ROW
WHEN NOT EXISTS (
    SELECT 1
    FROM ppx_governance_particle AS p
    JOIN ppx_telemetry_signature AS s
      ON s.signature_id = NEW.signature_id
     AND s.owner_did = NEW.owner_did
     AND s.verification_state = 'VERIFIED'
    WHERE p.owner_did = NEW.owner_did
      AND p.policy_id = NEW.policy_id
      AND NEW.k_knowledge >= p.k_min
      AND NEW.e_eco_impact >= p.e_min
      AND NEW.r_risk <= p.r_max
      AND NEW.roh <= p.roh_max
)
BEGIN
    SELECT RAISE(ABORT, 'telemetry does not satisfy owner, signature, or KER corridor policy');
END;

CREATE TRIGGER IF NOT EXISTS ppx_governed_telemetry_guard_update
BEFORE UPDATE OF owner_did, policy_id, signature_id, k_knowledge, e_eco_impact,
                 r_risk, roh, delta_vt ON ppx_governed_telemetry
FOR EACH ROW
WHEN NOT EXISTS (
    SELECT 1
    FROM ppx_governance_particle AS p
    JOIN ppx_telemetry_signature AS s
      ON s.signature_id = NEW.signature_id
     AND s.owner_did = NEW.owner_did
     AND s.verification_state = 'VERIFIED'
    WHERE p.owner_did = NEW.owner_did
      AND p.policy_id = NEW.policy_id
      AND NEW.k_knowledge >= p.k_min
      AND NEW.e_eco_impact >= p.e_min
      AND NEW.r_risk <= p.r_max
      AND NEW.roh <= p.roh_max
)
BEGIN
    SELECT RAISE(ABORT, 'updated telemetry does not satisfy owner, signature, or KER corridor policy');
END;

CREATE TABLE IF NOT EXISTS ppx_canal_node (
    node_id INTEGER PRIMARY KEY,
    hex_anchor INTEGER NOT NULL
        REFERENCES ppx_hex_anchor(hex_anchor),
    elevation_m REAL NOT NULL,
    hydraulic_head_m REAL NOT NULL CHECK(hydraulic_head_m >= 0.0),
    head_to_radius_m REAL NOT NULL CHECK(head_to_radius_m >= 0.0)
) STRICT;

CREATE TABLE IF NOT EXISTS ppx_canal_edge (
    from_node_id INTEGER NOT NULL
        REFERENCES ppx_canal_node(node_id),
    to_node_id INTEGER NOT NULL
        REFERENCES ppx_canal_node(node_id),
    friction_head_m REAL NOT NULL CHECK(friction_head_m >= 0.0),
    PRIMARY KEY(from_node_id, to_node_id),
    CHECK(from_node_id <> to_node_id)
) STRICT;

CREATE INDEX IF NOT EXISTS idx_ppx_canal_edge_source
ON ppx_canal_edge(from_node_id);

CREATE TABLE IF NOT EXISTS blast_radius (
    blast_radius_id INTEGER PRIMARY KEY,
    source_node_id INTEGER NOT NULL
        REFERENCES ppx_canal_node(node_id),
    breach_scenario TEXT NOT NULL CHECK(length(breach_scenario) BETWEEN 1 AND 128),
    delta_head_m REAL NOT NULL CHECK(delta_head_m >= 0.0),
    flow_m3_s REAL NOT NULL CHECK(flow_m3_s >= 0.0),
    surcharge_risk REAL NOT NULL CHECK(surcharge_risk BETWEEN 0.0 AND 1.0),
    impact_b REAL NOT NULL,
    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
) STRICT;

CREATE TABLE IF NOT EXISTS blast_radius_affected_shards (
    blast_radius_id INTEGER NOT NULL
        REFERENCES blast_radius(blast_radius_id) ON DELETE CASCADE,
    affected_node_id INTEGER NOT NULL
        REFERENCES ppx_canal_node(node_id),
    affected_hex_anchor INTEGER NOT NULL
        REFERENCES ppx_hex_anchor(hex_anchor),
    remaining_head_m REAL NOT NULL CHECK(remaining_head_m >= 0.0),
    PRIMARY KEY(blast_radius_id, affected_node_id, affected_hex_anchor)
) STRICT;

CREATE INDEX IF NOT EXISTS idx_blast_radius_node_impact
ON blast_radius(source_node_id, impact_b DESC);

CREATE INDEX IF NOT EXISTS idx_blast_affected_anchor
ON blast_radius_affected_shards(affected_hex_anchor, blast_radius_id);

WITH RECURSIVE
root AS (
    SELECT
        n.node_id,
        n.elevation_m,
        n.hydraulic_head_m,
        n.head_to_radius_m,
        h.x_m,
        h.y_m
    FROM ppx_canal_node AS n
    JOIN ppx_hex_anchor AS h ON h.hex_anchor = n.hex_anchor
    WHERE n.node_id = :breach_node_id
),
reach(node_id, remaining_head_m, path) AS (
    SELECT node_id, hydraulic_head_m, '/' || node_id || '/' FROM root
    UNION ALL
    SELECT
        edge.to_node_id,
        reach.remaining_head_m - edge.friction_head_m -
        MAX(0.0, next_node.elevation_m - current_node.elevation_m),
        reach.path || edge.to_node_id || '/'
    FROM reach
    JOIN ppx_canal_edge AS edge ON edge.from_node_id = reach.node_id
    JOIN ppx_canal_node AS current_node ON current_node.node_id = edge.from_node_id
    JOIN ppx_canal_node AS next_node ON next_node.node_id = edge.to_node_id
    WHERE reach.remaining_head_m > edge.friction_head_m
      AND instr(reach.path, '/' || edge.to_node_id || '/') = 0
)
SELECT DISTINCT
    node.hex_anchor,
    node.node_id,
    reach.remaining_head_m
FROM reach
JOIN ppx_canal_node AS node ON node.node_id = reach.node_id
JOIN ppx_hex_anchor AS anchor ON anchor.hex_anchor = node.hex_anchor
JOIN root
JOIN ppx_hex_anchor_rtree AS spatial ON spatial.hex_anchor = anchor.hex_anchor
WHERE spatial.min_x <= root.x_m + root.hydraulic_head_m * root.head_to_radius_m
  AND spatial.max_x >= root.x_m - root.hydraulic_head_m * root.head_to_radius_m
  AND spatial.min_y <= root.y_m + root.hydraulic_head_m * root.head_to_radius_m
  AND spatial.max_y >= root.y_m - root.hydraulic_head_m * root.head_to_radius_m
  AND (anchor.x_m - root.x_m) * (anchor.x_m - root.x_m) +
      (anchor.y_m - root.y_m) * (anchor.y_m - root.y_m) <=
      (root.hydraulic_head_m * root.head_to_radius_m) *
      (root.hydraulic_head_m * root.head_to_radius_m);
