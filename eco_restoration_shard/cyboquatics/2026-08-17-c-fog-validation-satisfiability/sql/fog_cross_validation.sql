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

CREATE TABLE IF NOT EXISTS fog_threshold_sets (
    threshold_set_id INTEGER PRIMARY KEY,
    set_name TEXT NOT NULL UNIQUE,
    valid_from_utc TEXT NOT NULL,
    valid_to_utc TEXT,
    oil_threshold_mg_l REAL NOT NULL CHECK (oil_threshold_mg_l > 0.0),
    tss_threshold_mg_l REAL NOT NULL CHECK (tss_threshold_mg_l > 0.0),
    turbidity_threshold_ntu REAL NOT NULL CHECK (turbidity_threshold_ntu > 0.0),
    calibration_method TEXT NOT NULL,
    approved_by TEXT NOT NULL,
    CHECK (valid_to_utc IS NULL OR valid_to_utc > valid_from_utc)
);

CREATE TABLE IF NOT EXISTS fog_validation_samples (
    sample_id INTEGER PRIMARY KEY,
    canal_node_id TEXT NOT NULL REFERENCES canal_nodes(canal_node_id),
    threshold_set_id INTEGER NOT NULL REFERENCES fog_threshold_sets(threshold_set_id),
    fold_id INTEGER NOT NULL CHECK (fold_id >= 0 AND fold_id <= 999),
    sampled_at_utc TEXT NOT NULL,
    oil_mg_l REAL NOT NULL CHECK (oil_mg_l >= 0.0),
    tss_mg_l REAL NOT NULL CHECK (tss_mg_l >= 0.0),
    turbidity_ntu REAL NOT NULL CHECK (turbidity_ntu >= 0.0),
    reference_hazard INTEGER NOT NULL CHECK (reference_hazard IN (0, 1)),
    reference_method TEXT NOT NULL,
    ker_assessment_id INTEGER NOT NULL REFERENCES ker_assessments(ker_assessment_id),

    predicted_fog INTEGER GENERATED ALWAYS AS (
        CASE
            WHEN oil_mg_l >= (
                SELECT oil_threshold_mg_l
                FROM fog_threshold_sets
                WHERE threshold_set_id = fog_validation_samples.threshold_set_id
            )
            AND tss_mg_l >= (
                SELECT tss_threshold_mg_l
                FROM fog_threshold_sets
                WHERE threshold_set_id = fog_validation_samples.threshold_set_id
            )
            AND turbidity_ntu >= (
                SELECT turbidity_threshold_ntu
                FROM fog_threshold_sets
                WHERE threshold_set_id = fog_validation_samples.threshold_set_id
            )
            THEN 1
            ELSE 0
        END
    ) STORED
);

CREATE INDEX IF NOT EXISTS idx_fog_validation_fold_prediction_reference
    ON fog_validation_samples (
        fold_id,
        predicted_fog,
        reference_hazard,
        sample_id
    );

CREATE INDEX IF NOT EXISTS idx_fog_validation_threshold_fold
    ON fog_validation_samples (
        threshold_set_id,
        fold_id,
        predicted_fog,
        reference_hazard,
        sample_id
    );

CREATE INDEX IF NOT EXISTS idx_fog_threshold_validity
    ON fog_threshold_sets (
        threshold_set_id,
        valid_from_utc,
        valid_to_utc
    );

CREATE VIEW IF NOT EXISTS v_fog_cross_validation_metrics AS
SELECT
    fold_id,
    SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS true_positive,
    SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 0 THEN 1 ELSE 0 END) AS false_positive,
    SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS false_negative,
    SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 0 THEN 1 ELSE 0 END) AS true_negative,
    CASE
        WHEN SUM(CASE WHEN predicted_fog = 1 THEN 1 ELSE 0 END) = 0 THEN NULL
        ELSE CAST(SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS REAL)
             / SUM(CASE WHEN predicted_fog = 1 THEN 1 ELSE 0 END)
    END AS precision_value,
    CASE
        WHEN SUM(CASE WHEN reference_hazard = 1 THEN 1 ELSE 0 END) = 0 THEN NULL
        ELSE CAST(SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS REAL)
             / SUM(CASE WHEN reference_hazard = 1 THEN 1 ELSE 0 END)
    END AS recall_value,
    CASE
        WHEN (
            2 * SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 0 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 1 THEN 1 ELSE 0 END)
        ) = 0 THEN NULL
        ELSE CAST(
            2 * SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END)
            AS REAL
        ) / (
            2 * SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 0 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 1 THEN 1 ELSE 0 END)
        )
    END AS f1_value
FROM fog_validation_samples
GROUP BY fold_id;

CREATE VIEW IF NOT EXISTS v_fog_cross_validation_overall AS
SELECT
    SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS true_positive,
    SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 0 THEN 1 ELSE 0 END) AS false_positive,
    SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS false_negative,
    SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 0 THEN 1 ELSE 0 END) AS true_negative,
    CASE
        WHEN SUM(CASE WHEN predicted_fog = 1 THEN 1 ELSE 0 END) = 0 THEN NULL
        ELSE CAST(SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS REAL)
             / SUM(CASE WHEN predicted_fog = 1 THEN 1 ELSE 0 END)
    END AS precision_value,
    CASE
        WHEN SUM(CASE WHEN reference_hazard = 1 THEN 1 ELSE 0 END) = 0 THEN NULL
        ELSE CAST(SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END) AS REAL)
             / SUM(CASE WHEN reference_hazard = 1 THEN 1 ELSE 0 END)
    END AS recall_value,
    CASE
        WHEN (
            2 * SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 0 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 1 THEN 1 ELSE 0 END)
        ) = 0 THEN NULL
        ELSE CAST(
            2 * SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END)
            AS REAL
        ) / (
            2 * SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 1 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 1 AND reference_hazard = 0 THEN 1 ELSE 0 END)
            + SUM(CASE WHEN predicted_fog = 0 AND reference_hazard = 1 THEN 1 ELSE 0 END)
        )
    END AS f1_value
FROM fog_validation_samples;

CREATE VIEW IF NOT EXISTS v_fog_manual_review_samples AS
SELECT
    sample.sample_id,
    sample.canal_node_id,
    sample.sampled_at_utc,
    sample.oil_mg_l,
    sample.tss_mg_l,
    sample.turbidity_ntu,
    sample.predicted_fog,
    sample.reference_hazard,
    threshold_set.set_name,
    ker.knowledge_factor,
    ker.eco_impact_value,
    ker.harm_risk,
    ker.ker_score
FROM fog_validation_samples AS sample
JOIN fog_threshold_sets AS threshold_set
    ON threshold_set.threshold_set_id = sample.threshold_set_id
JOIN ker_assessments AS ker
    ON ker.ker_assessment_id = sample.ker_assessment_id
WHERE sample.predicted_fog <> sample.reference_hazard;
