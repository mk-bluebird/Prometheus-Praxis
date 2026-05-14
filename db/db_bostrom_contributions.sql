-- filename db_bostrom_contributions.sql
-- destination Eco-Fort/db/db_bostrom_contributions.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 43. Bostrom contributions ledger schema
-------------------------------------------------------------------------------

-- Identity table (assumed to exist or to be created once in Eco-Fort)
CREATE TABLE IF NOT EXISTS eco_identity_address (
    identity_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    signingdid      TEXT NOT NULL UNIQUE,
    primary_address TEXT NOT NULL,
    alias_label     TEXT,
    created_utc     TEXT NOT NULL
);

-- Ledger of fine-grained evidence items for Bostrom identities.
CREATE TABLE IF NOT EXISTS ledger_evidence (
    evidence_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    signingdid      TEXT NOT NULL REFERENCES eco_identity_address(signingdid),
    shardid         INTEGER REFERENCES shardinstance(shardid) ON DELETE SET NULL,
    repoid          INTEGER REFERENCES repo(repoid) ON DELETE SET NULL,
    repofileid      INTEGER REFERENCES repofile(fileid) ON DELETE SET NULL,
    scope_kind      TEXT NOT NULL CHECK (scope_kind IN ('REPO','SHARD','FILE','SCHEMA','DOCUMENT')),
    scope_refid     INTEGER NOT NULL,
    k_delta         REAL,   -- contribution to K, optional
    e_delta         REAL,   -- contribution to E, optional
    r_delta         REAL,   -- contribution to R, optional
    note            TEXT,   -- human-readable rationale
    timestamputc    TEXT NOT NULL,
    rohanchorhex    TEXT    -- optional risk-of-harm anchor
);

-- Summary table aggregating contributions per identity and repo.
CREATE TABLE IF NOT EXISTS contribution_summary (
    summary_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    signingdid      TEXT NOT NULL REFERENCES eco_identity_address(signingdid),
    repoid          INTEGER NOT NULL REFERENCES repo(repoid),
    shard_count     INTEGER NOT NULL DEFAULT 0,
    kfactor         REAL NOT NULL DEFAULT 0.0,
    efactor         REAL NOT NULL DEFAULT 0.0,
    rfactor         REAL NOT NULL DEFAULT 0.0,
    rationale       TEXT,
    timestamputc    TEXT NOT NULL
);

-- View: join summaries with identity data and repo metadata.
CREATE VIEW IF NOT EXISTS v_bostrom_contribution_summary AS
SELECT
    cs.summary_id,
    cs.signingdid,
    i.primary_address,
    i.alias_label,
    cs.repoid,
    r.name          AS reponame,
    r.githubslug,
    r.roleband,
    cs.shard_count,
    cs.kfactor,
    cs.efactor,
    cs.rfactor,
    cs.rationale,
    cs.timestamputc
FROM contribution_summary cs
JOIN eco_identity_address i
  ON i.signingdid = cs.signingdid
JOIN repo r
  ON r.repoid = cs.repoid;

-- View: project knowledgeecoscore entries onto identities via ledger_evidence.
CREATE VIEW IF NOT EXISTS v_bostrom_knowledge_scores AS
SELECT
    le.signingdid,
    i.primary_address,
    ks.scopetype,
    ks.scoperefid,
    ks.kfactor,
    ks.efactor,
    ks.rfactor,
    ks.rationale,
    ks.timestamputc,
    ks.issuedby
FROM ledger_evidence le
JOIN eco_identity_address i
  ON i.signingdid = le.signingdid
JOIN knowledgeecoscore ks
  ON ks.scopetype = le.scope_kind
 AND ks.scoperefid = le.scope_refid;
