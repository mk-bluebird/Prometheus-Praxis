CREATE TABLE health_dataset_registry (
    dataset_id          TEXT PRIMARY KEY,         -- koid or UUID
    name                TEXT NOT NULL,
    description         TEXT NOT NULL,
    owner_union_id      TEXT NOT NULL,            -- data union or coop
    birth_sign_id       TEXT NOT NULL,
    neurorights_policy_id TEXT NOT NULL,
    evidence_mode       TEXT NOT NULL,            -- 'HASHONLY' | 'PSEUDONYMOUS_FEATURES' | 'AGGREGATED_STATISTICS'
    neurorights_safe    INTEGER NOT NULL,         -- 0 = false, 1 = true
    stake_min_credits   INTEGER NOT NULL,         -- microunits, >= 0
    stake_locked_credits INTEGER NOT NULL DEFAULT 0,
    created_at_utc      TEXT NOT NULL,            -- ISO8601
    updated_at_utc      TEXT NOT NULL
);

CREATE TABLE health_dataset_registry_tag (
    dataset_id  TEXT NOT NULL REFERENCES health_dataset_registry(dataset_id) ON DELETE CASCADE,
    tag         TEXT NOT NULL,
    PRIMARY KEY (dataset_id, tag)
);

-- CHECK constraints
ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_evidence_mode
CHECK (evidence_mode IN ('HASHONLY', 'PSEUDONYMOUS_FEATURES', 'AGGREGATED_STATISTICS'));

ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_neurorights_safe_bool
CHECK (neurorights_safe IN (0, 1));

ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_stake_min_nonnegative
CHECK (stake_min_credits >= 0);

ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_stake_locked_nonnegative
CHECK (stake_locked_credits >= 0);

-- If dataset is not neurorights-safe, enforce a higher minimum stake, e.g. ≥ 1_000_000 microunits
ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_stake_min_vs_neurorights
CHECK (
  (neurorights_safe = 1 AND stake_min_credits >= 0)
  OR
  (neurorights_safe = 0 AND stake_min_credits >= 1000000)
);
