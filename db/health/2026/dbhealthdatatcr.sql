-- Path: db/health/2026/dbhealthdatatcr.sql
-- Role: Health-data token-curated registry + QF wiring for EcoNet/Bostrom.

CREATE SCHEMA IF NOT EXISTS health_tcr2026;

CREATE TYPE health_tcr2026.curation_status AS ENUM (
  'PENDING',
  'ACCEPTED',
  'REJECTED',
  'SLASHED'
);

CREATE TYPE health_tcr2026.dataset_category AS ENUM (
  'WEARABLE_VITALS',
  'EEG_NEURO',
  'EHR_SUMMARY',
  'LAB_RESULTS_AGGREGATE',
  'ENV_EXPOSURE',
  'CLIMATE_HEALTH_LINKED',
  'OTHER_HEALTH_RESTRICTED'
);

CREATE TABLE IF NOT EXISTS health_tcr2026.health_dataset_registry (
  dataset_cid                TEXT PRIMARY KEY,
  contributor_address        TEXT NOT NULL,
  curator_address            TEXT NOT NULL,
  stake_amount               NUMERIC(36, 18) NOT NULL CHECK (stake_amount >= 0),
  curation_status            health_tcr2026.curation_status NOT NULL,
  category                   health_tcr2026.dataset_category NOT NULL,
  quality_score              NUMERIC(6, 3) NOT NULL CHECK (quality_score >= 0 AND quality_score <= 1),
  neurorights_safe           BOOLEAN NOT NULL,
  created_at_utc             TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at_utc             TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  msg_publish_id             BIGINT NOT NULL,
  responsibility_scalar_snap NUMERIC(6, 3) NOT NULL CHECK (responsibility_scalar_snap >= 0 AND responsibility_scalar_snap <= 1),
  ker_residual_anchor_id     BIGINT NOT NULL,
  eco_credit_reward_locked   NUMERIC(36, 18) NOT NULL DEFAULT 0,
  eco_credit_reward_vested   NUMERIC(36, 18) NOT NULL DEFAULT 0,
  health_qf_eligible         BOOLEAN NOT NULL DEFAULT FALSE,
  health_qf_round_id         BIGINT,
  CHECK (eco_credit_reward_locked >= 0),
  CHECK (eco_credit_reward_vested >= 0)
);

CREATE INDEX IF NOT EXISTS idx_health_dataset_registry_curator
ON health_tcr2026.health_dataset_registry (curator_address);

CREATE INDEX IF NOT EXISTS idx_health_dataset_registry_status
ON health_tcr2026.health_dataset_registry (curation_status);

CREATE INDEX IF NOT EXISTS idx_health_dataset_registry_qf
ON health_tcr2026.health_dataset_registry (health_qf_eligible, health_qf_round_id);

-- Each curated dataset can be challenged by others.
CREATE TYPE health_tcr2026.challenge_status AS ENUM (
  'OPEN',
  'RESOLVED_UPHOLD',
  'RESOLVED_SLASH'
);

CREATE TABLE IF NOT EXISTS health_tcr2026.health_dataset_challenge (
  challenge_id          BIGSERIAL PRIMARY KEY,
  dataset_cid           TEXT NOT NULL REFERENCES health_tcr2026.health_dataset_registry(dataset_cid) ON DELETE CASCADE,
  challenger_address    TEXT NOT NULL,
  challenge_stake       NUMERIC(36, 18) NOT NULL CHECK (challenge_stake >= 0),
  alleged_violation     TEXT NOT NULL,
  evidence_cid          TEXT NOT NULL,
  status                health_tcr2026.challenge_status NOT NULL DEFAULT 'OPEN',
  created_at_utc        TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  resolved_at_utc       TIMESTAMPTZ,
  slash_ratio_curator   NUMERIC(4, 3) CHECK (slash_ratio_curator >= 0 AND slash_ratio_curator <= 1),
  slash_ratio_challenger NUMERIC(4, 3) CHECK (slash_ratio_challenger >= 0 AND slash_ratio_challenger <= 1)
);

CREATE INDEX IF NOT EXISTS idx_health_dataset_challenge_dataset
ON health_tcr2026.health_dataset_challenge (dataset_cid, status);

-- Record non-transferable patient credits tied to health data labor.
CREATE TABLE IF NOT EXISTS health_tcr2026.health_data_labor_credit (
  id                      BIGSERIAL PRIMARY KEY,
  contributor_address     TEXT NOT NULL,
  dataset_cid             TEXT NOT NULL REFERENCES health_tcr2026.health_dataset_registry(dataset_cid) ON DELETE CASCADE,
  eco_credit_earned       NUMERIC(36, 18) NOT NULL CHECK (eco_credit_earned >= 0),
  gas_discount_bps        INTEGER NOT NULL CHECK (gas_discount_bps >= 0 AND gas_discount_bps <= 10000),
  non_transferable        BOOLEAN NOT NULL DEFAULT TRUE,
  created_at_utc          TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  last_consumed_at_utc    TIMESTAMPTZ,
  remaining_gas_discount  NUMERIC(36, 18) NOT NULL DEFAULT 0 CHECK (remaining_gas_discount >= 0)
);

CREATE INDEX IF NOT EXISTS idx_health_data_labor_credit_contributor
ON health_tcr2026.health_data_labor_credit (contributor_address);

-- Quadratic funding pool for health campaigns.
CREATE TABLE IF NOT EXISTS health_tcr2026.health_qf_pool (
  pool_id             BIGSERIAL PRIMARY KEY,
  round_label         TEXT NOT NULL,
  matching_pool_boot  NUMERIC(36, 18) NOT NULL CHECK (matching_pool_boot >= 0),
  matching_pool_ecocredit NUMERIC(36, 18) NOT NULL CHECK (matching_pool_ecocredit >= 0),
  round_start_utc     TIMESTAMPTZ NOT NULL,
  round_end_utc       TIMESTAMPTZ NOT NULL,
  ker_residual_anchor_id BIGINT NOT NULL,
  created_at_utc      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS health_tcr2026.health_qf_campaign (
  campaign_id         BIGSERIAL PRIMARY KEY,
  pool_id             BIGINT NOT NULL REFERENCES health_tcr2026.health_qf_pool(pool_id) ON DELETE CASCADE,
  ecocampaign_id      BIGINT NOT NULL,
  creator_address     TEXT NOT NULL,
  created_at_utc      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  is_healthcare       BOOLEAN NOT NULL DEFAULT TRUE,
  qf_matching_received_boot NUMERIC(36, 18) NOT NULL DEFAULT 0 CHECK (qf_matching_received_boot >= 0),
  qf_matching_received_ecocredit NUMERIC(36, 18) NOT NULL DEFAULT 0 CHECK (qf_matching_received_ecocredit >= 0)
);

CREATE INDEX IF NOT EXISTS idx_health_qf_campaign_pool
ON health_tcr2026.health_qf_campaign (pool_id);

CREATE TABLE IF NOT EXISTS health_tcr2026.health_qf_contribution (
  contribution_id     BIGSERIAL PRIMARY KEY,
  campaign_id         BIGINT NOT NULL REFERENCES health_tcr2026.health_qf_campaign(campaign_id) ON DELETE CASCADE,
  contributor_address TEXT NOT NULL,
  contributed_boot    NUMERIC(36, 18) NOT NULL CHECK (contributed_boot >= 0),
  contributed_ecocredit NUMERIC(36, 18) NOT NULL CHECK (contributed_ecocredit >= 0),
  cumulative_ecocredit_snap NUMERIC(36, 18) NOT NULL CHECK (cumulative_ecocredit_snap >= 0),
  responsibility_scalar_snap NUMERIC(6, 3) NOT NULL CHECK (responsibility_scalar_snap >= 0 AND responsibility_scalar_snap <= 1),
  created_at_utc      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_health_qf_contribution_campaign
ON health_tcr2026.health_qf_contribution (campaign_id);

-- Gate: only neurorights-safe, HASHONLY, health-appropriate datasets can enter the registry.
CREATE OR REPLACE FUNCTION health_tcr2026.enforce_health_dataset_registry_gate()
RETURNS TRIGGER AS $$
DECLARE
  v_data_access_policy TEXT;
  v_evidence_mode      TEXT;
  v_bci_signature      TEXT;
BEGIN
  SELECT
    m.data_access_policy,
    m.evidence_mode,
    m.bci_challenge_signature
  INTO
    v_data_access_policy,
    v_evidence_mode,
    v_bci_signature
  FROM eco_core.msg_publish_data_contribution AS m
  WHERE m.id = NEW.msg_publish_id
  LIMIT 1;

  IF v_data_access_policy NOT IN ('DATAACCESSRESTRICTEDHEALTHCARE', 'DATAACCESSOPENCLIMATERESEARCH') THEN
    RAISE EXCEPTION 'HealthDataTCR: invalid data_access_policy % for dataset_cid %', v_data_access_policy, NEW.dataset_cid;
  END IF;

  IF v_evidence_mode <> 'HASHONLY' THEN
    RAISE EXCEPTION 'HealthDataTCR: evidence_mode must be HASHONLY for dataset_cid %', NEW.dataset_cid;
  END IF;

  IF v_bci_signature IS NULL THEN
    RAISE EXCEPTION 'HealthDataTCR: missing BCIChallengeSignature for dataset_cid %', NEW.dataset_cid;
  END IF;

  IF NEW.neurorights_safe IS FALSE THEN
    RAISE EXCEPTION 'HealthDataTCR: neurorights_safe must be TRUE for dataset_cid %', NEW.dataset_cid;
  END IF;

  NEW.updated_at_utc := NOW();
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_health_dataset_registry_gate ON health_tcr2026.health_dataset_registry;

CREATE TRIGGER trg_health_dataset_registry_gate
BEFORE INSERT OR UPDATE ON health_tcr2026.health_dataset_registry
FOR EACH ROW
EXECUTE FUNCTION health_tcr2026.enforce_health_dataset_registry_gate();

-- Neurorights: refusal to contribute data MUST NOT reduce Karma or eco benefits.
-- We encode an invariant table for audits (off-chain proofs).
CREATE TABLE IF NOT EXISTS health_tcr2026.neurorights_invariant_log (
  log_id               BIGSERIAL PRIMARY KEY,
  subject_address      TEXT NOT NULL,
  invariance_type      TEXT NOT NULL, -- e.g. "NO_DOWNGRADE_FOR_NONDISCLOSURE"
  related_dataset_cid  TEXT,
  created_at_utc       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  ker_residual_anchor_id BIGINT NOT NULL
);
