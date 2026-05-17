-- filename: eco_restoration_shard/db/db_sensor_trust_integration.sql
-- destination: eco_restoration_shard/db/db_sensor_trust_integration.sql

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- 22. Add r_calib and r_sigma to vshardker and vshardresidual
-- Assumes these views already expose per-shard risk coordinates r_calib and r_sigma
-- or can derive them from shardinstance / dataquality planes.

DROP VIEW IF EXISTS vshardker;

CREATE VIEW vshardker AS
SELECT
    si.shardid,
    si.repoid,
    si.nodeid,
    si.stewardid,
    si.region,
    si.lane,
    si.tstartutc,
    si.tendutc,
    si.kmetric         AS k,
    si.emetric         AS e,
    si.rmetric         AS r,
    si.vtmax           AS vt,
    -- Data quality risk coordinates: calibration and sigma
    dq.r_calib         AS r_calib,
    dq.r_sigma         AS r_sigma,
    -- Topology / representation risk if present
    tr.r_topology      AS r_topology,
    -- Lane / kernel deployability flags
    si.kerdeployable   AS kerdeployable,
    si.planecontractid,
    si.corridorid
FROM shardinstance AS si
LEFT JOIN sharddataquality AS dq
  ON dq.shardid = si.shardid
LEFT JOIN shardtopologyrisk AS tr
  ON tr.shardid = si.shardid;

DROP VIEW IF EXISTS vshardresidual;

CREATE VIEW vshardresidual AS
SELECT
    k.shardid,
    k.repoid,
    k.nodeid,
    k.stewardid,
    k.region,
    k.lane,
    k.tstartutc,
    k.tendutc,
    k.k,
    k.e,
    k.r,
    k.vt,
    -- Residual decomposition by planes, including sensor trust plane.
    rs.vt_materials,
    rs.vt_carbon,
    rs.vt_biodiversity,
    rs.vt_hydraulics,
    rs.vt_energy,
    rs.vt_topology,
    rs.vt_dataquality,
    -- Explicit data quality contributions from calibration and sigma.
    dq.r_calib,
    dq.r_sigma,
    rs.vt_dataquality_calib,
    rs.vt_dataquality_sigma
FROM vshardker AS k
JOIN residualsummary AS rs
  ON rs.shardid = k.shardid
LEFT JOIN sharddataquality AS dq
  ON dq.shardid = k.shardid;

COMMIT;
