-- filename: db/dbrohkernel_mt6883_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbrohkernel_mt6883_2026v1.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. MT6883 neuro capability catalog with RoH ceilings
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS neurocapability (
  neurocapabilityid   INTEGER PRIMARY KEY AUTOINCREMENT,
  capabilitycode      TEXT    NOT NULL UNIQUE,  -- e.g. MT6883.DETOX.COURSE.V1
  description         TEXT    NOT NULL,
  rohceiling          REAL    NOT NULL,         -- hard ceiling 0.0..1.0, e.g. 0.30
  rohkernelcode       TEXT    NOT NULL,         -- binds to RoHKernel definition
  lanehint            TEXT    NOT NULL,         -- e.g. HEALTHCARE, RESEARCH
  active              INTEGER NOT NULL DEFAULT 1,
  createdutc          TEXT    NOT NULL,
  updatedutc          TEXT    NOT NULL,
  CHECK (rohceiling >= 0.0 AND rohceiling <= 1.0),
  CHECK (active IN (0,1))
);

CREATE INDEX IF NOT EXISTS idx_neurocapability_active
  ON neurocapability (active);

----------------------------------------------------------------------
-- 2. RoH per-node, per-capability kernel surface
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS rohkernelmt6883 (
  rohkernelid         INTEGER PRIMARY KEY AUTOINCREMENT,
  nodeid              TEXT    NOT NULL,
  regioncode          TEXT    NOT NULL,
  neurocapabilityid   INTEGER NOT NULL,
  rohvalue            REAL    NOT NULL,        -- observed or inferred RoH in 0..1
  rohceiling          REAL    NOT NULL,        -- ceiling applied for this entry
  rohceilingok        INTEGER NOT NULL,        -- 1 if rohvalue <= rohceiling
  evidencehex         TEXT    NOT NULL,
  signingdid          TEXT    NOT NULL,
  tswindowstartutc    TEXT    NOT NULL,
  tswindowendutc      TEXT    NOT NULL,
  createdutc          TEXT    NOT NULL,
  CHECK (rohvalue >= 0.0 AND rohvalue <= 1.0),
  CHECK (rohceiling >= 0.0 AND rohceiling <= 1.0),
  CHECK (rohceilingok IN (0,1)),
  CHECK (length(evidencehex) > 0),
  CHECK (length(signingdid) > 0),
  FOREIGN KEY (neurocapabilityid) REFERENCES neurocapability(neurocapabilityid)
    ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_rohkernelmt6883_node_time
  ON rohkernelmt6883 (nodeid, tswindowstartutc, tswindowendutc);

CREATE INDEX IF NOT EXISTS idx_rohkernelmt6883_capability
  ON rohkernelmt6883 (neurocapabilityid, regioncode);

----------------------------------------------------------------------
-- 3. View: RoHKernel with capability metadata
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vrohkernelmt6883 AS
SELECT
  r.rohkernelid,
  r.nodeid,
  r.regioncode,
  c.capabilitycode,
  c.description AS capabilitydescription,
  c.rohceiling  AS capabilityrohceiling,
  r.rohvalue,
  r.rohceiling,
  r.rohceilingok,
  r.tswindowstartutc,
  r.tswindowendutc,
  r.evidencehex,
  r.signingdid
FROM
  rohkernelmt6883 AS r
JOIN
  neurocapability AS c
    ON c.neurocapabilityid = r.neurocapabilityid
WHERE
  c.active = 1;
