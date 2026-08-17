PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    fog_media_state TEXT NOT NULL CHECK (
        fog_media_state IN (
            'AQUEOUS_MODELED',
            'FIELD_SCREENED_NOT_ALL_FOG_CONDITIONS_MET',
            'UNMODELED_MIXED_OIL_WATER_SEDIMENT',
            'UNKNOWN_MEDIA'
        )
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS ker_assessments (
    ker_assessment_id INTEGER PRIMARY KEY,
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    harm_risk REAL NOT NULL CHECK (harm_risk BETWEEN 0.0 AND 1.0),
    ker_score REAL NOT NULL CHECK (ker_score BETWEEN -1.0 AND 1.0),
    CHECK (ABS(ker_score - knowledge_factor * (eco_impact_value - harm_risk)) <= 0.000001)
);

CREATE TABLE IF NOT EXISTS fog_threshold_profiles (
    fog_threshold_profile_id INTEGER PRIMARY KEY,
    profile_name TEXT NOT NULL UNIQUE,
    valid_from_utc TEXT NOT NULL,
    valid_to_utc TEXT,
    oil_threshold_mg_l REAL NOT NULL CHECK (oil_threshold_mg_l > 0.0),
    tss_threshold_mg_l REAL NOT NULL CHECK (tss_threshold_mg_l > 0.0),
    turbidity_threshold_ntu REAL NOT NULL CHECK (turbidity_threshold_ntu > 0.0),
    delta_score REAL NOT NULL CHECK (delta_score >= 0.0),
    safety_margin_multiplier REAL NOT NULL CHECK (safety_margin_multiplier >= 0.0),
    minimum_violations_for_forced_hold INTEGER NOT NULL CHECK (
        minimum_violations_for_forced_hold >= 1 AND minimum_violations_for_forced_hold <= 100
    ),
    calibration_method TEXT NOT NULL,
    approved_by TEXT NOT NULL,
    CHECK (valid_to_utc IS NULL OR valid_to_utc > valid_from_utc)
);

CREATE TABLE IF NOT EXISTS fog_sensor_calibrations (
    fog_sensor_calibration_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    sensor_kind TEXT NOT NULL CHECK (sensor_kind IN ('OIL', 'TSS', 'TURBIDITY')),
    calibration_id TEXT NOT NULL UNIQUE,
    valid_from_utc TEXT NOT NULL,
    valid_to_utc TEXT,
    standard_uncertainty REAL NOT NULL CHECK (standard_uncertainty >= 0.0),
    units TEXT NOT NULL,
    CHECK (valid_to_utc IS NULL OR valid_to_utc > valid_from_utc)
);

CREATE TABLE IF NOT EXISTS fog_router_state (
    canal_node_id TEXT PRIMARY KEY REFERENCES canal_nodes(canal_node_id),
    p_fog INTEGER NOT NULL CHECK (p_fog IN (0, 1)),
    prior_score REAL NOT NULL,
    consecutive_safety_violations INTEGER NOT NULL DEFAULT 0 CHECK (
        consecutive_safety_violations >= 0 AND consecutive_safety_violations <= 100000
    ),
    updated_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS fog_router_observations (
    fog_router_observation_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    fog_threshold_profile_id INTEGER NOT NULL REFERENCES fog_threshold_profiles(fog_threshold_profile_id),
    observed_at_utc TEXT NOT NULL,
    oil_mg_l REAL NOT NULL CHECK (oil_mg_l >= 0.0),
    tss_mg_l REAL NOT NULL CHECK (tss_mg_l >= 0.0),
    turbidity_ntu REAL NOT NULL CHECK (turbidity_ntu >= 0.0),
    oil_sigma_mg_l REAL NOT NULL CHECK (oil_sigma_mg_l >= 0.0),
    tss_sigma_mg_l REAL NOT NULL CHECK (tss_sigma_mg_l >= 0.0),
    turbidity_sigma_ntu REAL NOT NULL CHECK (turbidity_sigma_ntu >= 0.0),
    score REAL NOT NULL,
    score_sigma REAL NOT NULL CHECK (score_sigma >= 0.0),
    p_fog INTEGER NOT NULL CHECK (p_fog IN (0, 1)),
    confidence_state TEXT NOT NULL CHECK (
        confidence_state IN (
            'FOG_CONFIRMED_WITHIN_MARGIN_INTERVAL',
            'FOG_NOT_CONFIRMED_WITHIN_MARGIN_INTERVAL',
            'INDETERMINATE_MANUAL_FIELD_REVIEW'
        )
    ),
    switch_applied INTEGER NOT NULL CHECK (switch_applied IN (0, 1)),
    routing_action TEXT NOT NULL CHECK (
        routing_action IN (
            'MANUAL_FIELD_REVIEW',
            'RETAIN_FOR_SAMPLING_AND_CONTEXT_REVIEW',
            'HOLD_PREVIOUS_ROUTE'
        )
    ),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    UNIQUE (canal_node_id, observed_at_utc)
);

CREATE INDEX IF NOT EXISTS idx_fog_observations_node_time
    ON fog_router_observations(canal_node_id, observed_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_fog_uncertainty_review
    ON fog_router_observations(confidence_state, p_fog, switch_applied);

CREATE INDEX IF NOT EXISTS idx_fog_calibration_node_kind
    ON fog_sensor_calibrations(canal_node_id, sensor_kind, valid_from_utc DESC);

CREATE TRIGGER IF NOT EXISTS trg_fog_indeterminate_review
BEFORE INSERT ON fog_router_observations
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.confidence_state = 'INDETERMINATE_MANUAL_FIELD_REVIEW'
         AND NEW.routing_action <> 'MANUAL_FIELD_REVIEW'
        THEN RAISE(ABORT, 'indeterminate FOG confidence requires manual field review')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_fog_switch_requires_score_change
BEFORE INSERT ON fog_router_observations
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.switch_applied = 1
         AND ABS(
             NEW.score - COALESCE(
                 (SELECT prior_score FROM fog_router_state WHERE canal_node_id = NEW.canal_node_id),
                 NEW.score
             )
         ) <= (
             SELECT delta_score
             FROM fog_threshold_profiles
             WHERE fog_threshold_profile_id = NEW.fog_threshold_profile_id
         )
        THEN RAISE(ABORT, 'FOG state switch requires score change strictly greater than configured delta')
    END;
END;

CREATE VIEW IF NOT EXISTS v_fog_uncertainty_review AS
SELECT
    node.canal_node_id,
    observation.observed_at_utc,
    observation.score,
    observation.score_sigma,
    observation.p_fog,
    observation.confidence_state,
    observation.switch_applied,
    observation.routing_action,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM fog_router_observations AS observation
JOIN canal_nodes AS node ON node.canal_node_id = observation.canal_node_id
JOIN ker_assessments AS ker ON ker.ker_assessment_id = observation.ker_assessment_id
WHERE observation.confidence_state = 'INDETERMINATE_MANUAL_FIELD_REVIEW'
   OR observation.switch_applied = 1;
