-- filename db/dblanelyapunovker.sql

PRAGMA foreign_keys = ON;

-- 1. Core residual view: vshardresidual

CREATE VIEW IF NOT EXISTS vshardresidual AS
WITH activekernel AS (
    SELECT
        rk.kernelid,
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
FROM shardinstance  AS si
JOIN activekernel   AS ak
  ON ak.planecontractid = si.planecontractid
JOIN residualterm   AS rt
  ON rt.kernelid = ak.kernelid
JOIN shardriskcoord AS rc
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

-- 2. Residual with topology: vshardtopologyker

CREATE VIEW IF NOT EXISTS vshardtopologyker AS
WITH topoweight AS (
    SELECT
        pwp.contractid,
        pwp.weight AS wtopology
    FROM planeweightsplane AS pwp
    WHERE pwp.planecode = 'TOPOLOGY'
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
    vr.vtvalue
      + COALESCE(tw.wtopology, 0.0) * tr.rtopology * tr.rtopology
      AS vtwithtopology
FROM vshardresidual      AS vr
LEFT JOIN topologyriskmetric AS tr
       ON tr.shardid = vr.shardid
LEFT JOIN topoweight     AS tw
       ON tw.contractid = vr.planecontractid;

CREATE INDEX IF NOT EXISTS idx_vshardtopologyker_shardid_time
    ON vshardtopologyker (shardid, tsendutc);

-- 3. Consolidated KER view: vshardker

CREATE VIEW IF NOT EXISTS vshardker AS
SELECT
    si.shardid,
    si.tsstartutc,
    si.tsendutc,
    vk.residualkernelcode,
    vk.planecontractid,
    vk.vtcore,
    vk.itopology,
    vk.rtopology,
    vk.wtopology,
    vk.vtwithtopology,
    si.kmetric AS kvalue,
    si.emetric AS evalue,
    si.rmetric AS rvalue,
    si.lane,
    si.kerdeployable
FROM shardinstance     AS si
JOIN vshardtopologyker AS vk
  ON vk.shardid = si.shardid
 AND vk.tsendutc = si.tsendutc;

CREATE INDEX IF NOT EXISTS idx_vshardker_lane_time
    ON vshardker (lane, tsendutc);

-- 4. Lyapunov violation view: vshardkerviolation

CREATE TABLE IF NOT EXISTS safestepconfig (
    planecontractid INTEGER PRIMARY KEY,
    epsresidual     REAL NOT NULL DEFAULT 1e-6
);

CREATE VIEW IF NOT EXISTS vshardkerviolation AS
WITH ordered AS (
    SELECT
        k.*,
        LAG(k.vtwithtopology)
          OVER (PARTITION BY k.shardid ORDER BY k.tsendutc)
          AS vtprev
    FROM vshardker AS k
),
violations AS (
    SELECT
        o.shardid,
        o.tsstartutc,
        o.tsendutc,
        o.vtwithtopology AS vtcurrent,
        o.vtprev         AS vtprevious,
        (o.vtwithtopology - o.vtprev) AS deltavt,
        sc.epsresidual,
        CASE
          WHEN o.vtprev IS NULL THEN 0
          WHEN o.vtwithtopology <= o.vtprev + sc.epsresidual THEN 0
          ELSE 1
        END AS violateslyapunov
    FROM ordered AS o
    JOIN safestepconfig AS sc
      ON sc.planecontractid = o.planecontractid
)
SELECT
    shardid,
    tsstartutc,
    tsendutc,
    vtcurrent,
    vtprevious,
    deltavt,
    epsresidual,
    violateslyapunov
FROM violations;

CREATE INDEX IF NOT EXISTS idx_vshardkerviolation_flag
    ON vshardkerviolation (violateslyapunov);
