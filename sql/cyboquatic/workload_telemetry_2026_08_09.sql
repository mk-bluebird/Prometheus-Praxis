-- File: sql/cyboquatic/workload_telemetry_2026_08_09.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_workload_frame (
    frame_id INTEGER PRIMARY KEY,
    observed_utc TEXT NOT NULL DEFAULT (datetime('now')),
    node_id TEXT NOT NULL,
    owner_did TEXT NOT NULL CHECK (owner_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0),
    delta_vt REAL NOT NULL CHECK (delta_vt BETWEEN 0.0 AND 1.0),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_k REAL NOT NULL CHECK (ker_k BETWEEN 0.0 AND 1.0),
    ker_e REAL NOT NULL CHECK (ker_e BETWEEN 0.0 AND 1.0),
    ker_r REAL NOT NULL CHECK (ker_r BETWEEN 0.0 AND 1.0),
    fog_confidence REAL NOT NULL CHECK (fog_confidence BETWEEN 0.0 AND 1.0),
    canal_node TEXT NOT NULL,
    accepted INTEGER NOT NULL CHECK (accepted IN (0, 1)),
    -- New telemetry columns with constrained ranges matching language-level validation
    flow_m3_s REAL NOT NULL CHECK (flow_m3_s >= 0.0),
    lift_m REAL NOT NULL CHECK (lift_m >= 0.0),
    efficiency REAL NOT NULL CHECK (efficiency > 0.0 AND efficiency <= 1.0),
    runtime_s REAL NOT NULL CHECK (runtime_s >= 0.0),
    voltage_drop_v REAL NOT NULL CHECK (voltage_drop_v >= 0.0),
    renewable_fraction REAL NOT NULL CHECK (renewable_fraction BETWEEN 0.0 AND 1.0),
    embodied_carbon_g_per_j REAL NOT NULL CHECK (embodied_carbon_g_per_j >= 0.0),
    biodiversity_risk REAL NOT NULL CHECK (biodiversity_risk BETWEEN 0.0 AND 1.0),
    CHECK (accepted = 0 OR (delta_vt <= 0.35 AND eco_impact_value >= 0.60 AND ker_k * ker_e > ker_r))
);

CREATE INDEX IF NOT EXISTS idx_cyboquatic_workload_node_time
ON cyboquatic_workload_frame(node_id, observed_utc DESC);

CREATE TRIGGER IF NOT EXISTS reject_unverified_canal_frame
BEFORE INSERT ON cyboquatic_workload_frame
WHEN NEW.fog_confidence < 0.75
BEGIN
    SELECT RAISE(ABORT, 'FOG confidence below operational corridor');
END;

-- View for accepted frames: must satisfy KER condition and FOG threshold
CREATE VIEW IF NOT EXISTS v_cyboquatic_workload_admissible AS
SELECT
    frame_id,
    observed_utc,
    node_id,
    owner_did,
    energyreq_j,
    delta_vt,
    knowledge_factor,
    eco_impact_value,
    ker_k,
    ker_e,
    ker_r,
    fog_confidence,
    canal_node,
    flow_m3_s,
    lift_m,
    efficiency,
    runtime_s,
    voltage_drop_v,
    renewable_fraction,
    embodied_carbon_g_per_j,
    biodiversity_risk
FROM cyboquatic_workload_frame
WHERE accepted = 1
  AND ker_k * ker_e > ker_r
  AND fog_confidence >= 0.75;

-- View for rejected frames with deterministic reason text
CREATE VIEW IF NOT EXISTS v_cyboquatic_workload_review AS
SELECT
    frame_id,
    observed_utc,
    node_id,
    owner_did,
    energyreq_j,
    delta_vt,
    knowledge_factor,
    eco_impact_value,
    ker_k,
    ker_e,
    ker_r,
    fog_confidence,
    canal_node,
    flow_m3_s,
    lift_m,
    efficiency,
    runtime_s,
    voltage_drop_v,
    renewable_fraction,
    embodied_carbon_g_per_j,
    biodiversity_risk,
    CASE
        WHEN accepted = 1 THEN 'accepted'
        ELSE
            trim(
                CASE WHEN NOT (ker_k * ker_e > ker_r) THEN 'KER_FAILED; ' ELSE '' END ||
                CASE WHEN fog_confidence < 0.75 THEN 'FOG_FAILED; ' ELSE '' END ||
                CASE WHEN delta_vt > 0.35 THEN 'DELTA_EXCEEDED; ' ELSE '' END ||
                CASE WHEN eco_impact_value < 0.60 THEN 'ECO_IMPACT_LOW; ' ELSE '' END ||
                CASE WHEN knowledge_factor < 0.75 THEN 'KNOWLEDGE_LOW; ' ELSE '' END
            )
    END AS rejection_reason
FROM cyboquatic_workload_frame
WHERE accepted = 0
   OR NOT (ker_k * ker_e > ker_r)
   OR fog_confidence < 0.75;
