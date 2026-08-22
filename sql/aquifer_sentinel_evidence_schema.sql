PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

CREATE TABLE evidence_item (
    evidence_id TEXT PRIMARY KEY NOT NULL,
    event_time_utc TEXT NOT NULL,
    received_time_utc TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    evidence_class TEXT NOT NULL CHECK (
        evidence_class IN (
            'sensor',
            'laboratory',
            'operator_claim',
            'model_inference',
            'public_report'
        )
    ),
    subject_type TEXT NOT NULL CHECK (
        subject_type IN ('lake', 'reservoir', 'aquifer_unit', 'canal', 'monitoring_station')
    ),
    subject_id TEXT NOT NULL,
    variable_code TEXT NOT NULL,
    value_real REAL,
    value_text TEXT,
    unit_code TEXT,
    quality_flag TEXT NOT NULL CHECK (
        quality_flag IN ('verified', 'provisional', 'suspect', 'invalid', 'unverified')
    ),
    source_system TEXT NOT NULL,
    source_record_id TEXT,
    provenance_json TEXT NOT NULL,
    supersedes_evidence_id TEXT REFERENCES evidence_item(evidence_id),
    is_retracted INTEGER NOT NULL DEFAULT 0 CHECK (is_retracted IN (0, 1)),
    CHECK (
        (value_real IS NOT NULL AND value_text IS NULL)
        OR
        (value_real IS NULL AND value_text IS NOT NULL)
    )
);

CREATE TABLE evidence_provenance (
    evidence_id TEXT NOT NULL REFERENCES evidence_item(evidence_id),
    sequence_number INTEGER NOT NULL CHECK (sequence_number >= 1),
    action_time_utc TEXT NOT NULL,
    actor_type TEXT NOT NULL CHECK (
        actor_type IN ('sensor', 'laboratory', 'operator', 'reviewer', 'model', 'public_reporter')
    ),
    actor_id TEXT,
    action_type TEXT NOT NULL CHECK (
        action_type IN (
            'observed',
            'sampled',
            'calibrated',
            'validated',
            'derived',
            'reviewed',
            'retracted'
        )
    ),
    method_reference TEXT,
    input_evidence_id TEXT REFERENCES evidence_item(evidence_id),
    note TEXT,
    PRIMARY KEY (evidence_id, sequence_number)
);

CREATE TABLE corridor_definition (
    corridor_id TEXT PRIMARY KEY NOT NULL,
    variable_code TEXT NOT NULL,
    comparison_operator TEXT NOT NULL CHECK (
        comparison_operator IN ('>=', '<=', 'between')
    ),
    lower_bound REAL,
    upper_bound REAL,
    unit_code TEXT NOT NULL,
    ecological_basis TEXT NOT NULL,
    effective_start_utc TEXT NOT NULL,
    effective_end_utc TEXT,
    CHECK (
        (comparison_operator = '>=' AND lower_bound IS NOT NULL AND upper_bound IS NULL)
        OR
        (comparison_operator = '<=' AND lower_bound IS NULL AND upper_bound IS NOT NULL)
        OR
        (
            comparison_operator = 'between'
            AND lower_bound IS NOT NULL
            AND upper_bound IS NOT NULL
            AND lower_bound <= upper_bound
        )
    )
);

CREATE TABLE corridor_evaluation (
    evaluation_id TEXT PRIMARY KEY NOT NULL,
    corridor_id TEXT NOT NULL REFERENCES corridor_definition(corridor_id),
    evaluated_time_utc TEXT NOT NULL,
    subject_type TEXT NOT NULL,
    subject_id TEXT NOT NULL,
    status TEXT NOT NULL CHECK (
        status IN ('safe', 'warning', 'breached', 'unknown')
    ),
    measurement_evidence_id TEXT REFERENCES evidence_item(evidence_id),
    model_evidence_id TEXT REFERENCES evidence_item(evidence_id),
    uncertainty_low REAL,
    uncertainty_high REAL,
    rationale TEXT NOT NULL,
    CHECK (
        uncertainty_low IS NULL
        OR uncertainty_high IS NULL
        OR uncertainty_low <= uncertainty_high
    )
);

CREATE INDEX idx_evidence_subject_variable_time
    ON evidence_item(subject_type, subject_id, variable_code, event_time_utc DESC);

CREATE INDEX idx_evidence_class_quality_time
    ON evidence_item(evidence_class, quality_flag, event_time_utc DESC);

CREATE INDEX idx_corridor_evaluation_subject_time
    ON corridor_evaluation(subject_type, subject_id, evaluated_time_utc DESC);

CREATE INDEX idx_corridor_evaluation_status
    ON corridor_evaluation(status, evaluated_time_utc DESC);

CREATE VIEW aquifer_sentinel_verdict AS
SELECT
    subject_type,
    subject_id,
    CASE
        WHEN SUM(CASE WHEN status = 'breached' THEN 1 ELSE 0 END) > 0
            THEN 'blocked'
        WHEN SUM(CASE WHEN status = 'unknown' THEN 1 ELSE 0 END) > 0
            THEN 'indeterminate'
        WHEN COUNT(*) = 0
            THEN 'indeterminate'
        ELSE 'eligible'
    END AS ecological_verdict,
    MAX(evaluated_time_utc) AS last_evaluated_utc
FROM corridor_evaluation
GROUP BY subject_type, subject_id;
