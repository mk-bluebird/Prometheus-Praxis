-- ecosafety-grammar-core/sql/db_plane_weights.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS planeweights (
    weight_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    policy_id        TEXT NOT NULL,
    plane_id         TEXT NOT NULL,
    w_plane          REAL NOT NULL,
    non_offsettable  INTEGER NOT NULL CHECK (non_offsettable IN (0,1)),
    r_unc_max_prod   REAL NOT NULL,
    r_safe           REAL NOT NULL,
    r_gold           REAL NOT NULL,
    r_hard           REAL NOT NULL,
    lyap_channel     TEXT NOT NULL,
    version_tag      TEXT,
    signing_did      TEXT,
    evidence_hex     TEXT
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_planeweights_policy_plane
    ON planeweights(policy_id, plane_id);
