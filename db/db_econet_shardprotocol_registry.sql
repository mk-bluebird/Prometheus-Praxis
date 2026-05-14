-- filename db_econet_shardprotocol_registry.sql
-- destination Eco-Fort/db/db_econet_shardprotocol_registry.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Registry of valid shardprotocol identifiers for econetrepoindex.shardprotocol
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS econet_shardprotocol (
    protocol_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    protocol_name TEXT NOT NULL UNIQUE,
    description   TEXT NOT NULL,
    version_tag   TEXT NOT NULL,
    category      TEXT NOT NULL, -- SCHEMA, INDEX, GOVERNANCE, LANE_STATUS, PLANE_WEIGHTS
    active        INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc   TEXT NOT NULL,
    updated_utc   TEXT NOT NULL
);

INSERT OR IGNORE INTO econet_shardprotocol
    (protocol_name, description, version_tag, category, active, created_utc, updated_utc)
VALUES
    ('EcoNetSchemaShard2026v1',
     'Default EcoNet ALN-CSV schema shard for repo discovery and KER indexing.',
     '2026v1',
     'SCHEMA',
     1,
     '2026-01-01T00:00:00Z',
     '2026-01-01T00:00:00Z'),
    ('EcoNetRepoIndexShard2026v1',
     'Master-index shard schema for econetrepoindex and econetlayer tables.',
     '2026v1',
     'INDEX',
     1,
     '2026-01-01T00:00:00Z',
     '2026-01-01T00:00:00Z'),
    ('LaneStatusShard2026v1',
     'Lane governance status shard schema for lanestatusshard rows.',
     '2026v1',
     'LANE_STATUS',
     1,
     '2026-01-01T00:00:00Z',
     '2026-01-01T00:00:00Z'),
    ('PlaneWeightsShard2026v1',
     'Plane weights and non-offsettable plane contract for Lyapunov residual.',
     '2026v1',
     'PLANE_WEIGHTS',
     1,
     '2026-01-01T00:00:00Z',
     '2026-01-01T00:00:00Z');

-------------------------------------------------------------------------------
-- 2. Enforce that econetrepoindex.shardprotocol uses only registered protocols
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS econetrepoindex (
    reponame         TEXT PRIMARY KEY,
    githubslug       TEXT NOT NULL,
    roleband         TEXT NOT NULL,
    visibility       TEXT NOT NULL,
    languageprimary  TEXT NOT NULL,
    description      TEXT,
    ecosafetybinding TEXT NOT NULL,
    shardprotocol    TEXT NOT NULL,
    lanedefault      TEXT NOT NULL,
    kertargetk       REAL NOT NULL,
    kertargete       REAL NOT NULL,
    kertargetr       REAL NOT NULL,
    nonactuatingonly INTEGER NOT NULL CHECK (nonactuatingonly IN (0,1))
);

CREATE TRIGGER IF NOT EXISTS trg_econetrepoindex_shardprotocol_ins
BEFORE INSERT ON econetrepoindex
BEGIN
    SELECT
        CASE
            WHEN NOT EXISTS (
                SELECT 1
                FROM econet_shardprotocol
                WHERE protocol_name = NEW.shardprotocol
                  AND active = 1
            )
            THEN
                RAISE(ABORT, 'Invalid shardprotocol: not registered in econet_shardprotocol.')
        END;
END;

CREATE TRIGGER IF NOT EXISTS trg_econetrepoindex_shardprotocol_upd
BEFORE UPDATE OF shardprotocol ON econetrepoindex
BEGIN
    SELECT
        CASE
            WHEN NOT EXISTS (
                SELECT 1
                FROM econet_shardprotocol
                WHERE protocol_name = NEW.shardprotocol
                  AND active = 1
            )
            THEN
                RAISE(ABORT, 'Invalid shardprotocol: not registered in econet_shardprotocol.')
        END;
END;
