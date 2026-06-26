-- Assumes table health_dataset_registry as in your prior DDL, plus:
--   brain_did TEXT NOT NULL
--   bci_challenge_sig TEXT NOT NULL   -- base64url of signature

CREATE TABLE health_dataset_registry (
  dataset_id          TEXT PRIMARY KEY,
  name                TEXT NOT NULL,
  description         TEXT NOT NULL,
  owner_union_id      TEXT NOT NULL,
  birth_sign_id       TEXT NOT NULL,
  neurorights_policy_id TEXT NOT NULL,
  brain_did           TEXT NOT NULL,
  bci_challenge_sig   TEXT NOT NULL,
  evidence_mode       TEXT NOT NULL,
  neurorights_safe    INTEGER NOT NULL,
  stake_min_credits   INTEGER NOT NULL,
  stake_locked_credits INTEGER NOT NULL DEFAULT 0,
  created_at_utc      TEXT NOT NULL,
  updated_at_utc      TEXT NOT NULL
);

-- Deterministic scalar UDF to be registered from Rust:
--   validate_bci_challenge(brain_did, dataset_id, bci_challenge_sig) -> 0/1
-- It verifies the signature over canonical message:
--   "HealthDataTCR2026v1|" || dataset_id
-- using the DID's registered public key and returns 1 on success.
-- The Rust side ensures this uses non-blacklisted crypto.[file:5][file:6]

CREATE TRIGGER trg_health_dataset_registry_gate
BEFORE INSERT ON health_dataset_registry
FOR EACH ROW
BEGIN
  -- Neurorights-safe datasets MUST have HASHONLY or PSEUDONYMOUS_FEATURES
  SELECT
    CASE
      WHEN NEW.neurorights_safe = 1
           AND NEW.evidence_mode NOT IN ('HASHONLY', 'PSEUDONYMOUS_FEATURES')
      THEN RAISE(ABORT, 'neurorights_safe dataset must be HASHONLY or PSEUDONYMOUS_FEATURES')
    END;

  -- Enforce BCI challenge signature gate
  SELECT
    CASE
      WHEN validate_bci_challenge(NEW.brain_did, NEW.dataset_id, NEW.bci_challenge_sig) = 0
      THEN RAISE(ABORT, 'invalid BCI challenge signature')
    END;
END;
