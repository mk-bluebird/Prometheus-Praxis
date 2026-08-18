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

CREATE TABLE IF NOT EXISTS treatment_trains (
    treatment_train_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    train_name TEXT NOT NULL UNIQUE,
    contaminant_metric TEXT NOT NULL CHECK (
        contaminant_metric IN ('BOD_MG_L', 'TSS_MG_L', 'CEC_PROXY_MG_L')
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS treatment_layers (
    treatment_layer_id INTEGER PRIMARY KEY,
    treatment_train_id INTEGER NOT NULL REFERENCES treatment_trains(treatment_train_id) ON DELETE CASCADE,
    layer_position INTEGER NOT NULL CHECK (layer_position >= 1 AND layer_position <= 1000),
    layer_name TEXT NOT NULL,
    layer_type TEXT NOT NULL CHECK (
        layer_type IN ('SEDIMENTATION', 'VEGETATED_SWALE', 'BIOCHAR_MEDIA', 'SAND_MEDIA', 'POLISHING_FILTER')
    ),
    removal_fraction REAL NOT NULL CHECK (removal_fraction >= 0.0 AND removal_fraction <= 1.0),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
    UNIQUE (treatment_train_id, layer_position),
    UNIQUE (treatment_train_id, layer_name)
);

CREATE TABLE IF NOT EXISTS treatment_influent_events (
    treatment_influent_event_id INTEGER PRIMARY KEY,
    treatment_train_id INTEGER NOT NULL REFERENCES treatment_trains(treatment_train_id),
    observed_at_utc TEXT NOT NULL,
    influent_concentration_mg_l REAL NOT NULL CHECK (
        influent_concentration_mg_l >= 0.0 AND influent_concentration_mg_l <= 1000000.0
    ),
    flow_m3_s REAL NOT NULL CHECK (flow_m3_s >= 0.0 AND flow_m3_s <= 100000.0),
    UNIQUE (treatment_train_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS treatment_outlet_assessments (
    treatment_outlet_assessment_id INTEGER PRIMARY KEY,
    treatment_influent_event_id INTEGER NOT NULL UNIQUE REFERENCES treatment_influent_events(treatment_influent_event_id) ON DELETE CASCADE,
    calculated_outlet_concentration_mg_l REAL NOT NULL CHECK (
        calculated_outlet_concentration_mg_l >= 0.0
    ),
    layer_count INTEGER NOT NULL CHECK (layer_count >= 1),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS decay_calibration_runs (
    decay_calibration_run_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    run_name TEXT NOT NULL UNIQUE,
    metric_name TEXT NOT NULL CHECK (
        metric_name IN ('BOD_MG_L', 'TSS_MG_L', 'CEC_PROXY_MG_L')
    ),
    reference_concentration_mg_l REAL NOT NULL CHECK (
        reference_concentration_mg_l > 0.0 AND reference_concentration_mg_l <= 1000000.0
    ),
    forgetting_factor REAL NOT NULL CHECK (
        forgetting_factor > 0.0 AND forgetting_factor <= 1.0
    ),
    covariance_min REAL NOT NULL CHECK (covariance_min > 0.0),
    covariance_max REAL NOT NULL CHECK (covariance_max >= covariance_min),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS decay_calibration_observations (
    decay_calibration_observation_id INTEGER PRIMARY KEY,
    decay_calibration_run_id INTEGER NOT NULL REFERENCES decay_calibration_runs(decay_calibration_run_id) ON DELETE CASCADE,
    observed_at_utc TEXT NOT NULL,
    elapsed_days REAL NOT NULL CHECK (elapsed_days > 0.0 AND elapsed_days <= 3650.0),
    concentration_mg_l REAL NOT NULL CHECK (
        concentration_mg_l > 0.0 AND concentration_mg_l <= 1000000.0
    ),
    flow_m3_s REAL NOT NULL CHECK (flow_m3_s >= 0.0 AND flow_m3_s <= 100000.0),
    sensor_quality REAL NOT NULL CHECK (sensor_quality BETWEEN 0.0 AND 1.0),
    UNIQUE (decay_calibration_run_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS decay_rls_updates (
    decay_rls_update_id INTEGER PRIMARY KEY,
    decay_calibration_observation_id INTEGER NOT NULL UNIQUE REFERENCES decay_calibration_observations(decay_calibration_observation_id) ON DELETE CASCADE,
    prior_k_per_day REAL NOT NULL,
    posterior_k_per_day REAL NOT NULL,
    prior_covariance REAL NOT NULL CHECK (prior_covariance > 0.0),
    posterior_covariance REAL NOT NULL CHECK (posterior_covariance > 0.0),
    regression_phi REAL NOT NULL,
    regression_y REAL NOT NULL,
    residual REAL NOT NULL,
    gain REAL NOT NULL,
    flow_m3_s REAL NOT NULL CHECK (flow_m3_s >= 0.0),
    flow_drift_fraction REAL NOT NULL CHECK (flow_drift_fraction >= 0.0),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE INDEX IF NOT EXISTS idx_treatment_layers_train_order
    ON treatment_layers(treatment_train_id, enabled, layer_position, removal_fraction);

CREATE INDEX IF NOT EXISTS idx_influent_train_time
    ON treatment_influent_events(treatment_train_id, observed_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_decay_observation_run_time
    ON decay_calibration_observations(decay_calibration_run_id, observed_at_utc, elapsed_days);

CREATE INDEX IF NOT EXISTS idx_decay_rls_flow_drift
    ON decay_rls_updates(flow_drift_fraction DESC, posterior_k_per_day);

CREATE TRIGGER IF NOT EXISTS trg_outlet_assessment_matches_formula
BEFORE INSERT ON treatment_outlet_assessments
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN ABS(
            NEW.calculated_outlet_concentration_mg_l -
            (
                SELECT
                    CASE
                        WHEN EXISTS (
                            SELECT 1
                            FROM treatment_layers AS layer
                            JOIN treatment_influent_events AS influent
                                ON influent.treatment_train_id = layer.treatment_train_id
                            WHERE influent.treatment_influent_event_id = NEW.treatment_influent_event_id
                              AND layer.enabled = 1
                              AND layer.removal_fraction = 1.0
                        ) THEN 0.0
                        ELSE (
                            SELECT
                                influent.influent_concentration_mg_l *
                                EXP(SUM(LOG(1.0 - layer.removal_fraction)))
                            FROM treatment_influent_events AS influent
                            JOIN treatment_layers AS layer
                                ON layer.treatment_train_id = influent.treatment_train_id
                            WHERE influent.treatment_influent_event_id = NEW.treatment_influent_event_id
                              AND layer.enabled = 1
                        )
                    END
            )
        ) > 0.000001
        THEN RAISE(ABORT, 'outlet concentration does not match enabled layer removal fractions')
    END;
END;

CREATE VIEW IF NOT EXISTS v_deterministic_layered_outlet AS
SELECT
    influent.treatment_influent_event_id,
    train.train_name,
    train.contaminant_metric,
    influent.observed_at_utc,
    influent.influent_concentration_mg_l,
    influent.flow_m3_s,
    CASE
        WHEN EXISTS (
            SELECT 1
            FROM treatment_layers AS full_removal_layer
            WHERE full_removal_layer.treatment_train_id = train.treatment_train_id
              AND full_removal_layer.enabled = 1
              AND full_removal_layer.removal_fraction = 1.0
        ) THEN 0.0
        ELSE influent.influent_concentration_mg_l * EXP(
            COALESCE((
                SELECT SUM(LOG(1.0 - layer.removal_fraction))
                FROM treatment_layers AS layer
                WHERE layer.treatment_train_id = train.treatment_train_id
                  AND layer.enabled = 1
            ), 0.0)
        )
    END AS calculated_outlet_concentration_mg_l,
    (
        SELECT COUNT(*)
        FROM treatment_layers AS layer
        WHERE layer.treatment_train_id = train.treatment_train_id
          AND layer.enabled = 1
    ) AS enabled_layer_count
FROM treatment_influent_events AS influent
JOIN treatment_trains AS train
    ON train.treatment_train_id = influent.treatment_train_id;

CREATE VIEW IF NOT EXISTS v_decay_calibration_history AS
SELECT
    run.run_name,
    run.metric_name,
    observation.observed_at_utc,
    observation.elapsed_days,
    observation.concentration_mg_l,
    observation.flow_m3_s,
    observation.sensor_quality,
    update_row.prior_k_per_day,
    update_row.posterior_k_per_day,
    update_row.prior_covariance,
    update_row.posterior_covariance,
    update_row.residual,
    update_row.gain,
    update_row.flow_drift_fraction,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM decay_calibration_observations AS observation
JOIN decay_calibration_runs AS run
    ON run.decay_calibration_run_id = observation.decay_calibration_run_id
LEFT JOIN decay_rls_updates AS update_row
    ON update_row.decay_calibration_observation_id = observation.decay_calibration_observation_id
JOIN ker_assessments AS ker
    ON ker.ker_assessment_id = run.ker_assessment_id;
