-- filename: dbvirtual_security_backup.sql
-- destination: ecorestorationshard/db/dbvirtual_security_backup.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS virtual_security_backup (
    backup_id          INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Linkage into artifact/identity spine (no raw identity payloads)
    artifact_id        INTEGER NOT NULL,
    address_id         INTEGER NOT NULL,

    -- Snapshot semantics
    snapshot_logical   TEXT    NOT NULL,
    snapshot_region    TEXT    NOT NULL,
    snapshot_scope     TEXT    NOT NULL,   -- e.g. REGION, NODE, CONSTELLATION
    snapshot_role      TEXT    NOT NULL,   -- e.g. ECO_MEMORY, HEALTHCARE, GOV_ID

    -- Merkle commitments (no PII)
    merkle_root_hex    TEXT    NOT NULL,   -- hex-encoded Merkle root of the snapshot
    merkle_depth       INTEGER NOT NULL,   -- tree depth, for verifier expectations
    merkle_scheme      TEXT    NOT NULL,   -- e.g. "BINARY_TREE_KNOWN_SCHEME"

    -- KER / governance context at backup time
    lane               TEXT    NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD')),
    ker_band           TEXT    NOT NULL CHECK (ker_band IN ('SAFE','GUARDED','BLOCKED')),
    k_metric           REAL,
    e_metric           REAL,
    r_metric           REAL,
    vt_max             REAL,

    -- Evidence and external anchors
    evidence_hex       TEXT    NOT NULL,
    roh_anchor_hex     TEXT,
    signing_did        TEXT    NOT NULL,
    onchain_ref        TEXT,              -- optional external anchor (tx id, URL)

    created_utc        TEXT    NOT NULL,
    updated_utc        TEXT    NOT NULL,

    active             INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),

    FOREIGN KEY (artifact_id)
        REFERENCES artifactregistry(artifactid)
        ON DELETE CASCADE,

    FOREIGN KEY (address_id)
        REFERENCES bostromaddress(addressid)
        ON DELETE CASCADE,

    UNIQUE (artifact_id, snapshot_logical, merkle_root_hex)
);

CREATE INDEX IF NOT EXISTS idx_vsb_artifact
    ON virtual_security_backup (artifact_id, active);

CREATE INDEX IF NOT EXISTS idx_vsb_address
    ON virtual_security_backup (address_id, active);

CREATE INDEX IF NOT EXISTS idx_vsb_region_scope
    ON virtual_security_backup (snapshot_region, snapshot_scope, lane, ker_band);
