PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    node_name TEXT NOT NULL UNIQUE,
    fog_media_state TEXT NOT NULL CHECK (
        fog_media_state IN ('AQUEOUS_MODELED', 'MIXED_MEDIA_UNMODELED', 'UNKNOWN_MEDIA')
    ),
    energy_limit_j_per_hour REAL NOT NULL CHECK (
        energy_limit_j_per_hour > 0.0 AND energy_limit_j_per_hour <= 1000000000.0
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

CREATE TABLE IF NOT EXISTS sensor_metadata (
    sensor_id TEXT PRIMARY KEY NOT NULL,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    measurement_name TEXT NOT NULL CHECK (
        measurement_name IN ('DISSOLVED_OXYGEN_MG_L', 'TURBIDITY_NTU', 'CONDUCTIVITY_US_CM', 'TEMPERATURE_C')
    ),
    observation_gain_h REAL NOT NULL CHECK (observation_gain_h <> 0.0 AND observation_gain_h BETWEEN -1000.0 AND 1000.0),
    measurement_variance_r REAL NOT NULL CHECK (measurement_variance_r > 0.0 AND measurement_variance_r <= 1000000.0),
    calibration_valid_from_utc TEXT NOT NULL,
    calibration_valid_to_utc TEXT,
    fog_media_state TEXT NOT NULL CHECK (
        fog_media_state IN ('AQUEOUS_MODELED', 'MIXED_MEDIA_UNMODELED', 'UNKNOWN_MEDIA')
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    CHECK (calibration_valid_to_utc IS NULL OR calibration_valid_to_utc > calibration_valid_from_utc)
);

CREATE TABLE IF NOT EXISTS sensor_observations (
    observation_id INTEGER PRIMARY KEY,
    sensor_id TEXT NOT NULL REFERENCES sensor_metadata(sensor_id),
    observed_at_utc TEXT NOT NULL,
    measurement_value REAL,
    available INTEGER NOT NULL CHECK (available IN (0, 1)),
    energyreqJ REAL NOT NULL CHECK (energyreqJ >= 0.0 AND energyreqJ <= 1000000000.0),
    dropout_reason TEXT,
    CHECK (
        (available = 1 AND measurement_value IS NOT NULL AND dropout_reason IS NULL)
        OR
        (available = 0 AND measurement_value IS NULL AND dropout_reason IS NOT NULL)
    ),
    UNIQUE (sensor_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS residual_corridor_records (
    residual_record_id INTEGER PRIMARY KEY,
    observation_id INTEGER NOT NULL UNIQUE REFERENCES sensor_observations(observation_id) ON DELETE CASCADE,
    prior_estimate REAL NOT NULL,
    prior_covariance_p REAL NOT NULL CHECK (prior_covariance_p >= 0.0 AND prior_covariance_p <= 1000000000.0),
    innovation_residual REAL,
    innovation_variance_s REAL,
    kalman_gain REAL NOT NULL,
    normalized_residual REAL,
    update_applied INTEGER NOT NULL CHECK (update_applied IN (0, 1)),
    whiteness_lag1 REAL CHECK (whiteness_lag1 BETWEEN -1.0 AND 1.0),
    corridor_limit REAL NOT NULL CHECK (corridor_limit > 0.0),
    corridor_breach INTEGER NOT NULL CHECK (corridor_breach IN (0, 1)),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    CHECK (
        (update_applied = 1 AND innovation_residual IS NOT NULL AND innovation_variance_s > 0.0 AND normalized_residual IS NOT NULL)
        OR
        (update_applied = 0 AND innovation_residual IS NULL AND innovation_variance_s IS NULL AND normalized_residual IS NULL AND kalman_gain = 0.0)
    )
);

CREATE TABLE IF NOT EXISTS rls_corridor_calibrations (
    rls_calibration_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    calibrated_at_utc TEXT NOT NULL,
    phi REAL NOT NULL CHECK (phi BETWEEN -100000.0 AND 100000.0),
    observation_y REAL NOT NULL CHECK (observation_y BETWEEN -1000000000.0 AND 1000000000.0),
    theta_prior REAL NOT NULL CHECK (theta_prior BETWEEN -1000000000.0 AND 1000000000.0),
    theta_posterior REAL NOT NULL CHECK (theta_posterior BETWEEN -1000000000.0 AND 1000000000.0),
    covariance_prior_p REAL NOT NULL CHECK (covariance_prior_p > 0.0 AND covariance_prior_p <= 1000000000.0),
    covariance_posterior_p REAL NOT NULL CHECK (covariance_posterior_p > 0.0 AND covariance_posterior_p <= 1000000000.0),
    forgetting_factor REAL NOT NULL CHECK (forgetting_factor > 0.0 AND forgetting_factor <= 1.0),
    residual REAL NOT NULL,
    gain REAL NOT NULL,
    gain_bound REAL NOT NULL CHECK (gain_bound >= 0.0),
    corridor_radius REAL NOT NULL CHECK (corridor_radius > 0.0),
    accepted INTEGER NOT NULL CHECK (accepted IN (0, 1)),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    CHECK (ABS(gain) <= gain_bound + 0.000001)
);

CREATE INDEX IF NOT EXISTS idx_sensor_observations_sensor_time
    ON sensor_observations(sensor_id, observed_at_utc);

CREATE INDEX IF NOT EXISTS idx_residual_corridor_breach
    ON residual_corridor_records(corridor_breach, update_applied, whiteness_lag1);

CREATE INDEX IF NOT EXISTS idx_rls_calibration_node_time
    ON rls_corridor_calibrations(canal_node_id, calibrated_at_utc);

CREATE TRIGGER IF NOT EXISTS trg_sensor_energy_limit
BEFORE INSERT ON sensor_observations
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.energyreqJ > (
            SELECT energy_limit_j_per_hour
            FROM canal_nodes AS node
            JOIN sensor_metadata AS sensor ON sensor.canal_node_id = node.canal_node_id
            WHERE sensor.sensor_id = NEW.sensor_id
        )
        THEN RAISE(ABORT, 'sensor observation exceeds canal-node energy limit')
    END;
END;

CREATE VIEW IF NOT EXISTS v_available_innovations AS
SELECT
    node.canal_node_id,
    sensor.sensor_id,
    sensor.measurement_name,
    observation.observed_at_utc,
    record.innovation_residual,
    record.innovation_variance_s,
    record.normalized_residual,
    record.whiteness_lag1,
    record.corridor_breach,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM residual_corridor_records AS record
JOIN sensor_observations AS observation ON observation.observation_id = record.observation_id
JOIN sensor_metadata AS sensor ON sensor.sensor_id = observation.sensor_id
JOIN canal_nodes AS node ON node.canal_node_id = sensor.canal_node_id
JOIN ker_assessments AS ker ON ker.ker_assessment_id = record.ker_assessment_id
WHERE record.update_applied = 1
  AND observation.available = 1;
