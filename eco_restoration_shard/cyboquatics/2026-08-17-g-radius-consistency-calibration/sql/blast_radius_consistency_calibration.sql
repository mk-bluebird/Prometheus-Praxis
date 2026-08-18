PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS phoenix_hex_anchors (
    phoenix_hex_anchor_id INTEGER PRIMARY KEY,
    anchor_code TEXT NOT NULL UNIQUE,
    anchor_label TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS canal_nodes (
    canal_node_id TEXT PRIMARY KEY NOT NULL,
    phoenix_hex_anchor_id INTEGER NOT NULL REFERENCES phoenix_hex_anchors(phoenix_hex_anchor_id),
    node_name TEXT NOT NULL UNIQUE,
    bank_sensitivity REAL NOT NULL CHECK (bank_sensitivity BETWEEN 0.0 AND 1.0),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS surveyed_breach_observations (
    surveyed_breach_observation_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    surveyed_at_utc TEXT NOT NULL,
    discharge_m3_s REAL NOT NULL CHECK (discharge_m3_s > 0.0),
    duration_s REAL NOT NULL CHECK (duration_s > 0.0),
    bank_sensitivity REAL NOT NULL CHECK (bank_sensitivity BETWEEN 0.0 AND 1.0),
    observed_radius_m REAL NOT NULL CHECK (observed_radius_m >= 0.0),
    coefficient_c REAL NOT NULL CHECK (coefficient_c > 0.0),
    survey_method TEXT NOT NULL,
    survey_quality REAL NOT NULL CHECK (survey_quality BETWEEN 0.0 AND 1.0),
    UNIQUE (canal_node_id, surveyed_at_utc)
);

CREATE TABLE IF NOT EXISTS blast_radius_calibration_runs (
    blast_radius_calibration_run_id INTEGER PRIMARY KEY,
    calibration_label TEXT NOT NULL UNIQUE,
    calibrated_at_utc TEXT NOT NULL,
    bank_sensitivity_multiplier REAL NOT NULL CHECK (
        bank_sensitivity_multiplier >= 0.0
        AND bank_sensitivity_multiplier <= 1.5
    ),
    sum_squared_error REAL NOT NULL CHECK (sum_squared_error >= 0.0),
    mean_absolute_error_m REAL NOT NULL CHECK (mean_absolute_error_m >= 0.0),
    observation_count INTEGER NOT NULL CHECK (observation_count >= 2),
    calibration_status TEXT NOT NULL CHECK (
        calibration_status IN (
            'SCREENING_CALIBRATION_COMPLETE',
            'AT_CONSERVATIVE_CAP_REVIEW_REQUIRED'
        )
    ),
    created_by_did TEXT NOT NULL CHECK (
        created_by_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
    )
);

CREATE TABLE IF NOT EXISTS blast_radius_conformance_cases (
    conformance_case_id INTEGER PRIMARY KEY,
    case_label TEXT NOT NULL UNIQUE,
    discharge_m3_s REAL NOT NULL CHECK (discharge_m3_s > 0.0),
    duration_s REAL NOT NULL CHECK (duration_s > 0.0),
    bank_sensitivity REAL NOT NULL CHECK (bank_sensitivity BETWEEN 0.0 AND 1.0),
    distance_m REAL NOT NULL CHECK (distance_m >= 0.0),
    coefficient_c REAL NOT NULL CHECK (coefficient_c > 0.0),
    expected_zone TEXT NOT NULL CHECK (expected_zone IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    expected_scaled_radius INTEGER NOT NULL CHECK (expected_scaled_radius >= 0)
);

CREATE TABLE IF NOT EXISTS blast_radius_conformance_results (
    conformance_result_id INTEGER PRIMARY KEY,
    conformance_case_id INTEGER NOT NULL REFERENCES blast_radius_conformance_cases(conformance_case_id) ON DELETE CASCADE,
    implementation_language TEXT NOT NULL CHECK (
        implementation_language IN ('CPP', 'JAVA', 'KOTLIN', 'LUA')
    ),
    actual_zone TEXT NOT NULL CHECK (actual_zone IN ('SAFE', 'CAUTION', 'EXCLUDE')),
    actual_scaled_radius INTEGER NOT NULL CHECK (actual_scaled_radius >= 0),
    passed INTEGER NOT NULL CHECK (passed IN (0, 1)),
    executed_at_utc TEXT NOT NULL,
    UNIQUE (conformance_case_id, implementation_language),
    CHECK (
        (passed = 1 AND actual_zone = (
            SELECT expected_zone
            FROM blast_radius_conformance_cases
            WHERE conformance_case_id = blast_radius_conformance_results.conformance_case_id
        ) AND actual_scaled_radius = (
            SELECT expected_scaled_radius
            FROM blast_radius_conformance_cases
            WHERE conformance_case_id = blast_radius_conformance_results.conformance_case_id
        ))
        OR passed = 0
    )
);

CREATE INDEX IF NOT EXISTS idx_surveyed_breaches_node_time
    ON surveyed_breach_observations(canal_node_id, surveyed_at_utc DESC, survey_quality);

CREATE INDEX IF NOT EXISTS idx_calibration_multiplier_time
    ON blast_radius_calibration_runs(bank_sensitivity_multiplier, calibrated_at_utc DESC);

CREATE INDEX IF NOT EXISTS idx_conformance_result_case_language
    ON blast_radius_conformance_results(conformance_case_id, implementation_language, passed);

CREATE TRIGGER IF NOT EXISTS trg_conformance_case_boundary_consistency
BEFORE INSERT ON blast_radius_conformance_cases
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.expected_zone = 'SAFE'
         AND NEW.distance_m <= NEW.coefficient_c
            * SQRT(NEW.discharge_m3_s * NEW.duration_s)
            * (1.0 + 1.5 * NEW.bank_sensitivity)
        THEN RAISE(ABORT, 'SAFE conformance case requires distance greater than conservative radius')
        WHEN NEW.expected_zone = 'CAUTION'
         AND (
            NEW.distance_m <= (
                NEW.coefficient_c
                * SQRT(NEW.discharge_m3_s * NEW.duration_s)
                * (1.0 + 1.5 * NEW.bank_sensitivity)
            ) / 2.0
            OR NEW.distance_m > (
                NEW.coefficient_c
                * SQRT(NEW.discharge_m3_s * NEW.duration_s)
                * (1.0 + 1.5 * NEW.bank_sensitivity)
            )
         )
        THEN RAISE(ABORT, 'CAUTION conformance case must be inside outer and outside inner radius')
        WHEN NEW.expected_zone = 'EXCLUDE'
         AND NEW.distance_m > (
            NEW.coefficient_c
            * SQRT(NEW.discharge_m3_s * NEW.duration_s)
            * (1.0 + 1.5 * NEW.bank_sensitivity)
         ) / 2.0
        THEN RAISE(ABORT, 'EXCLUDE conformance case requires distance at or inside half-radius')
    END;
END;

CREATE VIEW IF NOT EXISTS v_conformance_failures AS
SELECT
    test.case_label,
    result.implementation_language,
    test.expected_zone,
    result.actual_zone,
    test.expected_scaled_radius,
    result.actual_scaled_radius,
    result.executed_at_utc
FROM blast_radius_conformance_results AS result
JOIN blast_radius_conformance_cases AS test
    ON test.conformance_case_id = result.conformance_case_id
WHERE result.passed = 0
   OR result.actual_zone <> test.expected_zone
   OR result.actual_scaled_radius <> test.expected_scaled_radius;

CREATE VIEW IF NOT EXISTS v_calibration_observations AS
SELECT
    anchor.anchor_code,
    node.canal_node_id,
    node.node_name,
    observation.surveyed_at_utc,
    observation.discharge_m3_s,
    observation.duration_s,
    observation.bank_sensitivity,
    observation.observed_radius_m,
    observation.coefficient_c,
    observation.survey_method,
    observation.survey_quality,
    observation.coefficient_c
        * SQRT(observation.discharge_m3_s * observation.duration_s) AS base_radius_m
FROM surveyed_breach_observations AS observation
JOIN canal_nodes AS node ON node.canal_node_id = observation.canal_node_id
JOIN phoenix_hex_anchors AS anchor
    ON anchor.phoenix_hex_anchor_id = node.phoenix_hex_anchor_id;
