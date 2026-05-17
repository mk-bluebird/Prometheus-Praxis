-- filename: eco_restoration_shard/db/db_lanelyapunovker.sql
PRAGMA foreign_keys = ON;

-- Core residual V_t per shard window.
CREATE VIEW IF NOT EXISTS vshardresidual AS
WITH activekernel AS (
    SELECT rk.kernelid,
           rk.kernelcode,
           rk.planecontractid
    FROM residualkernel AS rk
)
SELECT
    si.shardid,
    si.tsstartutc AS tsstartutc,
    si.tsendutc   AS tsendutc,
    ak.kernelcode AS residualkernelcode,
    ak.planecontractid,
    SUM(rt.alpha * rc.rvalue * rc.rvalue) AS vtvalue
FROM shardinstance    AS si
JOIN activekernel     AS ak
  ON ak.planecontractid = si.planecontractid
JOIN residualterm     AS rt
  ON rt.kernelid = ak.kernelid
JOIN shardriskcoord   AS rc
  ON rc.shardid = si.shardid
 AND rc.coordid = rt.coordid
GROUP BY
    si.shardid,
    si.tsstartutc,
    si.tsendutc,
    ak.kernelcode,
    ak.planecontractid;

CREATE INDEX IF NOT EXISTS idx_vshardresidual_shardid_time
    ON vshardresidual (shardid, tsendutc);

-- Topology and representation penalty overlay.
CREATE VIEW IF NOT EXISTS vshardtopologyker AS
WITH topoweight AS (
    SELECT pwp.contractid,
           pwp.weight AS wtopology
    FROM   planeweightsplane AS pwp
    WHERE  pwp.planecode = 'TOPOLOGY'
)
SELECT
    vr.shardid,
    vr.tsstartutc,
    vr.tsendutc,
    vr.residualkernelcode,
    vr.planecontractid,
    vr.vtvalue AS vtcore,
    tr.itopology,
    tr.rtopology,
    COALESCE(tw.wtopology, 0.0) AS wtopology,
    vr.vtvalue + COALESCE(tw.wtopology, 0.0) * tr.rtopology * tr.rtopology
      AS vtwithtopology
FROM vshardresidual      AS vr
LEFT JOIN topologyriskmetric AS tr
       ON tr.shardid = vr.shardid
LEFT JOIN topoweight     AS tw
       ON tw.contractid = vr.planecontractid;

CREATE INDEX IF NOT EXISTS idx_vshardtopologyker_shardid_time
    ON vshardtopologyker (shardid, tsendutc);

-- ResponsibilityAxis and portfolio diversity overlays.
CREATE VIEW IF NOT EXISTS vshardresponsibilityker AS
WITH respweight AS (
    SELECT pwp.contractid,
           pwp.weight AS wresp
    FROM   planeweightsplane AS pwp
    WHERE  pwp.planecode = 'RESPONSIBILITY'
),
divweight AS (
    SELECT pwp.contractid,
           pwp.weight AS wdiv
    FROM   planeweightsplane AS pwp
    WHERE  pwp.planecode = 'PORTFOLIO_DIVERSITY'
)
SELECT
    vk.shardid,
    vk.tsstartutc,
    vk.tsendutc,
    vk.residualkernelcode,
    vk.planecontractid,
    vk.vtcore,
    vk.itopology,
    vk.rtopology,
    vk.wtopology,
    COALESCE(rr.rresponsibility, 0.0) AS rresponsibility,
    COALESCE(rp.rportfoliodiversity, 0.0) AS rportfoliodiversity,
    COALESCE(rw.wresp, 0.0) AS wresp,
    COALESCE(dw.wdiv, 0.0) AS wdiv,
    vk.vtwithtopology
      + COALESCE(rw.wresp, 0.0) * COALESCE(rr.rresponsibility, 0.0) * COALESCE(rr.rresponsibility, 0.0)
      + COALESCE(dw.wdiv, 0.0) * COALESCE(rp.rportfoliodiversity, 0.0) * COALESCE(rp.rportfoliodiversity, 0.0)
      AS vtwithresponsibility
FROM vshardtopologyker AS vk
LEFT JOIN responsibilitymetric AS rr
       ON rr.shardid = vk.shardid
LEFT JOIN portfoliodiversitymetric AS rp
       ON rp.shardid = vk.shardid
LEFT JOIN respweight AS rw
       ON rw.contractid = vk.planecontractid
LEFT JOIN divweight AS dw
       ON dw.contractid = vk.planecontractid;

CREATE INDEX IF NOT EXISTS idx_vshardresponsibilityker_shardid_time
    ON vshardresponsibilityker (shardid, tsendutc);

-- Final KER view: K/E/R plus residual and overlays per shard window.
CREATE VIEW IF NOT EXISTS vshardker AS
SELECT
    si.shardid,
    si.tsstartutc,
    si.tsendutc,
    vrk.residualkernelcode,
    vrk.planecontractid,
    vrk.vtcore,
    vrk.itopology,
    vrk.rtopology,
    vrk.wtopology,
    vrk.rresponsibility,
    vrk.rportfoliodiversity,
    vrk.wresp,
    vrk.wdiv,
    vrk.vtwithresponsibility AS vtwithoverlays,
    si.kmetric AS kvalue,
    si.emetric AS evalue,
    si.rmetric AS rvalue,
    si.lane,
    si.kerdeployable
FROM shardinstance           AS si
JOIN vshardresponsibilityker AS vrk
  ON vrk.shardid = si.shardid
 AND vrk.tsendutc = si.tsendutc;

CREATE INDEX IF NOT EXISTS idx_vshardker_lane_time
    ON vshardker (lane, tsendutc);
