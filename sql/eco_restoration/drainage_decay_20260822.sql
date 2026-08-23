-- Repository: mk-bluebird/Prometheus-Praxis
-- Filename: sql/eco_restoration/drainage_decay_20260822.sql
-- Destination: sql/eco_restoration/

PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY,
    node_name TEXT NOT NULL CHECK (length(trim(node_name)) BETWEEN 1 AND 120),
    catchment_area_m2 REAL NOT NULL CHECK (catchment_area_m2 > 0.0 AND catchment_area_m2 <= 1000000000.0),
    design_flow_l_s REAL NOT NULL CHECK (design_flow_l_s > 0.0 AND design_flow_l_s <= 10000000.0),
    maximum_bod_mg_l REAL NOT NULL CHECK (maximum_bod_mg_l > 0.0 AND maximum_bod_mg_l <= 100000.0),
    maximum_tss_mg_l REAL NOT NULL CHECK (maximum_tss_mg_l > 0.0 AND maximum_tss_mg_l <= 100000.0),
    minimum_cec_cmol_kg REAL NOT NULL CHECK (minimum_cec_cmol_kg >= 0.0 AND minimum_cec_cmol_kg <= 200.0),
    maximum_energyreq_j REAL NOT NULL CHECK (maximum_energyreq_j > 0.0 AND maximum_energyreq_j <= 1000000000000.0),
    maximum_abs_delta_vt REAL NOT NULL CHECK (maximum_abs_delta_vt > 0.0 AND maximum_abs_delta_vt <= 1000.0),
    created_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE IF NOT EXISTS fog_media_registry (
    fog_media_code TEXT PRIMARY KEY,
    medium_name TEXT NOT NULL CHECK (length(trim(medium_name)) BETWEEN 1 AND 120),
    modeled INTEGER NOT NULL CHECK (modeled IN (0, 1)),
    containment_required INTEGER NOT NULL CHECK (containment_required IN (0, 1)),
    safe_discharge_allowed INTEGER NOT NULL CHECK (safe_discharge_allowed IN (0, 1)),
    CHECK (NOT (modeled = 0 AND safe_discharge_allowed = 1))
);

CREATE TABLE IF NOT EXISTS drainage_decay_frames (
    frame_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    fog_media_code TEXT NOT NULL REFERENCES fog_media_registry(fog_media_code),
    observed_at_utc TEXT NOT NULL,
    duration_hours REAL NOT NULL CHECK (duration_hours >= 0.0 AND duration_hours <= 8760.0),
    bod_mg_l REAL NOT NULL CHECK (bod_mg_l >= 0.0 AND bod_mg_l <= 100000.0),
    tss_mg_l REAL NOT NULL CHECK (tss_mg_l >= 0.0 AND tss_mg_l <= 100000.0),
    cec_cmol_kg REAL NOT NULL CHECK (cec_cmol_kg >= 0.0 AND cec_cmol_kg <= 200.0),
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0 AND energyreq_j <= 1000000000000.0),
    delta_vt REAL NOT NULL CHECK (delta_vt >= -1000.0 AND delta_vt <= 1000.0),
    sample_completeness REAL NOT NULL CHECK (sample_completeness >= 0.0 AND sample_completeness <= 1.0),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor >= 0.0 AND knowledge_factor <= 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value >= 0.0 AND eco_impact_value <= 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk >= 0.0 AND harm_risk <= 1.0),
    decision_support_status TEXT NOT NULL CHECK (
        decision_support_status IN ('hold', 'inspect', 'restore', 'restricted')
    ),
    created_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_drainage_decay_frames_node_time
ON drainage_decay_frames(canal_node_id, observed_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_drainage_decay_frames_status_risk
ON drainage_decay_frames(decision_support_status, harm_risk DESC);

CREATE TRIGGER IF NOT EXISTS validate_drainage_decay_frame_insert
BEFORE INSERT ON drainage_decay_frames
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.bod_mg_l > (
            SELECT maximum_bod_mg_l FROM canal_nodes WHERE canal_node_id = NEW.canal_node_id
        ) THEN RAISE(ABORT, 'BOD exceeds canal-node constraint')
    END;

    SELECT CASE
        WHEN NEW.tss_mg_l > (
            SELECT maximum_tss_mg_l FROM canal_nodes WHERE canal_node_id = NEW.canal_node_id
        ) THEN RAISE(ABORT, 'TSS exceeds canal-node constraint')
    END;

    SELECT CASE
        WHEN NEW.cec_cmol_kg < (
            SELECT minimum_cec_cmol_kg FROM canal_nodes WHERE canal_node_id = NEW.canal_node_id
        ) THEN RAISE(ABORT, 'CEC is below canal-node constraint')
    END;

    SELECT CASE
        WHEN NEW.energyreq_j > (
            SELECT maximum_energyreq_j FROM canal_nodes WHERE canal_node_id = NEW.canal_node_id
        ) THEN RAISE(ABORT, 'Energy demand exceeds canal-node constraint')
    END;

    SELECT CASE
        WHEN abs(NEW.delta_vt) > (
            SELECT maximum_abs_delta_vt FROM canal_nodes WHERE canal_node_id = NEW.canal_node_id
        ) THEN RAISE(ABORT, 'Voltage change exceeds canal-node constraint')
    END;

    SELECT CASE
        WHEN NEW.decision_support_status = 'restore'
             AND (
                 NEW.harm_risk > 0.25
                 OR NEW.knowledge_factor < 0.70
                 OR EXISTS (
                     SELECT 1
                     FROM fog_media_registry
                     WHERE fog_media_code = NEW.fog_media_code
                       AND (modeled = 0 OR containment_required = 1)
                 )
             )
        THEN RAISE(ABORT, 'Restore status requires low risk, adequate evidence, and modeled non-contained media')
    END;

    SELECT CASE
        WHEN NEW.decision_support_status = 'restricted'
             AND NOT EXISTS (
                 SELECT 1
                 FROM fog_media_registry
                 WHERE fog_media_code = NEW.fog_media_code
                   AND (modeled = 0 OR containment_required = 1)
             )
        THEN RAISE(ABORT, 'Restricted status requires unmodeled or contained media')
    END;
END;

CREATE VIEW IF NOT EXISTS drainage_decay_decision_support AS
SELECT
    f.frame_id,
    f.canal_node_id,
    f.fog_media_code,
    f.observed_at_utc,
    f.bod_mg_l,
    f.tss_mg_l,
    f.cec_cmol_kg,
    f.energyreq_j,
    f.delta_vt,
    f.knowledge_factor,
    f.eco_impact_value,
    f.harm_risk,
    f.decision_support_status,
    CASE
        WHEN f.decision_support_status = 'restricted' THEN 'manual_containment_review'
        WHEN f.harm_risk > 0.50 THEN 'stop_and_inspect'
        WHEN f.knowledge_factor < 0.70 THEN 'collect_additional_measurements'
        WHEN f.eco_impact_value >= 0.70 AND f.harm_risk <= 0.25 THEN 'candidate_for_supervised_restoration'
        ELSE 'monitor'
    END AS recommended_action
FROM drainage_decay_frames AS f;
