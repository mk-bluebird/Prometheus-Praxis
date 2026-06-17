-- filename: db_guard_spine_core.sql
-- destination: eco_restoration_shard/db/db_guard_spine_core.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
--
-- SQLite schema to support the Rust guard spine:
-- - monotone_state: forward-only state progression.
-- - eco_state_prev / eco_state_next: invariant comparison snapshots.
-- - ker_state_prev / ker_state_next: K, E, R monotone checks.
-- - eco_transaction_ledger: ecological, karma, thermo-aware transactions.
--

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS monotone_state (
    entity_id    TEXT    NOT NULL PRIMARY KEY,
    state_seq    INTEGER NOT NULL,
    updated_utc  TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_monotone_state_updated
    ON monotone_state (updated_utc);

CREATE TABLE IF NOT EXISTS eco_state_prev (
    entity_id    TEXT    NOT NULL PRIMARY KEY,
    vt_residual  REAL    NOT NULL,
    eco_balance  REAL    NOT NULL,
    updated_utc  TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS eco_state_next (
    entity_id    TEXT    NOT NULL PRIMARY KEY,
    vt_residual  REAL    NOT NULL,
    eco_balance  REAL    NOT NULL,
    updated_utc  TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS ker_state_prev (
    entity_id    TEXT    NOT NULL PRIMARY KEY,
    k_factor     REAL    NOT NULL,
    e_factor     REAL    NOT NULL,
    r_factor     REAL    NOT NULL,
    updated_utc  TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS ker_state_next (
    entity_id    TEXT    NOT NULL PRIMARY KEY,
    k_factor     REAL    NOT NULL,
    e_factor     REAL    NOT NULL,
    r_factor     REAL    NOT NULL,
    updated_utc  TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS eco_transaction_ledger (
    tx_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_id      TEXT    NOT NULL,
    applied_utc    TEXT    NOT NULL,
    eco_delta      REAL    NOT NULL,
    roh_after      REAL    NOT NULL,
    vt_after       REAL    NOT NULL,
    thermo_budget  REAL    NOT NULL,
    karma_delta    REAL    NOT NULL,
    CHECK (roh_after >= 0.0 AND roh_after <= 1.0),
    CHECK (thermo_budget >= 0.0 AND thermo_budget <= 1.0)
);

CREATE INDEX IF NOT EXISTS idx_eco_tx_entity_time
    ON eco_transaction_ledger (entity_id, applied_utc);

CREATE VIEW IF NOT EXISTS v_eco_tx_budget_summary AS
SELECT
    entity_id,
    COUNT(*)                      AS tx_count,
    SUM(eco_delta)                AS eco_delta_sum,
    MIN(thermo_budget)            AS thermo_budget_min,
    MAX(roh_after)                AS roh_after_max,
    SUM(karma_delta)              AS karma_delta_sum
FROM eco_transaction_ledger
GROUP BY entity_id;
