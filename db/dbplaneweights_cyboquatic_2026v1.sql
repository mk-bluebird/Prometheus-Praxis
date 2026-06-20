-- filename: db/dbplaneweights_cyboquatic_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbplaneweights_cyboquatic_2026v1.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Seed planeweights rows for Cyboquatic planes
--    Assumes planeweightscontract and planeweights exist.
----------------------------------------------------------------------

INSERT OR IGNORE INTO planeweightscontract
  (contractid, contractname, description, createdutc)
VALUES
  ('PlaneWeightsShard2026v1', 'PlaneWeightsShard2026v1',
   'Canonical plane weights for ecosafety, including Cyboquatic planes and non-offsettable flags.',
   datetime('now'));

INSERT OR IGNORE INTO planeweights
  (contractid,
   planeid,
   weight,
   nonoffsettable,
   uncertaintycap,
   band,
   createdutc)
VALUES
  (
    'PlaneWeightsShard2026v1',
    'CyboquaticSurfaceCarbon',
    0.25,
    1,
    0.05,
    'CARBON',
    datetime('now')
  ),
  (
    'PlaneWeightsShard2026v1',
    'CyboquaticHydrologyImpact',
    0.20,
    1,
    0.05,
    'HYDRO',
    datetime('now')
  ),
  (
    'PlaneWeightsShard2026v1',
    'CyboquaticRestorationRadius',
    0.20,
    1,
    0.05,
    'RESTORATION',
    datetime('now')
  );
