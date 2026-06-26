-- spine/econet/db/health_dataset_registry.sql

CREATE TABLE IF NOT EXISTS health_dataset_registry (
    dataset_id           TEXT PRIMARY KEY,
    title                TEXT NOT NULL,
    description          TEXT NOT NULL,
    region_code          TEXT NOT NULL,
    owner_did            TEXT NOT NULL,
    corridor_id          TEXT NOT NULL,
    neurorights_safe     INTEGER NOT NULL,
    evidence_mode        TEXT NOT NULL,
    min_stake_eco        REAL NOT NULL,
    min_stake_fiat       REAL NOT NULL,
    created_utc_ms       INTEGER NOT NULL,
    updated_utc_ms       INTEGER NOT NULL,
    metadata_json        TEXT NOT NULL,
    evidencehex          TEXT NOT NULL,
    signinghex           TEXT
);

-- neurorights_safe is boolean 0/1
ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_health_neurorights_safe_bool
CHECK (neurorights_safe IN (0, 1));

-- evidence_mode must match corridor-safe modes
ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_health_evidence_mode
CHECK (
    evidence_mode IN (
        'TOPIC_VECTOR_ONLY',
        'REDACTED_TEXT_SNIPPET',
        'FULL_TEXT_LOCAL_ONLY'
    )
);

-- stake minimums: non-negative, with at least one positive for PROD-like use
ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_health_stake_minimums
CHECK (
    min_stake_eco >= 0.0
    AND min_stake_fiat >= 0.0
    AND (min_stake_eco > 0.0 OR min_stake_fiat > 0.0)
);

ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_health_timestamps
CHECK (updated_utc_ms >= created_utc_ms);
