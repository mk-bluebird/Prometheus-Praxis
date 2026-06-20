-- filename: db/dbcyboquatic_ker_window_planes_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbcyboquatic_ker_window_planes_2026v1.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic per-plane KER window view
--    Specialization of ecosafetywindowplane for Cyboquatic planes/nodes.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboquaticwindowwithplanes AS
SELECT
  p.nodeid,
  p.windowstartutc,
  p.windowendutc,
  p.planeid,
  p.kplane,
  p.eplane,
  p.rplane,
  p.vtplane,
  p.deltarplane,
  p.planecontractid,
  w.kscore    AS kscore_node,
  w.escore    AS escore_node,
  w.rscore    AS rscore_node,
  w.vtmax     AS vtmax_node
FROM
  ecosafetywindowplane AS p
JOIN
  ecosafetywindows AS w
    ON w.nodeid        = p.nodeid
   AND w.windowstartutc = p.windowstartutc
   AND w.windowendutc   = p.windowendutc
WHERE
  p.planeid IN (
    'CyboquaticSurfaceCarbon',
    'CyboquaticHydrologyImpact',
    'CyboquaticRestorationRadius'
  );
