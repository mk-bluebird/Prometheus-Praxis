-- filename: dbeco_wealth_ledger.sql
-- destination: Eco-Fort/db/dbeco_wealth_ledger.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS eco_wealth_ledger (
    tx_id            INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Identity & classification
    scope            TEXT NOT NULL,  -- ACCOUNT, REGION, CONSTELLATION
    scope_id         TEXT NOT NULL,  -- DID, region code, etc.
    tx_type          TEXT NOT NULL CHECK (tx_type IN ('MINT','TRANSFER','DEMURRAGE')),
    asset_id         TEXT NOT NULL,  -- e.g. 'ECO-WEALTH-BASE-2026v1'
    unit_symbol      TEXT NOT NULL,  -- e.g. 'ECO'

    -- Amounts (signed for debits/credits)
    amount           REAL NOT NULL,  -- positive for credit, negative for debit
    amount_abs       REAL NOT NULL,  -- ABS(amount), for analytics
    plane_carbon     REAL,           -- optional per-plane decomposition
    plane_biodiv     REAL,
    plane_water      REAL,
    plane_materials  REAL,

    -- Transaction context
    from_scope       TEXT,           -- for transfers (scope/type/id triples)
    from_scope_id    TEXT,
    to_scope         TEXT,
    to_scope_id      TEXT,
    reason           TEXT NOT NULL,  -- enum in ALN: REWARD, GENESIS, DEMURRAGE, REALLOC, CORRECTION

    -- KER at transaction time (windowed snapshot)
    ker_context      TEXT NOT NULL,  -- e.g. 'PhoenixEcoSafetySpine2026v1'
    k_at_tx          REAL NOT NULL,
    e_at_tx          REAL NOT NULL,
    r_at_tx          REAL NOT NULL,
    vtmax_window     REAL NOT NULL,  -- max Vt over the scoring window
    lane_at_tx       TEXT NOT NULL,  -- RESEARCH, PILOT, PROD

    -- Provenance links
    shard_id         INTEGER NOT NULL
                         REFERENCES shardinstance(shardid) ON DELETE RESTRICT,
    t03_shard_path   TEXT,           -- optional convenience pointer
    t10_shard_path   TEXT,
    t12_batch_id     TEXT,           -- batch or settlement group id

    evidencehex      TEXT NOT NULL,  -- hash over the evidence bundle
    alnspechash      TEXT NOT NULL,  -- spec for KER and wealth policy
    signing_did      TEXT NOT NULL,  -- steward or governance DID
    cosigner_did     TEXT,           -- optional second signer for high-value txs

    created_utc      TEXT NOT NULL,  -- ISO-8601
    logical_epoch    INTEGER NOT NULL,  -- monotone epoch / block height
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_eco_wealth_scope_time
    ON eco_wealth_ledger(scope, scope_id, created_utc);

CREATE INDEX IF NOT EXISTS idx_eco_wealth_tx_type
    ON eco_wealth_ledger(tx_type, reason);

CREATE INDEX IF NOT EXISTS idx_eco_wealth_shard
    ON eco_wealth_ledger(shard_id);

CREATE INDEX IF NOT EXISTS idx_eco_wealth_epoch
    ON eco_wealth_ledger(logical_epoch);
