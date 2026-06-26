-- Path: db/healthdata_tcr/healthdata_tcr_schema_v1.sql

PRAGMA foreign_keys = ON;

------------------------------------------------------------------------------
-- Core registry: health_dataset_registry
------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS health_dataset_registry (
    dataset_id              TEXT PRIMARY KEY,          -- koid or UUID
    name                    TEXT NOT NULL,
    description             TEXT NOT NULL,
    owner_union_id          TEXT NOT NULL,             -- data union / coop
    birth_sign_id           TEXT NOT NULL,
    neurorights_policy_id   TEXT NOT NULL,
    brain_did               TEXT NOT NULL,             -- DID of primary brain-owner
    neurorights_corridor_id TEXT NOT NULL,             -- bound neuroright corridor spec
    evidence_mode           TEXT NOT NULL,             -- 'HASHONLY' | 'PSEUDONYMOUS_FEATURES' | 'AGGREGATED_STATISTICS'
    neurorights_safe        INTEGER NOT NULL,          -- 0 = false, 1 = true
    stake_min_credits       INTEGER NOT NULL,          -- microunits, >= 0
    stake_locked_credits    INTEGER NOT NULL DEFAULT 0,
    labor_event_ids         TEXT NOT NULL DEFAULT '[]',-- JSON array of opaque ids/hashes
    created_at_utc          TEXT NOT NULL,             -- ISO8601
    updated_at_utc          TEXT NOT NULL,             -- ISO8601
    bci_challenge_sig       TEXT NOT NULL              -- base64url signature over dataset challenge
);

CREATE TABLE IF NOT EXISTS health_dataset_registry_tag (
    dataset_id  TEXT NOT NULL,
    tag         TEXT NOT NULL,
    PRIMARY KEY (dataset_id, tag),
    FOREIGN KEY (dataset_id) REFERENCES health_dataset_registry(dataset_id) ON DELETE CASCADE
);

------------------------------------------------------------------------------
-- Labor credit ledger: monotone, idempotent gas-discount accounting
------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS health_data_labor_credit_ledger (
    op_id            TEXT PRIMARY KEY,   -- unique operation id (hash)
    account_id       TEXT NOT NULL,
    dataset_id       TEXT NOT NULL,
    delta_credits    INTEGER NOT NULL,   -- negative for consumption, positive for awards
    created_at_utc   TEXT NOT NULL,      -- ISO8601
    context          TEXT NOT NULL,      -- e.g. 'GAS_DISCOUNT', 'SLASH_RESTITUTION',
                                         --      'INITIAL_GRANT', 'CHALLENGE_REWARD'
    FOREIGN KEY (dataset_id) REFERENCES health_dataset_registry(dataset_id) ON DELETE CASCADE
);

CREATE VIEW IF NOT EXISTS health_data_labor_credit_balance AS
SELECT
    account_id,
    SUM(delta_credits) AS balance_credits
FROM health_data_labor_credit_ledger
GROUP BY account_id;

------------------------------------------------------------------------------
-- CHECK constraints on health_dataset_registry
------------------------------------------------------------------------------

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

-- If dataset is not neurorights-safe, enforce a higher minimum stake, e.g. ≥ 1_000_000 microunits.
ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_stake_min_vs_neurorights
CHECK (
  (neurorights_safe = 1 AND stake_min_credits >= 0)
  OR
  (neurorights_safe = 0 AND stake_min_credits >= 1000000)
);

-- HASHONLY and neurorights-safe datasets must not store raw or pseudonymous evidence
ALTER TABLE health_dataset_registry
ADD CONSTRAINT chk_hashonly_neurorights_safe_mode
CHECK (
  neurorights_safe = 0
  OR evidence_mode IN ('HASHONLY', 'PSEUDONYMOUS_FEATURES')
);

------------------------------------------------------------------------------
-- Indexes for fast access patterns
------------------------------------------------------------------------------

-- Lookup datasets by BrainDid, filtered by neurorights status
CREATE INDEX IF NOT EXISTS idx_health_dataset_by_brain_did
ON health_dataset_registry (brain_did, neurorights_safe, dataset_id);

-- Lookup datasets by neurorights corridor binding
CREATE INDEX IF NOT EXISTS idx_health_dataset_by_corridor
ON health_dataset_registry (neurorights_corridor_id, neurorights_safe, dataset_id);

-- Labor credit queries by account
CREATE INDEX IF NOT EXISTS idx_labor_credit_by_account
ON health_data_labor_credit_ledger (account_id, created_at_utc);

-- Labor credit queries by dataset
CREATE INDEX IF NOT EXISTS idx_labor_credit_by_dataset
ON health_data_labor_credit_ledger (dataset_id, created_at_utc);

------------------------------------------------------------------------------
-- Triggers to enforce registry gates and monotone credit behavior
------------------------------------------------------------------------------

-- UDFs expected from the Rust layer:
--   validate_bci_challenge(brain_did TEXT, dataset_id TEXT, sig TEXT) RETURNS INTEGER
--   validate_labor_event_ids(dataset_id TEXT, labor_event_ids TEXT) RETURNS INTEGER
-- They must be registered before using this schema.

CREATE TRIGGER IF NOT EXISTS trg_health_dataset_registry_before_insert
BEFORE INSERT ON health_dataset_registry
FOR EACH ROW
BEGIN
  -- Enforce neurorights-safe evidence mode constraint at gate time
  SELECT
    CASE
      WHEN NEW.neurorights_safe = 1
           AND NEW.evidence_mode NOT IN ('HASHONLY', 'PSEUDONYMOUS_FEATURES')
      THEN RAISE(ABORT, 'neurorights_safe dataset must be HASHONLY or PSEUDONYMOUS_FEATURES')
    END;

  -- Validate BCI challenge signature
  SELECT
    CASE
      WHEN validate_bci_challenge(NEW.brain_did, NEW.dataset_id, NEW.bci_challenge_sig) = 0
      THEN RAISE(ABORT, 'invalid BCI challenge signature')
    END;

  -- Validate labor_event_ids linkage semantics (JSON, hashes only, etc.)
  SELECT
    CASE
      WHEN validate_labor_event_ids(NEW.dataset_id, NEW.labor_event_ids) = 0
      THEN RAISE(ABORT, 'invalid labor_event_ids payload')
    END;
END;

CREATE TRIGGER IF NOT EXISTS trg_health_dataset_registry_before_update
BEFORE UPDATE ON health_dataset_registry
FOR EACH ROW
BEGIN
  -- Prevent evidence_mode from becoming less protective after creation
  SELECT
    CASE
      WHEN OLD.evidence_mode = 'HASHONLY'
           AND NEW.evidence_mode <> 'HASHONLY'
      THEN RAISE(ABORT, 'evidence_mode cannot be relaxed from HASHONLY')
    END;

  -- Preserve neurorights_safe flag from being downgraded without re-registration
  SELECT
    CASE
      WHEN OLD.neurorights_safe = 1
           AND NEW.neurorights_safe = 0
      THEN RAISE(ABORT, 'neurorights_safe flag cannot be switched off in-place')
    END;

  -- Re-validate labor_event_ids on change
  SELECT
    CASE
      WHEN NEW.labor_event_ids <> OLD.labor_event_ids
           AND validate_labor_event_ids(NEW.dataset_id, NEW.labor_event_ids) = 0
      THEN RAISE(ABORT, 'invalid labor_event_ids payload on update')
    END;
END;

-- Ensure labor credits never drift via non-monotone UPDATEs
-- By design, all changes must go through INSERTs into the ledger with unique op_id.
CREATE TRIGGER IF NOT EXISTS trg_labor_credit_ledger_block_updates
BEFORE UPDATE ON health_data_labor_credit_ledger
FOR EACH ROW
BEGIN
  SELECT RAISE(ABORT, 'updates to health_data_labor_credit_ledger are not allowed; use INSERT with new op_id');
END;

CREATE TRIGGER IF NOT EXISTS trg_labor_credit_ledger_block_deletes
BEFORE DELETE ON health_data_labor_credit_ledger
FOR EACH ROW
BEGIN
  SELECT RAISE(ABORT, 'deletes from health_data_labor_credit_ledger are not allowed; use compensating INSERTs');
END;
