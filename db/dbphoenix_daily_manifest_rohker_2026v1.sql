-- filename: db/dbphoenix_daily_manifest_rohker_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbphoenix_daily_manifest_rohker_2026v1.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Extend Phoenix daily state with MT6883 flags (if not already)
--    This block assumes stewarddailystatephx is created as in prior work.
--    If the extra columns already exist, these ALTERs will be skipped.
----------------------------------------------------------------------

ALTER TABLE stewarddailystatephx
  ADD COLUMN mt6883okday INTEGER NOT NULL DEFAULT 1;

ALTER TABLE stewarddailystatephx
  ADD COLUMN neuroethicokday INTEGER NOT NULL DEFAULT 1;

CREATE INDEX IF NOT EXISTS idx_stewarddailystatephx_mt6883
  ON stewarddailystatephx (mt6883okday, neuroethicokday);

----------------------------------------------------------------------
-- 2. MT6883 daily RoH aggregation per steward and region
--    Assumes a mapping from nodeid -> stewarddid exists (e.g. nodeowner)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mt6883dailyrohphx (
  rohdayid          INTEGER PRIMARY KEY AUTOINCREMENT,
  stewarddid        TEXT    NOT NULL,
  regioncode        TEXT    NOT NULL,
  dayutc            TEXT    NOT NULL,   -- YYYY-MM-DD
  rohmaxday         REAL    NOT NULL,
  rohkerneldid      TEXT    NOT NULL,
  rohceilingday     REAL    NOT NULL,
  rohokday          INTEGER NOT NULL,   -- 1 if rohmaxday <= rohceilingday
  evidencehex       TEXT    NOT NULL,
  signingdid        TEXT    NOT NULL,
  createdutc        TEXT    NOT NULL,
  CHECK (rohmaxday >= 0.0 AND rohmaxday <= 1.0),
  CHECK (rohceilingday >= 0.0 AND rohceilingday <= 1.0),
  CHECK (rohokday IN (0,1)),
  CHECK (length(evidencehex) > 0),
  CHECK (length(signingdid) > 0),
  UNIQUE (stewarddid, regioncode, dayutc)
);

CREATE INDEX IF NOT EXISTS idx_mt6883dailyrohphx_flags
  ON mt6883dailyrohphx (rohokday);

----------------------------------------------------------------------
-- 3. Unified Phoenix evolution manifest, including RoH
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphoenixdailyevolutionfull AS
SELECT
  d.stewarddid,
  d.regioncode,
  d.dayutc,
  d.kmeanday,
  d.emeanday,
  d.rmeanday,
  d.vtmaxday,
  d.ecounitissuedday,
  d.rohmaxday,
  d.rohok,
  d.rresponsibilityday,
  d.rportfoliodiversityday,
  d.rtopologyday,
  d.governancepenaltyday,
  d.dataqualityfactorday,
  d.lifeforcedeltaday,
  d.biokarmadeltaday,
  d.representationfloor,
  d.actualecowealthshare,
  d.representationok,
  d.kerdeployableday,
  d.lyapunovokday,
  d.mt6883okday,
  d.neuroethicokday
FROM
  stewarddailystatephx AS d;

CREATE VIEW IF NOT EXISTS vphoenixdailyevolutionadmissible AS
SELECT
  f.*
FROM
  vphoenixdailyevolutionfull AS f
WHERE
  f.rohok = 1
  AND f.kerdeployableday = 1
  AND f.lyapunovokday = 1
  AND f.mt6883okday = 1
  AND f.neuroethicokday = 1;
