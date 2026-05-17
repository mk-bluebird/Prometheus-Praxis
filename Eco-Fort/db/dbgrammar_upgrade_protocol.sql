-- 44. Frozen grammar upgrade protocol with replay windows
-- filename: Eco-Fort/db/dbgrammar_upgrade_protocol.sql
-- destination: Eco-Fort/db/dbgrammar_upgrade_protocol.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS grammar_contract (
  contractid       INTEGER PRIMARY KEY AUTOINCREMENT,
  logicalname      TEXT NOT NULL,     -- e.g. 'EcosafetyGrammarCore'
  versiontag       TEXT NOT NULL,     -- e.g. '2026v1'
  status           TEXT NOT NULL CHECK (status IN ('FROZEN_ACTIVE','FROZEN_DEPRECATED','EXPERIMENTAL')),
  supersedesid     INTEGER REFERENCES grammar_contract(contractid),
  hashhex          TEXT NOT NULL,     -- hex hash of canonical ALN+SQL bundle
  docpath          TEXT NOT NULL,     -- path to Bible / spec doc
  registered_utc   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS grammar_replay_window (
  replayid         INTEGER PRIMARY KEY AUTOINCREMENT,
  old_contractid   INTEGER NOT NULL REFERENCES grammar_contract(contractid),
  new_contractid   INTEGER NOT NULL REFERENCES grammar_contract(contractid),
  region           TEXT NOT NULL,     -- e.g. 'Phoenix-AZ'
  tstartutc        TEXT NOT NULL,
  tendutc          TEXT NOT NULL,
  shards_tested    INTEGER NOT NULL,
  ker_upgrade_ok   INTEGER NOT NULL CHECK (ker_upgrade_ok IN (0,1)),
  vt_nonworse_ok   INTEGER NOT NULL CHECK (vt_nonworse_ok IN (0,1)),
  noncomp_ok       INTEGER NOT NULL CHECK (noncomp_ok IN (0,1)),
  registered_utc   TEXT NOT NULL
);

CREATE VIEW IF NOT EXISTS v_grammar_upgrade_ready AS
SELECT
  g_old.logicalname,
  g_old.versiontag AS old_version,
  g_new.versiontag AS new_version,
  w.region,
  MIN(w.tstartutc) AS replay_from,
  MAX(w.tendutc)   AS replay_to,
  SUM(w.shards_tested) AS total_shards
FROM grammar_replay_window AS w
JOIN grammar_contract AS g_old ON g_old.contractid = w.old_contractid
JOIN grammar_contract AS g_new ON g_new.contractid = w.new_contractid
WHERE w.ker_upgrade_ok = 1
  AND w.vt_nonworse_ok = 1
  AND w.noncomp_ok = 1
GROUP BY g_old.logicalname, g_old.versiontag, g_new.versiontag, w.region;

-- Upgrade protocol (normative, not enforced solely by SQL):
--
-- 1) Register new grammar:
--    INSERT new grammar_contract row with status='EXPERIMENTAL', supersedesid=old.[file:11]
--
-- 2) Dual-compute replay windows:
--    For each protected region (e.g., Phoenix-AZ), replay historical
--    shardinstance KER/Vt under old and new contracts over a window
--    [tstart, tend] (e.g., 6–12 months) and insert grammar_replay_window
--    rows where:
--      - ker_upgrade_ok=1 iff K_new <= K_old never and E_new >= E_old,
--        R_new <= R_old (non-weakening), respecting non-offsettable planes.
--      - vt_nonworse_ok=1 iff Vt_new >= Vt_old (admissible set shrinks or
--        stays same).
--      - noncomp_ok=1 iff all non-compensation invariants still hold.[file:11]
--
-- 3) Promotion:
--    Only when v_grammar_upgrade_ready shows coverage for all required
--    regions do we:
--      - SET old.status='FROZEN_DEPRECATED';
--      - SET new.status='FROZEN_ACTIVE'.
--
-- 4) CI enforcement:
--    - ecosafety-core and eco_wealth must consult grammar_contract where
--      status='FROZEN_ACTIVE' for KER/residual semantics.
--    - No downgrade to deprecated contracts is allowed for new PROD shards.[file:11]
