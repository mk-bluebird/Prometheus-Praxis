-- filename: db_contribution_graph.sql
-- destination: ecorestorationshard/db/db_contribution_graph.sql

PRAGMA foreign_keys = ON;

-- Base table for contributors (Bostrom addresses).
CREATE TABLE IF NOT EXISTS eco_contributor (
    contributor_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    contributor_address TEXT    NOT NULL UNIQUE,
    label               TEXT    NOT NULL DEFAULT 'CONTRIBUTOR',
    created_utc         TEXT    NOT NULL,
    updated_utc         TEXT    NOT NULL
);

-- Aggregate contribution count per contributor.
CREATE TABLE IF NOT EXISTS eco_contributor_stats (
    contributor_id      INTEGER PRIMARY KEY,
    contribution_count  INTEGER NOT NULL DEFAULT 0 CHECK(contribution_count >= 0),
    last_contribution_utc TEXT  NOT NULL,
    FOREIGN KEY (contributor_id) REFERENCES eco_contributor(contributor_id)
        ON DELETE CASCADE
);

-- Detailed contribution record keyed by (contributor_address | artifact_cid).
CREATE TABLE IF NOT EXISTS eco_contribution_record (
    contribution_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    contributor_id      INTEGER NOT NULL,
    artifact_cid        TEXT    NOT NULL,
    artifact_type       TEXT    NOT NULL,
    timestamp_utc       TEXT    NOT NULL,
    tx_hash             TEXT    NOT NULL,
    msg_type            TEXT    NOT NULL,
    region              TEXT    NOT NULL,
    eco_scope           TEXT    NOT NULL,
    evidence_cid        TEXT,
    karmadelta          REAL,
    r_axis_delta        REAL,
    UNIQUE (contributor_id, artifact_cid),
    FOREIGN KEY (contributor_id) REFERENCES eco_contributor(contributor_id)
        ON DELETE CASCADE
);

-- Secondary index for artifact_type queries.
CREATE INDEX IF NOT EXISTS idx_eco_contribution_artifact_type
    ON eco_contribution_record (artifact_type, timestamp_utc);

-- Secondary index for timestamp queries (per contributor dashboard).
CREATE INDEX IF NOT EXISTS idx_eco_contribution_timestamp
    ON eco_contribution_record (contributor_id, timestamp_utc);

-- Convenience view mapping contributor_address → contribution_count.
CREATE VIEW IF NOT EXISTS v_eco_contributor_counts AS
SELECT
    c.contributor_address,
    s.contribution_count,
    s.last_contribution_utc
FROM eco_contributor AS c
JOIN eco_contributor_stats AS s
    ON c.contributor_id = s.contributor_id;

-- Convenience view for (contributor_address | artifact_cid) → record.
CREATE VIEW IF NOT EXISTS v_eco_contribution_graph AS
SELECT
    c.contributor_address,
    r.artifact_cid,
    r.artifact_type,
    r.timestamp_utc,
    r.tx_hash,
    r.msg_type,
    r.region,
    r.eco_scope,
    r.evidence_cid,
    r.karmadelta,
    r.r_axis_delta
FROM eco_contribution_record AS r
JOIN eco_contributor AS c
    ON r.contributor_id = c.contributor_id;
