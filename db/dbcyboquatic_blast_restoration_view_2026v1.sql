-- filename: db/dbcyboquatic_blast_restoration_view_2026v1.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination: Eco-Fort/db/dbcyboquatic_blast_restoration_view_2026v1.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic blast-radius + restoration adjacency view
--    Requires vblastradiusadjacent and CyboquaticRestorationSurface.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcyboquaticblastradiusrestoration AS
SELECT
  c.nodeid              AS centernodeid,
  c.basinid             AS centerbasinid,
  c.region              AS centerregion,
  c.restorationradiusm  AS centerrestorationradiusm,
  c.restorationok       AS centerrestorationok,
  a.graphid,
  a.nodeid              AS sourcenodeid,
  a.neighbornodeid      AS neighbornodeid,
  a.distancem,
  a.latencys,
  a.neighborplaneid     AS neighborplaneid,
  a.neighborradiusm,
  a.neighborradiustimes,
  a.neighborkerband,
  a.distanceweightedreach,
  cn.restorationok      AS neighborrestorationok
FROM
  CyboquaticRestorationSurface AS c
JOIN
  vblastradiusadjacent AS a
    ON a.nodeid = c.nodeid
LEFT JOIN
  CyboquaticRestorationSurface AS cn
    ON cn.nodeid          = a.neighbornodeid
   AND cn.region          = c.region
   AND cn.tswindowendutc  = c.tswindowendutc;
