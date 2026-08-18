PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    soil_condition_label TEXT NOT NULL,
    allowable_bearing_pressure_pa REAL NOT NULL CHECK (
        allowable_bearing_pressure_pa > 0.0 AND allowable_bearing_pressure_pa <= 100000000.0
    ),
    bank_sensitivity REAL NOT NULL CHECK (bank_sensitivity BETWEEN 0.0 AND 1.0),
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

CREATE TABLE IF NOT EXISTS machinery_profiles (
    machinery_profile_id INTEGER PRIMARY KEY,
    machinery_name TEXT NOT NULL UNIQUE,
    activity_type TEXT NOT NULL CHECK (activity_type IN ('LOW_IMPACT_DREDGING', 'NATIVE_PLANTING')),
    drivetrain_efficiency REAL NOT NULL CHECK (drivetrain_efficiency > 0.0 AND drivetrain_efficiency <= 1.0),
    contact_area_m2 REAL NOT NULL CHECK (contact_area_m2 > 0.0 AND contact_area_m2 <= 10000.0),
    normal_load_n REAL NOT NULL CHECK (normal_load_n > 0.0 AND normal_load_n <= 100000000.0),
    max_plan_energyreq_j REAL NOT NULL CHECK (max_plan_energyreq_j > 0.0 AND max_plan_energyreq_j <= 100000000000.0),
    site_delta_vt_limit REAL NOT NULL CHECK (site_delta_vt_limit > 0.0 AND site_delta_vt_limit <= 10000.0)
);

CREATE TABLE IF NOT EXISTS workload_telemetry (
    workload_telemetry_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    machinery_profile_id INTEGER NOT NULL REFERENCES machinery_profiles(machinery_profile_id),
    observed_at_utc TEXT NOT NULL,
    force_n REAL NOT NULL CHECK (force_n >= 0.0 AND force_n <= 100000000.0),
    distance_m REAL NOT NULL CHECK (distance_m >= 0.0 AND distance_m <= 1000000.0),
    mechanical_work_j REAL NOT NULL CHECK (mechanical_work_j >= 0.0),
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0),
    ground_pressure_pa REAL NOT NULL CHECK (ground_pressure_pa >= 0.0),
    adjusted_allowable_pressure_pa REAL NOT NULL CHECK (adjusted_allowable_pressure_pa >= 0.0),
    pressure_safe INTEGER NOT NULL CHECK (pressure_safe IN (0, 1)),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    UNIQUE (canal_node_id, machinery_profile_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS delta_vt_samples (
    delta_vt_sample_id INTEGER PRIMARY KEY,
    workload_telemetry_id INTEGER NOT NULL REFERENCES workload_telemetry(workload_telemetry_id) ON DELETE CASCADE,
    elapsed_seconds REAL NOT NULL CHECK (elapsed_seconds >= 0.0),
    acceleration_m_per_s2 REAL NOT NULL CHECK (
        acceleration_m_per_s2 BETWEEN -1000.0 AND 1000.0
    ),
    UNIQUE (workload_telemetry_id, elapsed_seconds)
);

CREATE TABLE IF NOT EXISTS delta_vt_assessments (
    delta_vt_assessment_id INTEGER PRIMARY KEY,
    workload_telemetry_id INTEGER NOT NULL UNIQUE REFERENCES workload_telemetry(workload_telemetry_id) ON DELETE CASCADE,
    integrated_delta_vt_m_per_s REAL NOT NULL,
    site_delta_vt_limit REAL NOT NULL CHECK (site_delta_vt_limit > 0.0 AND site_delta_vt_limit <= 10000.0),
    within_site_limit INTEGER NOT NULL CHECK (within_site_limit IN (0, 1)),
    CHECK (
        (within_site_limit = 1 AND ABS(integrated_delta_vt_m_per_s) <= site_delta_vt_limit)
        OR
        (within_site_limit = 0 AND ABS(integrated_delta_vt_m_per_s) > site_delta_vt_limit)
    )
);

CREATE INDEX IF NOT EXISTS idx_workload_telemetry_node_time
    ON workload_telemetry(canal_node_id, observed_at_utc DESC, pressure_safe);

CREATE INDEX IF NOT EXISTS idx_delta_vt_assessment_limit
    ON delta_vt_assessments(within_site_limit, site_delta_vt_limit);

CREATE TRIGGER IF NOT EXISTS trg_energy_and_pressure_limits
BEFORE INSERT ON workload_telemetry
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.energyreq_j > (
            SELECT max_plan_energyreq_j
            FROM machinery_profiles
            WHERE machinery_profile_id = NEW.machinery_profile_id
        )
        THEN RAISE(ABORT, 'energy requirement exceeds machinery plan limit')
        WHEN NEW.ground_pressure_pa > NEW.adjusted_allowable_pressure_pa
        THEN RAISE(ABORT, 'ground pressure exceeds sensitivity-adjusted allowable pressure')
    END;
END;

CREATE VIEW IF NOT EXISTS v_low_impact_workload_status AS
SELECT
    node.canal_node_id,
    node.node_name,
    profile.machinery_name,
    profile.activity_type,
    telemetry.observed_at_utc,
    telemetry.energyreq_j,
    telemetry.ground_pressure_pa,
    telemetry.adjusted_allowable_pressure_pa,
    telemetry.pressure_safe,
    assessment.integrated_delta_vt_m_per_s,
    assessment.site_delta_vt_limit,
    assessment.within_site_limit,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM workload_telemetry AS telemetry
JOIN canal_nodes AS node ON node.canal_node_id = telemetry.canal_node_id
JOIN machinery_profiles AS profile ON profile.machinery_profile_id = telemetry.machinery_profile_id
LEFT JOIN delta_vt_assessments AS assessment ON assessment.workload_telemetry_id = telemetry.workload_telemetry_id
JOIN ker_assessments AS ker ON ker.ker_assessment_id = telemetry.ker_assessment_id;
