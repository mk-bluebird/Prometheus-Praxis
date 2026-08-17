PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    fog_media_state TEXT NOT NULL CHECK (
        fog_media_state IN ('AQUEOUS_MODELED', 'MIXED_MEDIA_UNMODELED', 'UNKNOWN_MEDIA')
    ),
    operational_energy_limit_j REAL NOT NULL CHECK (
        operational_energy_limit_j > 0.0 AND operational_energy_limit_j <= 1000000000.0
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

CREATE TABLE IF NOT EXISTS biodegradation_runs (
    biodegradation_run_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    run_label TEXT NOT NULL UNIQUE,
    temperature_c REAL NOT NULL CHECK (temperature_c BETWEEN 0.0 AND 60.0),
    bod_u_mg_l REAL NOT NULL CHECK (bod_u_mg_l > 0.0 AND bod_u_mg_l <= 100000.0),
    candidate_k_per_day REAL NOT NULL CHECK (candidate_k_per_day > 0.0 AND candidate_k_per_day <= 100.0),
    oxygen_blank_corrected INTEGER NOT NULL CHECK (oxygen_blank_corrected IN (0, 1)),
    nitrification_reviewed INTEGER NOT NULL CHECK (nitrification_reviewed IN (0, 1)),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS bod_measurements (
    bod_measurement_id INTEGER PRIMARY KEY,
    biodegradation_run_id INTEGER NOT NULL REFERENCES biodegradation_runs(biodegradation_run_id) ON DELETE CASCADE,
    measured_at_hours REAL NOT NULL CHECK (measured_at_hours >= 0.0 AND measured_at_hours <= 8760.0),
    bod_mg_l REAL NOT NULL CHECK (bod_mg_l >= 0.0 AND bod_mg_l <= 100000.0),
    replicate_id INTEGER NOT NULL CHECK (replicate_id >= 1 AND replicate_id <= 100),
    temperature_c REAL NOT NULL CHECK (temperature_c BETWEEN 0.0 AND 60.0),
    UNIQUE (biodegradation_run_id, replicate_id, measured_at_hours)
);

CREATE TABLE IF NOT EXISTS algal_growth_runs (
    algal_growth_run_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    run_label TEXT NOT NULL UNIQUE,
    mu_max_per_hour REAL NOT NULL CHECK (mu_max_per_hour >= 0.0 AND mu_max_per_hour <= 100.0),
    substrate_mg_l REAL NOT NULL CHECK (substrate_mg_l >= 0.0 AND substrate_mg_l <= 100000.0),
    ks_mg_l REAL NOT NULL CHECK (ks_mg_l >= 0.0 AND ks_mg_l <= 100000.0),
    ec50_mg_l REAL NOT NULL CHECK (ec50_mg_l > 0.0 AND ec50_mg_l <= 100000.0),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS algal_exposure_points (
    algal_exposure_point_id INTEGER PRIMARY KEY,
    algal_growth_run_id INTEGER NOT NULL REFERENCES algal_growth_runs(algal_growth_run_id) ON DELETE CASCADE,
    elapsed_hours REAL NOT NULL CHECK (elapsed_hours >= 0.0 AND elapsed_hours <= 8760.0),
    concentration_mg_l REAL NOT NULL CHECK (concentration_mg_l >= 0.0 AND concentration_mg_l <= 100000.0),
    fog_media_state TEXT NOT NULL CHECK (
        fog_media_state IN ('AQUEOUS_MODELED', 'MIXED_MEDIA_UNMODELED', 'UNKNOWN_MEDIA')
    ),
    UNIQUE (algal_growth_run_id, elapsed_hours)
);

CREATE INDEX IF NOT EXISTS idx_bod_measurements_run_time
    ON bod_measurements (biodegradation_run_id, measured_at_hours, replicate_id);

CREATE INDEX IF NOT EXISTS idx_exposure_points_run_time
    ON algal_exposure_points (algal_growth_run_id, elapsed_hours);

CREATE INDEX IF NOT EXISTS idx_canal_nodes_fog_active
    ON canal_nodes (fog_media_state, active);

CREATE TRIGGER IF NOT EXISTS trg_bod_below_bodu
BEFORE INSERT ON bod_measurements
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.bod_mg_l > (
            SELECT bod_u_mg_l
            FROM biodegradation_runs
            WHERE biodegradation_run_id = NEW.biodegradation_run_id
        )
        THEN RAISE(ABORT, 'BOD measurement cannot exceed configured BODu')
    END;
END;

CREATE VIEW IF NOT EXISTS v_bod_identifiability_input AS
SELECT
    run.run_label,
    run.temperature_c AS run_temperature_c,
    run.bod_u_mg_l,
    run.candidate_k_per_day,
    measurement.replicate_id,
    measurement.measured_at_hours / 24.0 AS time_days,
    measurement.bod_mg_l,
    measurement.temperature_c AS measurement_temperature_c,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM biodegradation_runs AS run
JOIN bod_measurements AS measurement
    ON measurement.biodegradation_run_id = run.biodegradation_run_id
JOIN ker_assessments AS ker
    ON ker.ker_assessment_id = run.ker_assessment_id;

CREATE VIEW IF NOT EXISTS v_algal_time_varying_exposure AS
SELECT
    run.run_label,
    exposure.elapsed_hours,
    exposure.concentration_mg_l,
    run.mu_max_per_hour,
    run.substrate_mg_l,
    run.ks_mg_l,
    run.ec50_mg_l,
    exposure.fog_media_state,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM algal_growth_runs AS run
JOIN algal_exposure_points AS exposure
    ON exposure.algal_growth_run_id = run.algal_growth_run_id
JOIN ker_assessments AS ker
    ON ker.ker_assessment_id = run.ker_assessment_id;
