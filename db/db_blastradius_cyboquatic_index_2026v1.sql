-- db/db_blastradius_cyboquatic_index_2026v1.sql
-- Cross-repo Cyboquatic blast-radius index and adjacency overlay, designed to
-- plug into the shared blastradiusindex and adjacencygraph patterns.[file:6][file:9]

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic blast radius extension
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blastradius_cyboquatic (
    shardid            TEXT    NOT NULL,
    nodeid             TEXT    NOT NULL,
    region             TEXT    NOT NULL,
    radiusmeters       REAL    NOT NULL,
    adjacencycount     INTEGER NOT NULL,
    restorationradiusm REAL    NOT NULL,
    carbonnegativeok   INTEGER NOT NULL CHECK (carbonnegativeok IN (0,1)),
    restorationok      INTEGER NOT NULL CHECK (restorationok IN (0,1)),
    created_utc        INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    PRIMARY KEY (shardid, nodeid)
);

CREATE INDEX IF NOT EXISTS idx_blast_cybo_region
    ON blastradius_cyboquatic(region, radiusmeters);

----------------------------------------------------------------------
-- 2. Cyboquatic adjacency view backed by generic adjacencygraph
--    (assumes adjacencygraph exists in the governance spine DB).[file:6][file:9]
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboquatic_blastradius AS
SELECT
    b.shardid,
    b.nodeid,
    b.region,
    b.radiusmeters,
    b.adjacencycount,
    b.restorationradiusm,
    b.carbonnegativeok,
    b.restorationok
FROM blastradius_cyboquatic AS b;

----------------------------------------------------------------------
-- 3. Agent-facing overlay combining KER, lane, and blast radius
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboquatic_lane_ker_blast AS
SELECT
    ls.shardid,
    ls.region,
    ls.lane,
    ls.verdict,
    ls.kerk,
    ls.kere,
    ls.kerr,
    ls.residualvt,
    br.radiusmeters,
    br.adjacencycount,
    br.restorationradiusm,
    br.carbonnegativeok,
    br.restorationok
FROM vlaneadmissibility AS ls
JOIN vcyboquatic_blastradius AS br
  ON ls.shardid = br.shardid;
