-- filename: db/dbphx_dailyevolutionmanifest.sql
-- destination: Eco-Fort/db/dbphx_dailyevolutionmanifest.sql
-- description: Unified daily evolution manifest for Phoenix-AZ-US stewards.
-- This file defines a read-only composition layer that joins KER, EcoUnit,
-- responsibility, RoH, and representation floors into a single daily manifest
-- surface, suitable for non-actuating queries and CI replay.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Assumed existing tables and views (Phoenix spine)
--
-- shardinstance
--   shardid TEXT PRIMARY KEY,
--   stewarddid TEXT,
--   regioncode TEXT,
--   tsstartutc TEXT,
--   tsendutc TEXT,
--   planecontractid INTEGER,
--   kmetric REAL,
--   emetric REAL,
--   rmetric REAL,
--   vtmax REAL,
--   lane TEXT,
--   kerdeployable INTEGER
--
-- vshardker (from dblanelyapunovker.sql)
--   shardid TEXT,
--   tsstartutc TEXT,
--   tsendutc TEXT,
--   residualkernelcode TEXT,
--   planecontractid INTEGER,
--   vtcore REAL,
--   itopology REAL,
--   rtopology REAL,
--   wtopology REAL,
--   vtwithtopology REAL,
--   kvalue REAL,
--   evalue REAL,
--   rvalue REAL,
--   lane TEXT,
--   kerdeployable INTEGER
--
-- vshardkerviolation (from dblanelyapunovker.sql)
--   shardid TEXT,
--   tsstartutc TEXT,
--   tsendutc TEXT,
--   vtcurrent REAL,
--   vtprevious REAL,
--   deltavt REAL,
--   epsresidual REAL,
--   violateslyapunov INTEGER
--
-- kerresidualsnapshot
--   shardid TEXT,
--   tsendutc TEXT,
--   regioncode TEXT,
--   vtcore REAL,
--   vtwithoverlays REAL,
--   reff REAL
--
-- responsibilitymetric
--   shardid TEXT PRIMARY KEY,
--   rresponsibility REAL,
--   rpharma REAL,
--   rtoxicity REAL,
--   roverride REAL
--
-- portfoliodiversitymetric
--   stewarddid TEXT,
--   regioncode TEXT,
--   epoch TEXT,
--   rportfoliodiversity REAL
--
-- rohshard
--   shardid TEXT PRIMARY KEY,
--   rohmax REAL,
--   rohwindowstartutc TEXT,
--   rohwindowendutc TEXT,
--   rohkernelcode TEXT
--
-- lifeforcetraitshard
--   shardid TEXT PRIMARY KEY,
--   stewarddid TEXT,
--   regioncode TEXT,
--   lifeforcedelta REAL,
--   biokarmadelta REAL,
--   mt6883registryid TEXT
--
-- StewardEcoWealthStatement
--   stewarddid TEXT,
--   regioncode TEXT,
--   lane TEXT,
--   windowstartutc TEXT,
--   windowendutc TEXT,
--   kmean REAL,
--   kmin REAL,
--   kmax REAL,
--   emean REAL,
--   emin REAL,
--   emax REAL,
--   rmean REAL,
--   rmax REAL,
--   vtmaxwindow REAL,
--   kdelta REAL,
--   edelta REAL,
--   rdelta REAL,
--   ecounitraw REAL,
--   mkeducation REAL,
--   deltaephys REAL,
--   ecounitaftereducation REAL,
--   representationweight REAL,
--   ecounitfinal REAL,
--   ecowealthkernelid TEXT,
--   planecontractid TEXT,
--   corridorsetid TEXT,
--   lanepolicyid TEXT,
--   vshardkerwindowhash TEXT,
--   shardlisthash TEXT,
--   evidencehex TEXT,
--   signingdid TEXT,
--   createdutc TEXT
--
-- vecowealthview
--   stewarddid TEXT,
--   regioncode TEXT,
--   lane TEXT,
--   windowstartutc TEXT,
--   windowendutc TEXT,
--   kmean REAL,
--   emean REAL,
--   rmean REAL,
--   vtmaxwindow REAL,
--   governancepenalty REAL,
--   dataqualityfactor REAL,
--   ecounitfinal REAL
--
-- regionrepresentation
--   regionid TEXT PRIMARY KEY,
--   sensordensityindex REAL,
--   representationfloor REAL,
--   maxtrustweight REAL,
--   lastupdatedutc TEXT,
--   evidencehex TEXT
--
-- veconetstakekarmadailyphx
--   regioncode TEXT,
--   dailytag TEXT,
--   day TEXT,
--   karmaunitstotalsum REAL,
--   karmaunitsliquidsum REAL,
--   karmaunitsrestrictedsum REAL,
--   ecounitscreditedsum REAL,
--   ecounitsliquidsum REAL,
--   ecounitsrestrictedsum REAL,
--   ecoliabilityunitssum REAL
--
-- econetstaketerminalbatch2026q2phx
--   batchid TEXT,
--   stewarddid TEXT,
--   regioncode TEXT,
--   lane TEXT,
--   batchwindowstartutc TEXT,
--   batchwindowendutc TEXT,
--   ecounitscredited REAL,
--   ecounitsliquid REAL,
--   ecounitsrestricted REAL
--
-- Note: this file defines only read-only views; no new tables with mutable
-- state are introduced.

----------------------------------------------------------------------
-- 2. Helper view: Phoenix steward-day base index
--
-- This view normalizes shardinstance rows into Phoenix steward-day bins.
-- It is used as a backbone for joining KER, responsibility, RoH, and ecowealth.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_steward_day_base AS
SELECT
    si.stewarddid                   AS stewarddid,
    si.regioncode                   AS regioncode,
    date(si.tsendutc)               AS day,
    MIN(si.tsstartutc)              AS first_tsstartutc,
    MAX(si.tsendutc)                AS last_tsendutc
FROM shardinstance AS si
WHERE si.regioncode = 'Phoenix-AZ-US'
GROUP BY
    si.stewarddid,
    si.regioncode,
    date(si.tsendutc);

CREATE INDEX IF NOT EXISTS idx_vphx_steward_day_base
ON vphx_steward_day_base (regioncode, day, stewarddid);

----------------------------------------------------------------------
-- 3. Helper view: Phoenix daily KER and residual aggregates
--
-- Aggregates vshardker and kerresidualsnapshot into per-steward-day metrics.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_steward_day_ker AS
SELECT
    b.stewarddid                                           AS stewarddid,
    b.regioncode                                           AS regioncode,
    b.day                                                  AS day,
    AVG(k.kvalue)                                          AS k_mean_day,
    MIN(k.kvalue)                                          AS k_min_day,
    MAX(k.kvalue)                                          AS k_max_day,
    AVG(k.evalue)                                          AS e_mean_day,
    MIN(k.evalue)                                          AS e_min_day,
    MAX(k.evalue)                                          AS e_max_day,
    AVG(k.rvalue)                                          AS r_mean_day,
    MAX(k.rvalue)                                          AS r_max_day,
    MAX(k.vtwithtopology)                                  AS vt_max_day,
    AVG(rs.reff)                                           AS reff_mean_day
FROM vphx_steward_day_base AS b
JOIN shardinstance AS si
  ON si.stewarddid = b.stewarddid
 AND si.regioncode = b.regioncode
 AND date(si.tsendutc) = b.day
JOIN vshardker AS k
  ON k.shardid = si.shardid
 AND k.tsendutc = si.tsendutc
LEFT JOIN kerresidualsnapshot AS rs
  ON rs.shardid = si.shardid
 AND rs.tsendutc = si.tsendutc
GROUP BY
    b.stewarddid,
    b.regioncode,
    b.day;

CREATE INDEX IF NOT EXISTS idx_vphx_steward_day_ker
ON vphx_steward_day_ker (regioncode, day, stewarddid);

----------------------------------------------------------------------
-- 4. Helper view: Phoenix daily responsibility and RoH overlays
--
-- Combines responsibilitymetric, portfoliodiversitymetric, and rohshard.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_steward_day_responsibility AS
SELECT
    b.stewarddid                           AS stewarddid,
    b.regioncode                           AS regioncode,
    b.day                                  AS day,
    AVG(rm.rresponsibility)               AS rresponsibility_day,
    AVG(rm.rpharma)                       AS rpharma_day,
    AVG(rm.rtoxicity)                     AS rtoxicity_day,
    AVG(rm.roverride)                     AS roverride_day,
    MAX(ro.rohmax)                        AS roh_max_day,
    CASE
        WHEN MAX(ro.rohmax) IS NULL THEN 1
        WHEN MAX(ro.rohmax) <= 0.30 THEN 1
        ELSE 0
    END                                   AS roh_ok_day
FROM vphx_steward_day_base AS b
JOIN shardinstance AS si
  ON si.stewarddid = b.stewarddid
 AND si.regioncode = b.regioncode
 AND date(si.tsendutc) = b.day
LEFT JOIN responsibilitymetric AS rm
  ON rm.shardid = si.shardid
LEFT JOIN rohshard AS ro
  ON ro.shardid = si.shardid
GROUP BY
    b.stewarddid,
    b.regioncode,
    b.day;

CREATE INDEX IF NOT EXISTS idx_vphx_steward_day_responsibility
ON vphx_steward_day_responsibility (regioncode, day, stewarddid);

----------------------------------------------------------------------
-- 5. Helper view: Phoenix daily lifeforce and biokarma metrics
--
-- Aggregates lifeforcetraitshard for vampiric / healthcare shards.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_steward_day_lifeforce AS
SELECT
    b.stewarddid                           AS stewarddid,
    b.regioncode                           AS regioncode,
    b.day                                  AS day,
    SUM(lt.lifeforcedelta)                AS lifeforce_delta_day,
    SUM(lt.biokarmadelta)                 AS biokarma_delta_day
FROM vphx_steward_day_base AS b
LEFT JOIN lifeforcetraitshard AS lt
  ON lt.stewarddid = b.stewarddid
 AND lt.regioncode = b.regioncode
 AND date(lt.shardid) IS NOT NULL -- placeholder; shardid is not a timestamp
-- In practice, lifeforcetraitshard should carry a tsendutc or day column.
-- Adjust this join when the schema exposes temporal fields explicitly.
GROUP BY
    b.stewarddid,
    b.regioncode,
    b.day;

CREATE INDEX IF NOT EXISTS idx_vphx_steward_day_lifeforce
ON vphx_steward_day_lifeforce (regioncode, day, stewarddid);

----------------------------------------------------------------------
-- 6. Helper view: Phoenix daily EcoUnit aggregates
--
-- Aggregates StewardEcoWealthStatement and vecowealthview by day.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_steward_day_ecounit AS
SELECT
    s.stewarddid                                   AS stewarddid,
    s.regioncode                                   AS regioncode,
    date(s.windowendutc)                           AS day,
    SUM(s.ecounitfinal)                            AS ecounit_final_day,
    AVG(v.kmean)                                   AS kmean_window_day,
    AVG(v.emean)                                   AS emean_window_day,
    AVG(v.rmean)                                   AS rmean_window_day,
    AVG(v.vtmaxwindow)                             AS vtmaxwindow_day,
    AVG(v.governancepenalty)                       AS governancepenalty_day,
    AVG(v.dataqualityfactor)                       AS dataqualityfactor_day
FROM StewardEcoWealthStatement AS s
JOIN vecowealthview AS v
  ON v.stewarddid = s.stewarddid
 AND v.regioncode = s.regioncode
 AND v.windowstartutc = s.windowstartutc
 AND v.windowendutc = s.windowendutc
WHERE s.regioncode = 'Phoenix-AZ-US'
GROUP BY
    s.stewarddid,
    s.regioncode,
    date(s.windowendutc);

CREATE INDEX IF NOT EXISTS idx_vphx_steward_day_ecounit
ON vphx_steward_day_ecounit (regioncode, day, stewarddid);

----------------------------------------------------------------------
-- 7. Helper view: Phoenix daily stake-based EcoUnit aggregates
--
-- Uses veconetstakekarmadailyphx and econetstaketerminalbatch2026q2phx.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_steward_day_stake AS
SELECT
    b.stewarddid                                 AS stewarddid,
    b.regioncode                                 AS regioncode,
    date(b.batchwindowendutc)                    AS day,
    SUM(b.ecounitscredited)                      AS ecounits_credited_day,
    SUM(b.ecounitsliquid)                        AS ecounits_liquid_day,
    SUM(b.ecounitsrestricted)                    AS ecounits_restricted_day
FROM econetstaketerminalbatch2026q2phx AS b
WHERE b.regioncode = 'Phoenix-AZ-US'
GROUP BY
    b.stewarddid,
    b.regioncode,
    date(b.batchwindowendutc);

CREATE INDEX IF NOT EXISTS idx_vphx_steward_day_stake
ON vphx_steward_day_stake (regioncode, day, stewarddid);

----------------------------------------------------------------------
-- 8. Helper view: Phoenix region representation snapshot
--
-- Exposes representationfloor and maxtrustweight for Phoenix-AZ-US.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_region_representation AS
SELECT
    rr.regionid                AS regioncode,
    rr.sensordensityindex      AS sensordensityindex,
    rr.representationfloor     AS representationfloor,
    rr.maxtrustweight          AS maxtrustweight,
    rr.lastupdatedutc          AS lastupdatedutc
FROM regionrepresentation AS rr
WHERE rr.regionid = 'Phoenix-AZ-US';

----------------------------------------------------------------------
-- 9. Helper view: Phoenix daily representation share
--
-- Computes daily EcoUnit share per steward vs region total, to check
-- representation floors and concentration.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_steward_day_representation AS
WITH region_totals AS (
    SELECT
        e.regioncode                AS regioncode,
        e.day                       AS day,
        SUM(e.ecounit_final_day)    AS ecounit_region_day
    FROM vphx_steward_day_ecounit AS e
    GROUP BY
        e.regioncode,
        e.day
)
SELECT
    e.stewarddid                               AS stewarddid,
    e.regioncode                               AS regioncode,
    e.day                                      AS day,
    e.ecounit_final_day                        AS ecounit_final_day,
    rt.ecounit_region_day                      AS ecounit_region_day,
    CASE
        WHEN rt.ecounit_region_day IS NULL
             OR rt.ecounit_region_day <= 0.0 THEN NULL
        ELSE e.ecounit_final_day / rt.ecounit_region_day
    END                                        AS ecowealth_share_day
FROM vphx_steward_day_ecounit AS e
LEFT JOIN region_totals AS rt
  ON rt.regioncode = e.regioncode
 AND rt.day = e.day;

CREATE INDEX IF NOT EXISTS idx_vphx_steward_day_representation
ON vphx_steward_day_representation (regioncode, day, stewarddid);

----------------------------------------------------------------------
-- 10. Phoenix unified daily evolution manifest
--
-- This is the primary composition layer. It joins:
--   - vphx_steward_day_ker               (KER and residual)
--   - vphx_steward_day_responsibility    (ResponsibilityAxis, RoH)
--   - vphx_steward_day_lifeforce         (lifeforce and biokarma)
--   - vphx_steward_day_ecounit           (EcoUnit issuance)
--   - vphx_steward_day_stake             (stake-based EcoUnits)
--   - vphx_steward_day_representation    (representation share)
--   - vphx_region_representation         (representation floors)
--
-- It also enforces non-actuating filters:
--   - regioncode = 'Phoenix-AZ-US'
--   - roh_ok_day = 1 (RoH <= 0.30)
--   - no Lyapunov violations (checked via subquery)
--   - kerdeployable = 1 at shard level contributing to the day
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vphx_daily_evolution_manifest AS
WITH lyapunov_ok AS (
    SELECT DISTINCT
        si.stewarddid AS stewarddid,
        si.regioncode AS regioncode,
        date(si.tsendutc) AS day
    FROM vshardkerviolation AS kv
    JOIN shardinstance AS si
      ON si.shardid = kv.shardid
     AND si.tsendutc = kv.tsendutc
    WHERE kv.violateslyapunov = 1
),
kerdeployable_ok AS (
    SELECT DISTINCT
        si.stewarddid AS stewarddid,
        si.regioncode AS regioncode,
        date(si.tsendutc) AS day
    FROM shardinstance AS si
    WHERE si.kerdeployable = 0
)
SELECT
    k.stewarddid                              AS stewarddid,
    k.regioncode                              AS regioncode,
    k.day                                     AS day,
    k.k_mean_day                              AS k_mean_day,
    k.k_min_day                               AS k_min_day,
    k.k_max_day                               AS k_max_day,
    k.e_mean_day                              AS e_mean_day,
    k.e_min_day                               AS e_min_day,
    k.e_max_day                               AS e_max_day,
    k.r_mean_day                              AS r_mean_day,
    k.r_max_day                               AS r_max_day,
    k.vt_max_day                              AS vt_max_day,
    k.reff_mean_day                           AS reff_mean_day,
    r.rresponsibility_day                     AS rresponsibility_day,
    r.rpharma_day                             AS rpharma_day,
    r.rtoxicity_day                           AS rtoxicity_day,
    r.roverride_day                           AS roverride_day,
    r.roh_max_day                             AS roh_max_day,
    r.roh_ok_day                              AS roh_ok_day,
    lf.lifeforce_delta_day                    AS lifeforce_delta_day,
    lf.biokarma_delta_day                     AS biokarma_delta_day,
    e.ecounit_final_day                       AS ecounit_final_day,
    e.kmean_window_day                        AS kmean_window_day,
    e.emean_window_day                        AS emean_window_day,
    e.rmean_window_day                        AS rmean_window_day,
    e.vtmaxwindow_day                         AS vtmaxwindow_day,
    e.governancepenalty_day                   AS governancepenalty_day,
    e.dataqualityfactor_day                   AS dataqualityfactor_day,
    s.ecounits_credited_day                   AS ecounits_credited_day,
    s.ecounits_liquid_day                     AS ecounits_liquid_day,
    s.ecounits_restricted_day                 AS ecounits_restricted_day,
    rep.ecounit_region_day                    AS ecounit_region_day,
    rep.ecowealth_share_day                   AS ecowealth_share_day,
    rr.representationfloor                    AS representationfloor,
    CASE
        WHEN rep.ecowealth_share_day IS NULL THEN NULL
        WHEN rr.representationfloor IS NULL THEN NULL
        WHEN rep.ecowealth_share_day + 1e-9 >= rr.representationfloor THEN 1
        ELSE 0
    END                                       AS representation_ok_day
FROM vphx_steward_day_ker AS k
JOIN vphx_steward_day_responsibility AS r
  ON r.stewarddid = k.stewarddid
 AND r.regioncode = k.regioncode
 AND r.day = k.day
LEFT JOIN vphx_steward_day_lifeforce AS lf
  ON lf.stewarddid = k.stewarddid
 AND lf.regioncode = k.regioncode
 AND lf.day = k.day
LEFT JOIN vphx_steward_day_ecounit AS e
  ON e.stewarddid = k.stewarddid
 AND e.regioncode = k.regioncode
 AND e.day = k.day
LEFT JOIN vphx_steward_day_stake AS s
  ON s.stewarddid = k.stewarddid
 AND s.regioncode = k.regioncode
 AND s.day = k.day
LEFT JOIN vphx_steward_day_representation AS rep
  ON rep.stewarddid = k.stewarddid
 AND rep.regioncode = k.regioncode
 AND rep.day = k.day
LEFT JOIN vphx_region_representation AS rr
  ON rr.regioncode = k.regioncode
WHERE k.regioncode = 'Phoenix-AZ-US'
  AND r.roh_ok_day = 1
  AND NOT EXISTS (
        SELECT 1
        FROM lyapunov_ok AS lo
        WHERE lo.stewarddid = k.stewarddid
          AND lo.regioncode = k.regioncode
          AND lo.day = k.day
    )
  AND NOT EXISTS (
        SELECT 1
        FROM kerdeployable_ok AS ko
        WHERE ko.stewarddid = k.stewarddid
          AND ko.regioncode = k.regioncode
          AND ko.day = k.day
    );

CREATE INDEX IF NOT EXISTS idx_vphx_daily_evolution_manifest
ON vphx_daily_evolution_manifest (regioncode, day, stewarddid);
