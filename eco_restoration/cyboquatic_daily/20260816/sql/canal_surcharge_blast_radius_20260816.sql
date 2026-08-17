PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    latitude_deg REAL NOT NULL CHECK (latitude_deg BETWEEN -90.0 AND 90.0),
    longitude_deg REAL NOT NULL CHECK (longitude_deg BETWEEN -180.0 AND 180.0),
    waterway_class TEXT NOT NULL CHECK (waterway_class IN ('CANAL', 'DRAIN', 'RETENTION')),
    bank_sensitivity REAL NOT NULL CHECK (bank_sensitivity BETWEEN 0.0 AND 1.0),
    fog_media_state TEXT NOT NULL CHECK (fog_media_state IN ('MODELED_WATER', 'UNMODELED_MIXED_MEDIA', 'UNKNOWN_MEDIA')),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS ker_policy (
    policy_id INTEGER PRIMARY KEY,
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    decision_band TEXT NOT NULL CHECK (decision_band IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    CHECK (
        (decision_band = 'SAFE' AND knowledge_factor >= 0.75 AND eco_impact_value >= 0.60 AND harm_risk <= 0.25)
        OR
        (decision_band = 'CAUTION' AND harm_risk <= 0.60)
        OR
        (decision_band = 'EXCLUDE' AND harm_risk >= 0.60)
    )
);

CREATE TABLE IF NOT EXISTS surcharge_events (
    event_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    observed_at_utc TEXT NOT NULL,
    breach_flow_lps REAL NOT NULL CHECK (breach_flow_lps > 0.0 AND breach_flow_lps <= 100000.0),
    surcharge_duration_s REAL NOT NULL CHECK (surcharge_duration_s > 0.0 AND surcharge_duration_s <= 604800.0),
    energyreqJ REAL NOT NULL CHECK (energyreqJ >= 0.0 AND energyreqJ <= 1000000000.0),
    delta_vt REAL NOT NULL CHECK (delta_vt >= 0.0 AND delta_vt <= 10000.0),
    canal_stage_m REAL NOT NULL CHECK (canal_stage_m >= 0.0 AND canal_stage_m <= 100.0),
    fog_media_state TEXT NOT NULL CHECK (fog_media_state IN ('MODELED_WATER', 'UNMODELED_MIXED_MEDIA', 'UNKNOWN_MEDIA')),
    UNIQUE (canal_node_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS blast_radius_assessments (
    assessment_id INTEGER PRIMARY KEY,
    event_id INTEGER NOT NULL UNIQUE REFERENCES surcharge_events(event_id) ON DELETE CASCADE,
    assessed_at_utc TEXT NOT NULL,
    base_radius_m REAL NOT NULL CHECK (base_radius_m >= 0.0),
    conservative_radius_m REAL NOT NULL CHECK (conservative_radius_m >= base_radius_m),
    evaluated_distance_m REAL NOT NULL CHECK (evaluated_distance_m >= 0.0),
    zone TEXT NOT NULL CHECK (zone IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    machine_action TEXT NOT NULL CHECK (machine_action IN ('OPERATE_LOW_IMPACT', 'HOLD_FOR_INSPECTION', 'NO_ENTRY')),
    CHECK (
        (zone = 'SAFE' AND machine_action = 'OPERATE_LOW_IMPACT' AND harm_risk <= 0.25)
        OR
        (zone = 'CAUTION' AND machine_action = 'HOLD_FOR_INSPECTION' AND harm_risk > 0.25 AND harm_risk < 0.60)
        OR
        (zone = 'EXCLUDE' AND machine_action = 'NO_ENTRY' AND harm_risk >= 0.60)
    )
);

CREATE INDEX IF NOT EXISTS idx_surcharge_events_node_time
    ON surcharge_events(canal_node_id, observed_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_blast_assessments_zone_radius
    ON blast_radius_assessments(zone, conservative_radius_m);

CREATE INDEX IF NOT EXISTS idx_canal_nodes_fog_active
    ON canal_nodes(fog_media_state, active);

INSERT OR IGNORE INTO ker_policy (
    policy_id, knowledge_factor, eco_impact_value, harm_risk, decision_band
) VALUES
    (1, 0.85, 0.75, 0.20, 'SAFE'),
    (2, 0.55, 0.55, 0.40, 'CAUTION'),
    (3, 0.30, 0.20, 0.75, 'EXCLUDE');

CREATE VIEW IF NOT EXISTS v_active_blast_radius_status AS
SELECT
    n.canal_node_id,
    n.node_name,
    n.fog_media_state AS node_fog_media_state,
    e.observed_at_utc,
    e.breach_flow_lps,
    e.surcharge_duration_s,
    a.conservative_radius_m,
    a.zone,
    a.machine_action,
    a.knowledge_factor,
    a.eco_impact_value,
    a.harm_risk
FROM blast_radius_assessments AS a
JOIN surcharge_events AS e ON e.event_id = a.event_id
JOIN canal_nodes AS n ON n.canal_node_id = e.canal_node_id
WHERE n.active = 1;
