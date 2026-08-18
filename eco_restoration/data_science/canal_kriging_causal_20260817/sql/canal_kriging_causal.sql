PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS canal_sensor_sites (
    sensor_site_id TEXT PRIMARY KEY NOT NULL,
    canal_node_id TEXT NOT NULL,
    x_m REAL NOT NULL,
    y_m REAL NOT NULL,
    metric_name TEXT NOT NULL CHECK (
        metric_name IN (
            'WATER_TEMPERATURE_C',
            'DISSOLVED_OXYGEN_MG_L',
            'BOD_MG_L',
            'TSS_MG_L',
            'CONDUCTIVITY_US_CM'
        )
    ),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS canal_sensor_observations (
    sensor_observation_id INTEGER PRIMARY KEY,
    sensor_site_id TEXT NOT NULL REFERENCES canal_sensor_sites(sensor_site_id),
    observed_at_utc TEXT NOT NULL,
    observed_value REAL NOT NULL,
    sensor_quality REAL NOT NULL CHECK (sensor_quality BETWEEN 0.0 AND 1.0),
    missing_flag INTEGER NOT NULL DEFAULT 0 CHECK (missing_flag IN (0, 1)),
    UNIQUE (sensor_site_id, observed_at_utc),
    CHECK (
        (missing_flag = 0 AND observed_value IS NOT NULL)
        OR
        (missing_flag = 1)
    )
);

CREATE TABLE IF NOT EXISTS variogram_models (
    variogram_model_id INTEGER PRIMARY KEY,
    metric_name TEXT NOT NULL,
    model_type TEXT NOT NULL CHECK (model_type IN ('EXPONENTIAL')),
    nugget REAL NOT NULL CHECK (nugget >= 0.0),
    sill REAL NOT NULL CHECK (sill >= 0.0),
    range_m REAL NOT NULL CHECK (range_m > 0.0),
    leave_one_out_rmse REAL CHECK (leave_one_out_rmse >= 0.0),
    selected INTEGER NOT NULL DEFAULT 0 CHECK (selected IN (0, 1)),
    fitted_at_utc TEXT NOT NULL,
    model_note TEXT NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_selected_variogram_per_metric
    ON variogram_models(metric_name)
    WHERE selected = 1;

CREATE TABLE IF NOT EXISTS kriging_estimates (
    kriging_estimate_id INTEGER PRIMARY KEY,
    metric_name TEXT NOT NULL,
    target_x_m REAL NOT NULL,
    target_y_m REAL NOT NULL,
    estimated_at_utc TEXT NOT NULL,
    variogram_model_id INTEGER NOT NULL REFERENCES variogram_models(variogram_model_id),
    estimate_value REAL NOT NULL,
    kriging_variance REAL NOT NULL CHECK (kriging_variance >= 0.0),
    weight_sum REAL NOT NULL,
    unbiasedness_constraint_satisfied INTEGER NOT NULL CHECK (
        unbiasedness_constraint_satisfied IN (0, 1)
    ),
    source_observation_count INTEGER NOT NULL CHECK (source_observation_count >= 2),
    CHECK (
        (unbiasedness_constraint_satisfied = 1 AND ABS(weight_sum - 1.0) <= 0.000001)
        OR unbiasedness_constraint_satisfied = 0
    )
);

CREATE TABLE IF NOT EXISTS canal_restoration_interventions (
    intervention_id TEXT PRIMARY KEY NOT NULL,
    canal_node_id TEXT NOT NULL,
    intervention_type TEXT NOT NULL CHECK (
        intervention_type IN (
            'NATIVE_PLANTING',
            'VEGETATED_SWALE',
            'SEDIMENT_REMOVAL',
            'BIOCHAR_MEDIA',
            'SHADE_INSTALLATION'
        )
    ),
    treated INTEGER NOT NULL CHECK (treated IN (0, 1)),
    intervention_start_utc TEXT,
    intervention_end_utc TEXT,
    geographic_stratum TEXT NOT NULL,
    baseline_water_temperature_c REAL,
    baseline_bod_mg_l REAL,
    baseline_tss_mg_l REAL,
    baseline_flow_m3_s REAL,
    baseline_shade_fraction REAL CHECK (baseline_shade_fraction BETWEEN 0.0 AND 1.0),
    baseline_sensor_quality REAL CHECK (baseline_sensor_quality BETWEEN 0.0 AND 1.0)
);

CREATE TABLE IF NOT EXISTS restoration_outcomes (
    restoration_outcome_id INTEGER PRIMARY KEY,
    intervention_id TEXT NOT NULL REFERENCES canal_restoration_interventions(intervention_id),
    measured_at_utc TEXT NOT NULL,
    outcome_window_days REAL NOT NULL CHECK (outcome_window_days > 0.0),
    downstream_heat_delta_c REAL,
    downstream_bod_delta_mg_l REAL,
    downstream_tss_delta_mg_l REAL,
    outcome_quality REAL NOT NULL CHECK (outcome_quality BETWEEN 0.0 AND 1.0),
    UNIQUE (intervention_id, measured_at_utc)
);

CREATE TABLE IF NOT EXISTS causal_matching_runs (
    causal_matching_run_id INTEGER PRIMARY KEY,
    run_label TEXT NOT NULL UNIQUE,
    outcome_metric TEXT NOT NULL CHECK (
        outcome_metric IN (
            'DOWNSTREAM_HEAT_DELTA_C',
            'DOWNSTREAM_BOD_DELTA_MG_L',
            'DOWNSTREAM_TSS_DELTA_MG_L'
        )
    ),
    caliper REAL NOT NULL CHECK (caliper >= 0.0),
    covariate_definition TEXT NOT NULL,
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS causal_matches (
    causal_match_id INTEGER PRIMARY KEY,
    causal_matching_run_id INTEGER NOT NULL REFERENCES causal_matching_runs(causal_matching_run_id),
    treated_intervention_id TEXT NOT NULL REFERENCES canal_restoration_interventions(intervention_id),
    control_intervention_id TEXT NOT NULL REFERENCES canal_restoration_interventions(intervention_id),
    standardized_distance REAL NOT NULL CHECK (standardized_distance >= 0.0),
    treated_outcome REAL NOT NULL,
    control_outcome REAL NOT NULL,
    treatment_effect REAL NOT NULL,
    UNIQUE (causal_matching_run_id, treated_intervention_id),
    UNIQUE (causal_matching_run_id, control_intervention_id),
    CHECK (treated_intervention_id <> control_intervention_id),
    CHECK (ABS(treatment_effect - (treated_outcome - control_outcome)) <= 0.000001)
);

CREATE INDEX IF NOT EXISTS idx_sensor_observations_site_time
    ON canal_sensor_observations(sensor_site_id, observed_at_utc DESC, missing_flag, sensor_quality);

CREATE INDEX IF NOT EXISTS idx_sensor_sites_metric_location
    ON canal_sensor_sites(metric_name, x_m, y_m, sensor_site_id);

CREATE INDEX IF NOT EXISTS idx_intervention_stratum_treated
    ON canal_restoration_interventions(geographic_stratum, treated, intervention_type, canal_node_id);

CREATE INDEX IF NOT EXISTS idx_outcomes_intervention_time
    ON restoration_outcomes(intervention_id, measured_at_utc DESC, outcome_quality);

CREATE INDEX IF NOT EXISTS idx_causal_matches_run_effect
    ON causal_matches(causal_matching_run_id, treatment_effect, standardized_distance);

CREATE VIEW IF NOT EXISTS v_selected_variograms AS
SELECT
    metric_name,
    model_type,
    nugget,
    sill,
    range_m,
    leave_one_out_rmse,
    fitted_at_utc,
    model_note
FROM variogram_models
WHERE selected = 1;

CREATE VIEW IF NOT EXISTS v_matched_treatment_effects AS
SELECT
    run.run_label,
    run.outcome_metric,
    COUNT(match_row.causal_match_id) AS matched_treated_count,
    AVG(match_row.treatment_effect) AS matched_att_estimate,
    MAX(match_row.standardized_distance) AS maximum_match_distance
FROM causal_matching_runs AS run
LEFT JOIN causal_matches AS match_row
    ON match_row.causal_matching_run_id = run.causal_matching_run_id
GROUP BY run.causal_matching_run_id;
