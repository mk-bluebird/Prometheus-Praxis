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

CREATE TABLE IF NOT EXISTS pfas_cold_parameter_sets (
    parameter_set_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    parameter_label TEXT NOT NULL UNIQUE,
    temperature_c REAL NOT NULL CHECK (temperature_c BETWEEN -5.0 AND 50.0),
    concentration_c_mg_l REAL NOT NULL CHECK (concentration_c_mg_l >= 0.0 AND concentration_c_mg_l <= 100000.0),
    sediment_state_s_kg_m3 REAL NOT NULL CHECK (sediment_state_s_kg_m3 >= 0.0 AND sediment_state_s_kg_m3 <= 100000.0),
    removal_rate_per_day REAL NOT NULL CHECK (removal_rate_per_day >= 0.0 AND removal_rate_per_day <= 1000.0),
    partition_rate_m3_kg_day REAL NOT NULL CHECK (partition_rate_m3_kg_day >= 0.0 AND partition_rate_m3_kg_day <= 1000.0),
    release_rate_per_day REAL NOT NULL CHECK (release_rate_per_day >= 0.0 AND release_rate_per_day <= 1000.0),
    settling_rate_per_day REAL NOT NULL CHECK (settling_rate_per_day >= 0.0 AND settling_rate_per_day <= 1000.0),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS pfas_bifurcation_assessments (
    bifurcation_assessment_id INTEGER PRIMARY KEY,
    parameter_set_id INTEGER NOT NULL UNIQUE REFERENCES pfas_cold_parameter_sets(parameter_set_id) ON DELETE CASCADE,
    assessed_at_utc TEXT NOT NULL,
    j11 REAL NOT NULL,
    j12 REAL NOT NULL,
    j21 REAL NOT NULL,
    j22 REAL NOT NULL,
    trace_j REAL NOT NULL,
    determinant_j REAL NOT NULL,
    lambda_max_real REAL NOT NULL,
    contraction_status TEXT NOT NULL CHECK (
        contraction_status IN ('CONTRACTING', 'BOUNDARY_REVIEW', 'LOST_CONTRACTION')
    ),
    corridor_limit REAL NOT NULL CHECK (corridor_limit > 0.0),
    CHECK (
        (contraction_status = 'CONTRACTING' AND lambda_max_real < 0.0)
        OR
        (contraction_status = 'BOUNDARY_REVIEW' AND lambda_max_real BETWEEN -0.00000001 AND 0.00000001)
        OR
        (contraction_status = 'LOST_CONTRACTION' AND lambda_max_real > 0.0)
    )
);

CREATE TABLE IF NOT EXISTS fog_threshold_profiles (
    fog_threshold_profile_id INTEGER PRIMARY KEY,
    profile_name TEXT NOT NULL UNIQUE,
    valid_from_utc TEXT NOT NULL,
    valid_to_utc TEXT,
    oil_threshold_mg_l REAL NOT NULL CHECK (oil_threshold_mg_l > 0.0 AND oil_threshold_mg_l <= 100000.0),
    tss_threshold_mg_l REAL NOT NULL CHECK (tss_threshold_mg_l > 0.0 AND tss_threshold_mg_l <= 100000.0),
    turbidity_threshold_ntu REAL NOT NULL CHECK (turbidity_threshold_ntu > 0.0 AND turbidity_threshold_ntu <= 100000.0),
    calibration_method TEXT NOT NULL,
    approved_by TEXT NOT NULL,
    CHECK (valid_to_utc IS NULL OR valid_to_utc > valid_from_utc)
);

CREATE TABLE IF NOT EXISTS fog_media_samples (
    fog_media_sample_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    fog_threshold_profile_id INTEGER NOT NULL REFERENCES fog_threshold_profiles(fog_threshold_profile_id),
    sampled_at_utc TEXT NOT NULL,
    oil_mg_l REAL NOT NULL CHECK (oil_mg_l >= 0.0 AND oil_mg_l <= 100000.0),
    tss_mg_l REAL NOT NULL CHECK (tss_mg_l >= 0.0 AND tss_mg_l <= 100000.0),
    turbidity_ntu REAL NOT NULL CHECK (turbidity_ntu >= 0.0 AND turbidity_ntu <= 100000.0),
    p_fog INTEGER NOT NULL CHECK (p_fog IN (0, 1)),
    media_state TEXT NOT NULL CHECK (
        media_state IN (
            'FIELD_SCREENED_NOT_ALL_FOG_CONDITIONS_MET',
            'UNMODELED_MIXED_OIL_WATER_SEDIMENT'
        )
    ),
    routing_action TEXT NOT NULL CHECK (
        routing_action IN ('RETAIN_FOR_SAMPLING_AND_CONTEXT_REVIEW', 'MANUAL_FIELD_REVIEW')
    ),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    UNIQUE (canal_node_id, sampled_at_utc)
);

CREATE INDEX IF NOT EXISTS idx_pfas_bifurcation_status
    ON pfas_bifurcation_assessments(contraction_status, lambda_max_real);

CREATE INDEX IF NOT EXISTS idx_fog_samples_node_time
    ON fog_media_samples(canal_node_id, sampled_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_fog_media_state
    ON fog_media_samples(media_state, p_fog);

CREATE TRIGGER IF NOT EXISTS trg_fog_state_consistency
BEFORE INSERT ON fog_media_samples
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.p_fog = 1
         AND (NEW.media_state <> 'UNMODELED_MIXED_OIL_WATER_SEDIMENT'
              OR NEW.routing_action <> 'MANUAL_FIELD_REVIEW')
        THEN RAISE(ABORT, 'P_fog=1 requires unmodeled mixed-media classification and manual review')
        WHEN NEW.p_fog = 0
         AND (NEW.media_state <> 'FIELD_SCREENED_NOT_ALL_FOG_CONDITIONS_MET'
              OR NEW.routing_action <> 'RETAIN_FOR_SAMPLING_AND_CONTEXT_REVIEW')
        THEN RAISE(ABORT, 'P_fog=0 requires field-screened classification and context review')
    END;
END;

CREATE VIEW IF NOT EXISTS v_pfas_cold_stability AS
SELECT
    node.canal_node_id,
    parameter_set.parameter_label,
    parameter_set.temperature_c,
    assessment.lambda_max_real,
    assessment.contraction_status,
    assessment.corridor_limit,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM pfas_bifurcation_assessments AS assessment
JOIN pfas_cold_parameter_sets AS parameter_set
    ON parameter_set.parameter_set_id = assessment.parameter_set_id
JOIN canal_nodes AS node
    ON node.canal_node_id = parameter_set.canal_node_id
JOIN ker_assessments AS ker
    ON ker.ker_assessment_id = parameter_set.ker_assessment_id;

CREATE VIEW IF NOT EXISTS v_fog_manual_review AS
SELECT
    node.canal_node_id,
    sample.sampled_at_utc,
    sample.oil_mg_l,
    sample.tss_mg_l,
    sample.turbidity_ntu,
    profile.profile_name,
    sample.media_state,
    sample.routing_action,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM fog_media_samples AS sample
JOIN canal_nodes AS node ON node.canal_node_id = sample.canal_node_id
JOIN fog_threshold_profiles AS profile ON profile.fog_threshold_profile_id = sample.fog_threshold_profile_id
JOIN ker_assessments AS ker ON ker.ker_assessment_id = sample.ker_assessment_id
WHERE sample.p_fog = 1;
