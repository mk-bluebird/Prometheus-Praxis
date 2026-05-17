-- filename: eco_restoration_shard/db/db_vecowealthview.sql
-- destination: eco_restoration_shard/db/db_vecowealthview.sql

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

DROP VIEW IF EXISTS vecowealthview;

CREATE VIEW vecowealthview AS
SELECT
    s.stewarddid                      AS stewarddid,
    s.region                          AS region,
    s.lane                            AS lane,
    s.windowstartutc                  AS windowstartutc,
    s.windowendutc                    AS windowendutc,
    -- Steward-level K/E/R aggregates from StewardEcoWealthStatement.
    s.kmean                           AS k_mean,
    s.emean                           AS e_mean,
    s.rmean                           AS r_mean,
    s.rmax                            AS r_max,
    s.vtmaxwindow                     AS vt_max_window,
    -- Governance penalty: topology + representation penalty compounded.
    tp.governance_penalty             AS governance_penalty,
    -- Data quality factor derived from r_calib and r_sigma in vshardker.
    dq.dataqualityfactor              AS dataqualityfactor,
    -- Adjusted K/E/R after governance and data quality discounts.
    (s.kmean * dq.dataqualityfactor)                      AS k_adjusted,
    (s.emean * dq.dataqualityfactor * (1.0 - tp.governance_penalty)) AS e_adjusted,
    (s.rmean + tp.governance_penalty)                    AS r_adjusted,
    -- EcoUnit scalar carried from statement, already respecting kernel contract.
    s.ecounitfinal                    AS ecounit_final,
    s.ecowealthkernelid               AS ecowealthkernelid,
    s.planecontractid                 AS planecontractid,
    s.corridorsetid                   AS corridorsetid,
    s.lanepolicyid                    AS lanepolicyid,
    -- Evidence linkage for replay and hex-stamped proofs.
    s.vshardkerwindowhash             AS vshardkerwindowhash,
    s.shardlisthash                   AS shardlisthash,
    s.evidencehex                     AS evidencehex
FROM stewardecowealthstatement AS s
JOIN (
    -- Trust-weighted dataquality factor per steward window from vshardker.
    SELECT
        vk.stewardid       AS stewarddid,
        sw.windowstartutc  AS windowstartutc,
        sw.windowendutc    AS windowendutc,
        -- Example: dataqualityfactor decays with calibration and sigma risk.
        AVG(1.0 - vk.r_calib) * AVG(1.0 - vk.r_sigma) AS dataqualityfactor
    FROM vshardker AS vk
    JOIN stewardwindowassignment AS sw
      ON sw.stewarddid = vk.stewardid
     AND vk.tendutc BETWEEN sw.windowstartutc AND sw.windowendutc
    GROUP BY vk.stewardid, sw.windowstartutc, sw.windowendutc
) AS dq
  ON dq.stewarddid = s.stewarddid
 AND dq.windowstartutc = s.windowstartutc
 AND dq.windowendutc   = s.windowendutc
JOIN (
    -- Governance penalty from topology and representation planes via vshardtopologyker.
    SELECT
        vt.stewardid      AS stewarddid,
        vt.windowstartutc AS windowstartutc,
        vt.windowendutc   AS windowendutc,
        -- Governance penalty is bounded residual contribution from topology/representation.
        MAX(vt.r_topology + vt.r_representation) AS governance_penalty
    FROM vshardtopologyker AS vt
    GROUP BY vt.stewardid, vt.windowstartutc, vt.windowendutc
) AS tp
  ON tp.stewarddid = s.stewarddid
 AND tp.windowstartutc = s.windowstartutc
 AND tp.windowendutc   = s.windowendutc;

COMMIT;
