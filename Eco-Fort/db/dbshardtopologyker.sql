-- 17. vshardtopologyker schema
-- filename: Eco-Fort/db/dbshardtopologyker.sql
-- destination: Eco-Fort/db/dbshardtopologyker.sql

PRAGMA foreign_keys = ON;

-- Canonical view: shardid, Vt_core, r_topology, w_topology, Vt_final, topology_band, topologyok.
-- Assumptions (as in ecosafetygrammarcore + lane/topology DRs):
--   - vshardresidual(shardid, tsstartutc, tsendutc, residualkernelcode, planecontractid, vtvalue)
--   - topologyriskmetric(shardid, itopology, rtopology)
--   - planeweights_plane(contractid, planeid, planecode, weight, nonoffsettable)
--   - corridordefinition(varid, coordcode, safe_band, gold_band, hard_band, lyapchannel)
--   - riskcoordinate(coordid, coordcode, lyapchannel) where coordcode='R_TOPOLOGY' for r_topology

CREATE VIEW IF NOT EXISTS vshardtopologyker AS
WITH
-- 1) Topology plane weight per planecontract
topoweight AS (
    SELECT
        pw.contractid      AS planecontractid,
        pw.weight          AS w_topology
    FROM planeweights_plane AS pw
    WHERE pw.planecode = 'TOPOLOGY'
),
-- 2) Topology corridor bands for r_topology
topology_corridor AS (
    SELECT
        cd.safe_band AS safe_topology,
        cd.gold_band AS gold_topology,
        cd.hard_band AS hard_topology
    FROM corridordefinition AS cd
    JOIN riskcoordinate AS rc
      ON rc.coordid   = cd.varid
     AND rc.coordcode = 'R_TOPOLOGY'
    LIMIT 1
)
SELECT
    vr.shardid                               AS shardid,
    vr.vtvalue                               AS vt_core,
    COALESCE(tr.rtopology, 0.0)              AS r_topology,
    COALESCE(tw.w_topology, 0.0)             AS w_topology,
    vr.vtvalue
      + COALESCE(tw.w_topology, 0.0)
        * COALESCE(tr.rtopology, 0.0)
        * COALESCE(tr.rtopology, 0.0)        AS vt_final,
    CASE
        WHEN tr.rtopology IS NULL
            THEN 'UNKNOWN'
        WHEN tr.rtopology <= tc.safe_topology
            THEN 'SAFE'
        WHEN tr.rtopology <= tc.gold_topology
            THEN 'GOLD'
        WHEN tr.rtopology <= tc.hard_topology
            THEN 'HARD'
        ELSE 'SATURATED'
    END                                      AS topology_band,
    CASE
        WHEN tr.rtopology IS NULL THEN 0
        WHEN tr.rtopology <= tc.gold_topology THEN 1
        ELSE 0
    END                                      AS topologyok
FROM vshardresidual AS vr
LEFT JOIN topologyriskmetric AS tr
       ON tr.shardid = vr.shardid
LEFT JOIN topoweight AS tw
       ON tw.planecontractid = vr.planecontractid
LEFT JOIN topology_corridor AS tc;

CREATE INDEX IF NOT EXISTS idx_vshardtopologyker_shardid
    ON vshardtopologyker (shardid);
