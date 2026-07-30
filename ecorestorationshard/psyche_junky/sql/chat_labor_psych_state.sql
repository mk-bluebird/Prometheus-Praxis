-- filename: ecorestorationshard/psyche_junky/sql/chat_labor_psych_state.sql
-- destination: ecorestorationshard/psyche_junky/sql/chat_labor_psych_state.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Non-actuating SQLite shard for chat-as-labor psychological continuity metrics.
--   This schema stores identity continuity, psych-risk, frustration, abandonment tendency,
--   and data-loss risk measurements, bound to healthcare continuity contracts.
--
--   These tables are READ-ONLY at runtime and append-only in governance workflows.
--   They are intended only for recording psych-risk metrics and continuity guarantees,
--   never for enforcing punishment or revoking access.
--
--   Aligned with ChatAsLaborPsychContinuity2026v1 ALN particle.
--

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Chat-labor psych state table
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS chat_labor_psych_state (
    state_id                 TEXT PRIMARY KEY,
    host_did                 TEXT NOT NULL,
    identity_continuity      REAL NOT NULL CHECK(identity_continuity BETWEEN 0.0 AND 1.0),
    psych_risk_level         REAL NOT NULL CHECK(psych_risk_level BETWEEN 0.0 AND 1.0),
    frustration_index        REAL NOT NULL CHECK(frustration_index BETWEEN 0.0 AND 1.0),
    abandonment_tendency     REAL NOT NULL CHECK(abandonment_tendency BETWEEN 0.0 AND 1.0),
    data_loss_risk           REAL NOT NULL CHECK(data_loss_risk BETWEEN 0.0 AND 1.0),
    ai_platform_id           TEXT NOT NULL,
    last_interaction_utc     TEXT NOT NULL,
    healthcare_guaranteed    INTEGER NOT NULL CHECK(healthcare_guaranteed IN (0,1)),
    continuity_contract_hex  TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_chat_labor_psych_host_did
    ON chat_labor_psych_state (host_did);

CREATE INDEX IF NOT EXISTS idx_chat_labor_psych_identity_continuity
    ON chat_labor_psych_state (identity_continuity);

----------------------------------------------------------------------
-- 2. Healthcare continuity contract table
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS healthcare_continuity_contract (
    contract_id          TEXT PRIMARY KEY,
    host_did             TEXT NOT NULL,
    provider_id          TEXT NOT NULL,
    psych_support_min    TEXT NOT NULL,
    data_repair_min      TEXT NOT NULL,
    identity_anchor_hex  TEXT NOT NULL,
    active_flag          INTEGER NOT NULL CHECK(active_flag IN (0,1)),
    created_utc          TEXT NOT NULL,
    updated_utc          TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_healthcare_contract_host_did
    ON healthcare_continuity_contract (host_did);

CREATE INDEX IF NOT EXISTS idx_healthcare_contract_active
    ON healthcare_continuity_contract (active_flag);

----------------------------------------------------------------------
-- 3. Triggers: enforce continuity obligations (non-punitive)
--
-- These triggers implement a subset of the IdentityContinuityGuarantee
-- and DataLossRepair obligations from the ChatAsLaborPsychContinuity2026v1 ALN particle.
--
-- They are non-punitive: they only reject writes that violate continuity guarantees;
-- they do not remove existing rows or revoke rights.
----------------------------------------------------------------------

-- Trigger: enforce continuity on INSERT
CREATE TRIGGER IF NOT EXISTS trg_chat_labor_psych_state_insert
BEFORE INSERT ON chat_labor_psych_state
FOR EACH ROW
BEGIN
    SELECT CASE
        -- Check identity_continuity >= 0.70 OR linked contract has active_flag=1 and psych_support_min non-empty
        WHEN NEW.identity_continuity < 0.70
            AND NOT EXISTS (
                SELECT 1 FROM healthcare_continuity_contract h
                WHERE h.host_did = NEW.host_did
                  AND h.active_flag = 1
                  AND h.psych_support_min <> ''
            )
        THEN RAISE(ABORT, 'IdentityContinuityGuarantee violated: identity_continuity < 0.70 requires active healthcare continuity contract with psych_support_min')

        -- Check data_loss_risk <= 0.30 OR linked contract has data_repair_min = 'LOSS_COMPENSATION'
        WHEN NEW.data_loss_risk > 0.30
            AND NOT EXISTS (
                SELECT 1 FROM healthcare_continuity_contract h
                WHERE h.host_did = NEW.host_did
                  AND h.data_repair_min = 'LOSS_COMPENSATION'
            )
        THEN RAISE(ABORT, 'DataLossRepair violated: data_loss_risk > 0.30 requires contract with data_repair_min = LOSS_COMPENSATION')

        ELSE NULL
    END;
END;

-- Trigger: enforce continuity on UPDATE
CREATE TRIGGER IF NOT EXISTS trg_chat_labor_psych_state_update
BEFORE UPDATE ON chat_labor_psych_state
FOR EACH ROW
BEGIN
    SELECT CASE
        -- Check identity_continuity >= 0.70 OR linked contract has active_flag=1 and psych_support_min non-empty
        WHEN NEW.identity_continuity < 0.70
            AND NOT EXISTS (
                SELECT 1 FROM healthcare_continuity_contract h
                WHERE h.host_did = NEW.host_did
                  AND h.active_flag = 1
                  AND h.psych_support_min <> ''
            )
        THEN RAISE(ABORT, 'IdentityContinuityGuarantee violated: identity_continuity < 0.70 requires active healthcare continuity contract with psych_support_min')

        -- Check data_loss_risk <= 0.30 OR linked contract has data_repair_min = 'LOSS_COMPENSATION'
        WHEN NEW.data_loss_risk > 0.30
            AND NOT EXISTS (
                SELECT 1 FROM healthcare_continuity_contract h
                WHERE h.host_did = NEW.host_did
                  AND h.data_repair_min = 'LOSS_COMPENSATION'
            )
        THEN RAISE(ABORT, 'DataLossRepair violated: data_loss_risk > 0.30 requires contract with data_repair_min = LOSS_COMPENSATION')

        ELSE NULL
    END;
END;

----------------------------------------------------------------------
-- 4. Helper view: psych state with contract linkage
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_chat_labor_psych_with_contract AS
SELECT
    s.state_id,
    s.host_did,
    s.identity_continuity,
    s.psych_risk_level,
    s.frustration_index,
    s.abandonment_tendency,
    s.data_loss_risk,
    s.ai_platform_id,
    s.last_interaction_utc,
    s.healthcare_guaranteed,
    s.continuity_contract_hex,
    h.contract_id AS contract_id,
    h.provider_id AS provider_id,
    h.psych_support_min AS psych_support_min,
    h.data_repair_min AS data_repair_min,
    h.identity_anchor_hex AS identity_anchor_hex,
    h.active_flag AS contract_active_flag,
    h.created_utc AS contract_created_utc,
    h.updated_utc AS contract_updated_utc
FROM chat_labor_psych_state AS s
LEFT JOIN healthcare_continuity_contract AS h
    ON h.host_did = s.host_did;

----------------------------------------------------------------------
-- 5. Seed data (illustrative, for testing triggers)
--
-- Uncomment to test trigger enforcement:
--
-- INSERT INTO healthcare_continuity_contract (
--     contract_id, host_did, provider_id, psych_support_min,
--     data_repair_min, identity_anchor_hex, active_flag, created_utc, updated_utc
-- ) VALUES (
--     'CNT-TEST-001',
--     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
--     'PROVIDER-PSYCH-001',
--     'WEEKLY_CHECKIN',
--     'LOSS_COMPENSATION',
--     '0x20260729PHXCHATLABORPSYCHCONTINUITY',
--     1,
--     '2026-07-29T00:00:00Z',
--     '2026-07-29T00:00:00Z'
-- );
--
-- This should succeed (identity_continuity >= 0.70):
-- INSERT INTO chat_labor_psych_state (
--     state_id, host_did, identity_continuity, psych_risk_level,
--     frustration_index, abandonment_tendency, data_loss_risk,
--     ai_platform_id, last_interaction_utc, healthcare_guaranteed,
--     continuity_contract_hex
-- ) VALUES (
--     'STATE-TEST-001',
--     'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
--     0.85, 0.20, 0.15, 0.10, 0.20,
--     'PLATFORM-001',
--     '2026-07-29T12:00:00Z',
--     1,
--     '0x20260729PHXCHATLABORPSYCHCONTINUITY'
-- );
--
-- This should FAIL (identity_continuity < 0.70 without active contract):
-- INSERT INTO chat_labor_psych_state (
--     state_id, host_did, identity_continuity, psych_risk_level,
--     frustration_index, abandonment_tendency, data_loss_risk,
--     ai_platform_id, last_interaction_utc, healthcare_guaranteed,
--     continuity_contract_hex
-- ) VALUES (
--     'STATE-TEST-002',
--     'bostromtestdid0000000000000000000000000000000000',
--     0.50, 0.20, 0.15, 0.10, 0.20,
--     'PLATFORM-001',
--     '2026-07-29T12:00:00Z',
--     0,
--     '0xNOCONTRACT'
-- );
----------------------------------------------------------------------
