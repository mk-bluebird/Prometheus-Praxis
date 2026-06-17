-- filename db_trust_diagnostics.sql
-- destination eco_restoration_shard/sql/shards/db_trust_diagnostics.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS trust_diagnostics (
    diagid        INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid        TEXT NOT NULL,
    twindowstart  TEXT NOT NULL,
    twindowend    TEXT NOT NULL,
    drift_norm    REAL NOT NULL,   -- 0..1
    var_norm      REAL NOT NULL,   -- 0..1
    resid_norm    REAL NOT NULL,   -- 0..1
    n_violations  INTEGER NOT NULL,
    dt            REAL NOT NULL,   -- TrustDisplacementKernel result
    evidencehex   TEXT NOT NULL,
    createdutc    TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_trust_node_time
    ON trust_diagnostics(nodeid, twindowend);
