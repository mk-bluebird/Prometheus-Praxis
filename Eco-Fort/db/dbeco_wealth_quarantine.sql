-- filename: Eco-Fort/db/dbeco_wealth_quarantine.sql
-- destination: Eco-Fort/db/dbeco_wealth_quarantine.sql
-- Purpose:
--   SQL mirror of EcoWealthQuarantineContract2026v1.aln, wired into
--   eco_wealth_ledger, shardinstance, Paycomp, and lane promotion.[file:11]

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 37. eco_wealth_ledger shardid archival strategy with tombstones
-------------------------------------------------------------------------------
-- Assumptions:
--   - eco_wealth_ledger(entry_id, steward_id, shardid, amount, timestamputc,...)
--   - shardinstance(shardid PRIMARY KEY, ...) as in the discovery spine.[file:11]

-- Tombstone table for archived shards: preserves identity and minimal KER
-- needed for audit while allowing physical data to move to cold storage.[file:11]

CREATE TABLE IF NOT EXISTS shardinstance_tombstone (
  shardid      INTEGER PRIMARY KEY,
  archived_at  TEXT NOT NULL,      -- ISO-8601
  region       TEXT,
  lane         TEXT,
  kmetric      REAL,
  emetric      REAL,
  rmetric      REAL,
  vtmax        REAL,
  evidencehex  TEXT,
  signingdid   TEXT
);

-- Foreign-key strategy:
--   - eco_wealth_ledger.shardid references EITHER shardinstance OR tombstone.
--   - We cannot express OR directly as a DB constraint, so we choose:
--       * eco_wealth_ledger.shardid REFERENCES shardinstance(shardid) ON DELETE NO ACTION
--     and a migration rule:
--       * Before deleting shardinstance(shardid), move a copy into
--         shardinstance_tombstone and update eco_wealth_ledger rows to
--         keep shardid but rely on tombstone for audit.[file:11]
--
-- This keeps ledger rows stable (no NULL) and preserves auditability.

-- Helper view for auditors: resolves current shard state or tombstone.

DROP VIEW IF EXISTS v_ledger_shard_audit;

CREATE VIEW v_ledger_shard_audit AS
SELECT
  l.entry_id,
  l.steward_id,
  l.shardid,
  l.amount,
  l.timestamputc,
  COALESCE(s.region, t.region)    AS region,
  COALESCE(s.lane, t.lane)        AS lane,
  COALESCE(s.kmetric, t.kmetric)  AS kmetric,
  COALESCE(s.emetric, t.emetric)  AS emetric,
  COALESCE(s.rmetric, t.rmetric)  AS rmetric,
  COALESCE(s.vtmax, t.vtmax)      AS vtmax,
  COALESCE(s.evidencehex, t.evidencehex) AS evidencehex,
  COALESCE(s.signingdid, t.signingdid)   AS signingdid
FROM eco_wealth_ledger AS l
LEFT JOIN shardinstance AS s
  ON s.shardid = l.shardid
LEFT JOIN shardinstance_tombstone AS t
  ON t.shardid = l.shardid;

-- Archival procedure (to be implemented in Rust, not SQL):
--   1) INSERT INTO shardinstance_tombstone(...) SELECT ... FROM shardinstance WHERE shardid = ?.
--   2) DELETE FROM shardinstance WHERE shardid = ?.
--   3) eco_wealth_ledger rows retain shardid; v_ledger_shard_audit still resolves.[file:11]


-------------------------------------------------------------------------------
-- Eco-wealth quarantine contract tables
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_wealth_quarantine_rule (
  contractid        INTEGER NOT NULL,   -- planeweightscontract or eco-wealth policy
  ruleid            INTEGER NOT NULL,
  scope             TEXT NOT NULL CHECK (scope IN ('STEWARD','SHARD','REGION','CONTRACT')),
  severity          TEXT NOT NULL CHECK (severity IN ('SOFT','HARD')),
  cause             TEXT NOT NULL CHECK (cause IN (
                        'RISK_SPIKE','DATA_QUALITY','GOVERNANCE_ORDER',
                        'SENSOR_CONFLICT','ROH_VIOLATION','OTHER'
                      )),
  max_propagation_s INTEGER NOT NULL,
  apply_to_paycomp  INTEGER NOT NULL CHECK (apply_to_paycomp IN (0,1)),
  apply_to_lane     INTEGER NOT NULL CHECK (apply_to_lane IN (0,1)),
  auto_clear_s      INTEGER NOT NULL DEFAULT 0,
  require_roh_anchor INTEGER NOT NULL DEFAULT 0 CHECK (require_roh_anchor IN (0,1)),
  notes             TEXT,
  PRIMARY KEY (contractid, ruleid),
  FOREIGN KEY (contractid)
    REFERENCES planeweightscontract(contractid)
    ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS eco_wealth_quarantine_state (
  state_id          INTEGER PRIMARY KEY AUTOINCREMENT,
  scope             TEXT NOT NULL CHECK (scope IN ('STEWARD','SHARD','REGION','CONTRACT')),
  steward_id        TEXT,
  shardid           INTEGER,
  region            TEXT,
  contractid        INTEGER,
  active            INTEGER NOT NULL CHECK (active IN (0,1)),
  severity          TEXT NOT NULL CHECK (severity IN ('SOFT','HARD')),
  cause             TEXT NOT NULL CHECK (cause IN (
                        'RISK_SPIKE','DATA_QUALITY','GOVERNANCE_ORDER',
                        'SENSOR_CONFLICT','ROH_VIOLATION','OTHER'
                      )),
  ruleid            INTEGER NOT NULL,
  kerblock_id       TEXT NOT NULL,
  ts_asserted_utc   TEXT NOT NULL,
  first_observed_in_paycomp TEXT,
  first_observed_in_lane    TEXT,
  ts_cleared_utc    TEXT,
  cleared_by_did    TEXT,
  rohanchorhex      TEXT,
  FOREIGN KEY (contractid, ruleid)
    REFERENCES eco_wealth_quarantine_rule(contractid, ruleid)
    ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_eco_wealth_quarantine_state_scope
  ON eco_wealth_quarantine_state (scope, steward_id, shardid, region, contractid);

-- Read-only view wiring quarantine to Paycomp and lane modules.

DROP VIEW IF EXISTS v_quarantine_effective_for_paycomp;
CREATE VIEW v_quarantine_effective_for_paycomp AS
SELECT
  s.state_id,
  s.scope,
  s.steward_id,
  s.shardid,
  s.region,
  s.contractid,
  r.severity,
  r.max_propagation_s,
  s.kerblock_id,
  s.ts_asserted_utc
FROM eco_wealth_quarantine_state AS s
JOIN eco_wealth_quarantine_rule  AS r
  ON r.contractid = s.contractid AND r.ruleid = s.ruleid
WHERE s.active = 1
  AND r.apply_to_paycomp = 1;

DROP VIEW IF EXISTS v_quarantine_effective_for_lane;
CREATE VIEW v_quarantine_effective_for_lane AS
SELECT
  s.state_id,
  s.scope,
  s.steward_id,
  s.shardid,
  s.region,
  s.contractid,
  r.severity,
  r.max_propagation_s,
  s.kerblock_id,
  s.ts_asserted_utc
FROM eco_wealth_quarantine_state AS s
JOIN eco_wealth_quarantine_rule  AS r
  ON r.contractid = s.contractid AND r.ruleid = s.ruleid
WHERE s.active = 1
  AND r.apply_to_lane = 1;


-------------------------------------------------------------------------------
-- 36. Genesis allocation with potential-based component
-------------------------------------------------------------------------------
-- Genesis policy objection:
--   - Pure ΔR-based reward favors historically degraded regions and may
--     under-reward low-degradation, high-potential areas.[file:11]
--
-- Contract pattern (conceptual, not enforced directly in SQL):
--   Define per-region eco-wealth weight as:
--
--     W_region = α * NormalizedPastDeltaR + (1 - α) * PotentialFactor
--
--   where:
--     - NormalizedPastDeltaR is scaled ΔR (residual reduction) over a
--       reference window.
--     - PotentialFactor is derived from:
--         * Baseline residual V_baseline (higher baseline risk gives
--           more potential, but is capped).
--         * Corridor-based restoration capacity metrics (hydraulics,
--           biodiversity, carbon) from shardinstance and knowledgeecoscore.
--     - α in (0,1) is chosen so that:
--         * Potential-only regions (no past ΔR) still earn some share
--           if their potential is high.
--         * Regions with large, verified past ΔR are not starved.
--
-- Guardrail against rewarding inaction:
--   - PotentialFactor MUST down-weight regions whose K/E or data quality
--     K is low (i.e., poor evidence or governance).
--   - Genesis contract forbids using PotentialFactor alone; there MUST be
--     some observed KER or knowledgeecoscore activity to unlock a floor
--     allocation.[file:11]


-------------------------------------------------------------------------------
-- 38. Minimal KER invariants in AI-chat TEE and attestation protocol
-------------------------------------------------------------------------------
-- Minimal invariants for prompt-filter TEE (conceptual):
--
--   1) Log completeness:
--        Every external prompt and model response that contributes to
--        eco-wealth decisions or corridor changes MUST be logged as a
--        PromptShard with:
--           - riskvector snapshot (r, Vt, KER) at time of use.
--           - lane, roleband, DID, and evidencehex.[file:11]
--
--   2) Monotone residual check:
--        For any sequence of TEE-mediated planning steps that update
--        shard proposals, the TEE MUST enforce checksafestep-style
--        Vt1 <= Vt and K/E/R bounds before allowing a proposal to
--        be forwarded to non-TEE components.[file:11]
--
--   3) Non-actuating boundary:
--        TEE must be NonActuatingWorkload-only: it may emit shard
--        proposals and prompt logs, but never directly call any
--        actuator-facing API.
--
-- Lightweight attestation:
--   - TEE maintains an internal, append-only log of PromptShards with:
--       * monotone log index,
--       * per-entry hash h_i,
--       * cumulative hash H_n = H(H_{n-1} || h_n) using an allowed hash.
--   - Periodically, the TEE emits an attestation shard:
--       TeePromptLogAttestation2026v1 with fields:
--         * tee_id, KerBlock id, H_n, time window, KER summary.[file:11]
--   - Paycomp and governance treat eco-wealth decisions as valid only if:
--       * They reference at least one recent TeePromptLogAttestation
--         consistent with the decision’s inputs.
--       * The attestation’s H_n chain verifies against stored prompt logs.
--
-- This avoids re-implementing the full grammar: only residual inequality,
-- K/E/R corridor checks, and the RoH-like log-chain are enforced inside
-- the TEE. All other ecosafety semantics remain in the spine.[file:11]


-------------------------------------------------------------------------------
-- 40. Life-safety lane DP override and multi-source corroboration
-------------------------------------------------------------------------------
-- Objection:
--   - Life-safety overrides could be triggered by compromised sensors
--     emitting fake high-R events.[file:11]
--
-- Contract-level mitigation (conceptual):
--
--   1) Multi-plane, multi-source rule:
--      A life-safety override can only fire if:
--        - At least two independent telemetry sources agree on
--          high-R condition within a KerBlock window:
--            * e.g., local IoT sensors AND upstream hydrological
--              shards, OR
--            * local air sensors AND satellite / remote data.
--        - At least one source must be non-local or differently owned
--          (different DID / region / provider).
--
--   2) Data-quality gating:
--      - Both sources must have rcalib and rsigma below hard data-quality
--        thresholds in corridordefinition; otherwise, high-R is treated
--        as untrusted and cannot trigger override.[file:11]
--
--   3) Governance confirmation:
--      - For sustained overrides (beyond a single KerBlock), require a
--        GOV-band governance shard signed by a trusted DID confirming
--        the event, bound via RoH to telemetry evidence.
--
--   4) Fail-safe if sensors conflict:
--      - If high-R is detected on some sources but not corroborated:
--          * Lane governor and Paycomp MUST default to safest admissible
--            actuation (e.g., hold or reduce flow, not increase).
--          * No promotion to a more aggressive actuation lane is allowed.
--      - This is encoded as:
--          * If R_high && !multi_source_confirmed:
--                - Freeze upgrades; allow only neutral or safer actions.
--                - Log sensor conflict as a dedicated risk coordinate
--                  rconflict in the data-quality plane for audit.[file:11]
--
-- These rules keep life-safety lanes aligned with the frozen KER grammar,
-- while structurally resisting single-sensor compromise.[file:11]
