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

CREATE TABLE IF NOT EXISTS machinery_telemetry (
    machinery_telemetry_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    observed_at_utc TEXT NOT NULL,
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0 AND energyreq_j <= 1000000000.0),
    delta_vt REAL NOT NULL CHECK (delta_vt >= 0.0 AND delta_vt <= 10000.0),
    operation_state TEXT NOT NULL CHECK (
        operation_state IN ('PLANNED', 'MONITORED', 'HOLD_FOR_REVIEW', 'COMPLETED')
    ),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),
    UNIQUE (canal_node_id, observed_at_utc)
);

CREATE TABLE IF NOT EXISTS drainagedecay_runs (
    drainagedecay_run_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    run_label TEXT NOT NULL UNIQUE,
    initial_bod0_mg_l REAL NOT NULL CHECK (initial_bod0_mg_l > 0.0 AND initial_bod0_mg_l <= 100000.0),
    k20_min_per_day REAL NOT NULL CHECK (k20_min_per_day > 0.0 AND k20_min_per_day <= 100.0),
    k20_max_per_day REAL NOT NULL CHECK (k20_max_per_day >= k20_min_per_day AND k20_max_per_day <= 100.0),
    theta_min REAL NOT NULL CHECK (theta_min > 0.0 AND theta_min <= 10.0),
    theta_max REAL NOT NULL CHECK (theta_max >= theta_min AND theta_max <= 10.0),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE TABLE IF NOT EXISTS bod_temperature_flow_samples (
    bod_temperature_flow_sample_id INTEGER PRIMARY KEY,
    drainagedecay_run_id INTEGER NOT NULL REFERENCES drainagedecay_runs(drainagedecay_run_id) ON DELETE CASCADE,
    elapsed_days REAL NOT NULL CHECK (elapsed_days >= 0.0 AND elapsed_days <= 3650.0),
    water_temperature_c REAL NOT NULL CHECK (water_temperature_c BETWEEN -5.0 AND 60.0),
    bod_mg_l REAL NOT NULL CHECK (bod_mg_l >= 0.0 AND bod_mg_l <= 100000.0),
    flow_m3_s REAL NOT NULL CHECK (flow_m3_s >= 0.0 AND flow_m3_s <= 100000.0),
    replicate_id INTEGER NOT NULL CHECK (replicate_id BETWEEN 1 AND 100),
    UNIQUE (drainagedecay_run_id, elapsed_days, replicate_id)
);

CREATE TABLE IF NOT EXISTS bod_temperature_fit_results (
    bod_temperature_fit_result_id INTEGER PRIMARY KEY,
    drainagedecay_run_id INTEGER NOT NULL UNIQUE REFERENCES drainagedecay_runs(drainagedecay_run_id) ON DELETE CASCADE,
    fitted_k20_per_day REAL NOT NULL CHECK (fitted_k20_per_day > 0.0 AND fitted_k20_per_day <= 100.0),
    fitted_theta REAL NOT NULL CHECK (fitted_theta > 0.0 AND fitted_theta <= 10.0),
    sum_squared_error REAL NOT NULL CHECK (sum_squared_error >= 0.0),
    temperature_level_count INTEGER NOT NULL CHECK (temperature_level_count >= 1),
    identifiability_status TEXT NOT NULL CHECK (
        identifiability_status IN (
            'ONLY_EFFECTIVE_RATE_IDENTIFIABLE',
            'TEMPERATURE_VARIATION_PRESENT',
            'MULTI_TEMPERATURE_SCREENING_READY'
        )
    ),
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id)
);

CREATE INDEX IF NOT EXISTS idx_machinery_telemetry_node_time
    ON machinery_telemetry(canal_node_id, observed_at_utc DESC, energyreq_j, delta_vt);

CREATE INDEX IF NOT EXISTS idx_bod_samples_run_temp_time
    ON bod_temperature_flow_samples(
        drainagedecay_run_id,
        water_temperature_c,
        elapsed_days,
        replicate_id
    );

CREATE INDEX IF NOT EXISTS idx_bod_fit_identifiability
    ON bod_temperature_fit_results(identifiability_status, fitted_k20_per_day, fitted_theta);

CREATE TRIGGER IF NOT EXISTS trg_bod_sample_within_initial_demand
BEFORE INSERT ON bod_temperature_flow_samples
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.bod_mg_l > (
            SELECT initial_bod0_mg_l
            FROM drainagedecay_runs
            WHERE drainagedecay_run_id = NEW.drainagedecay_run_id
        )
        THEN RAISE(ABORT, 'BOD sample exceeds configured initial BOD0')
    END;
END;

CREATE VIEW IF NOT EXISTS v_safe_machinery_telemetry AS
SELECT
    telemetry.machinery_telemetry_id,
    node.canal_node_id,
    node.node_name,
    telemetry.observed_at_utc,
    telemetry.energyreq_j,
    telemetry.delta_vt,
    telemetry.operation_state,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM machinery_telemetry AS telemetry
JOIN canal_nodes AS node ON node.canal_node_id = telemetry.canal_node_id
JOIN ker_assessments AS ker ON ker.ker_assessment_id = telemetry.ker_assessment_id;

CREATE VIEW IF NOT EXISTS v_bod_temperature_decay_inputs AS
SELECT
    run.run_label,
    run.initial_bod0_mg_l,
    sample.elapsed_days,
    sample.water_temperature_c,
    sample.bod_mg_l,
    sample.flow_m3_s,
    sample.replicate_id,
    fit.fitted_k20_per_day,
    fit.fitted_theta,
    fit.identifiability_status
FROM drainagedecay_runs AS run
JOIN bod_temperature_flow_samples AS sample
    ON sample.drainagedecay_run_id = run.drainagedecay_run_id
LEFT JOIN bod_temperature_fit_results AS fit
    ON fit.drainagedecay_run_id = run.drainagedecay_run_id;
