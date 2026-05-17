-- filename: db_region_representation_and_lane_admissibility.sql
-- destination: eco_restoration_shard/sql/spine/db_region_representation_and_lane_admissibility.sql

-- 1. Region representation table (with trust caps and floors)
CREATE TABLE IF NOT EXISTS regionrepresentation (
    regionid TEXT PRIMARY KEY,
    -- 0.0 (no sensors) .. 1.0 (very dense instrumentation)
    sensordensityindex REAL NOT NULL CHECK (sensordensityindex BETWEEN 0.0 AND 1.0),

    -- Minimum fractional share of global ecowealth budget this region must receive
    representationfloorweight REAL NOT NULL CHECK (representationfloorweight BETWEEN 0.0 AND 1.0),

    -- Maximum trust-based multiplier allowed for this region; caps K/trust upweighting
    maxtrustweight REAL NOT NULL CHECK (maxtrustweight >= representationfloorweight),

    -- Minimum region share used in rtopology / representation imbalance calculations
    minregionshare REAL NOT NULL CHECK (minregionshare BETWEEN 0.0 AND 1.0),

    -- Minimum share for designated steward classes (e.g. low-income, Indigenous)
    minstewardclassshare REAL NOT NULL CHECK (minstewardclassshare BETWEEN 0.0 AND 1.0),

    -- Floor on data quality required for floors/boosts to apply; derived from rcalib/rsigma
    dataquality_floor REAL NOT NULL CHECK (dataquality_floor BETWEEN 0.0 AND 1.0),

    created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    updated_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

CREATE TRIGGER IF NOT EXISTS trg_regionrepresentation_update_utc
AFTER UPDATE ON regionrepresentation
BEGIN
    UPDATE regionrepresentation
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ', 'now')
    WHERE rowid = NEW.rowid;
END;

CREATE INDEX IF NOT EXISTS idx_regionrepresentation_floor
    ON regionrepresentation (representationfloorweight, maxtrustweight);

----------------------------------------------------------------------
-- 2. StewardEcoWealthStatement table (qpudatashard for EcoUnit replay)
----------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS steward_ecowealth_statement (
    -- Identity
    stewarddid TEXT NOT NULL,
    region     TEXT NOT NULL,
    basin      TEXT,
    lane       TEXT NOT NULL, -- RESEARCH / EXP / PROD

    -- Window bounds
    windowstartutc TEXT NOT NULL,
    windowendutc   TEXT NOT NULL,

    -- KER aggregates
    kmean REAL NOT NULL CHECK (kmean BETWEEN 0.0 AND 1.0),
    kmin  REAL NOT NULL CHECK (kmin  BETWEEN 0.0 AND 1.0),
    kmax  REAL NOT NULL CHECK (kmax  BETWEEN 0.0 AND 1.0),

    emean REAL NOT NULL CHECK (emean BETWEEN 0.0 AND 1.0),
    emin  REAL NOT NULL CHECK (emin  BETWEEN 0.0 AND 1.0),
    emax  REAL NOT NULL CHECK (emax  BETWEEN 0.0 AND 1.0),

    rmean REAL NOT NULL CHECK (rmean BETWEEN 0.0 AND 1.0),
    rmax  REAL NOT NULL CHECK (rmax  BETWEEN 0.0 AND 1.0),

    vtmaxwindow REAL NOT NULL CHECK (vtmaxwindow >= 0.0),

    -- KER deltas vs previous window
    kdelta REAL NOT NULL,
    edelta REAL NOT NULL,
    rdelta REAL NOT NULL,

    -- Ecowealth computation
    ecounitraw            REAL NOT NULL CHECK (ecounitraw >= 0.0),
    mkeducation           REAL NOT NULL CHECK (mkeducation >= 0.0),
    deltaephys            REAL NOT NULL,
    ecounitaftereducation REAL NOT NULL CHECK (ecounitaftereducation >= 0.0),

    -- Representation weight derived from regionrepresentation floors/caps
    representationweight REAL NOT NULL CHECK (representationweight >= 0.0),

    -- Final EcoUnit after education + representation multipliers
    ecounitfinal REAL NOT NULL CHECK (ecounitfinal >= 0.0),

    -- Governance context
    ecowealthkernelid TEXT NOT NULL,
    planecontractid   TEXT NOT NULL,
    corridorsetid     TEXT NOT NULL,
    lanepolicyid      TEXT NOT NULL,

    -- Evidence signatures: replayability hooks
    vshardkerwindowhash TEXT NOT NULL,
    shardlisthash       TEXT NOT NULL,
    evidencehex         TEXT NOT NULL,

    signingdid TEXT NOT NULL,
    createdutc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),

    -- Primary key over steward + window + kernel context
    PRIMARY KEY (stewarddid, windowendutc, ecowealthkernelid, corridorsetid)
);

-- Fast lookups by steward and region for dashboards and AIchat
CREATE INDEX IF NOT EXISTS idx_steward_ecowealth_statement_steward_time
    ON steward_ecowealth_statement (stewarddid, windowendutc DESC);

CREATE INDEX IF NOT EXISTS idx_steward_ecowealth_statement_region_lane_time
    ON steward_ecowealth_statement (region, lane, windowendutc DESC);

CREATE INDEX IF NOT EXISTS idx_steward_ecowealth_statement_kernel_context
    ON steward_ecowealth_statement (ecowealthkernelid, planecontractid, corridorsetid);

CREATE INDEX IF NOT EXISTS idx_steward_ecowealth_statement_evidence
    ON steward_ecowealth_statement (vshardkerwindowhash, shardlisthash);

----------------------------------------------------------------------
-- 3. Lane admissibility view over vshardker
--
-- Assumes vshardker(view) exposes:
--   shardid, stewarddid, region, lane, windowstartutc, windowendutc,
--   kmean, emean, rmean, vtmaxwindow, kerdeployable (0/1)
--
-- And lanedecision / LaneStatusShard2026v1 mirrored in table lanestatusshard:
--   reponame, kernelid, region, lane, kmin, emin, rmax, vttrend,
--   corridorok, planesok, topologyok, evidencehex, signingdid
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS lane_admissibility AS
SELECT
    v.stewarddid,
    v.region,
    v.lane,
    v.windowstartutc,
    v.windowendutc,
    v.kmean,
    v.emean,
    v.rmean,
    v.vtmaxwindow,
    v.kerdeployable,

    ls.kernelid,
    ls.kmin    AS lane_kmin,
    ls.emin    AS lane_emin,
    ls.rmax    AS lane_rmax,
    ls.vttrend AS lane_vttrend,

    ls.corridorok,
    ls.planesok,
    ls.topologyok,

    -- Derived booleans: admissible if K/E/R within thresholds, vttrend<=0, kerdeployable=1
    CASE
        WHEN v.kerdeployable = 1
         AND v.kmean >= ls.kmin
         AND v.emean >= ls.emin
         AND v.rmean <= ls.rmax
         AND ls.vttrend <= 0.0
         AND ls.corridorok = 1
         AND ls.planesok   = 1
         AND ls.topologyok = 1
        THEN 1
        ELSE 0
    END AS lane_admissible
FROM
    vshardker AS v
JOIN
    lanestatusshard AS ls
      ON ls.region = v.region
     AND ls.lane   = v.lane
     -- kernelid binding can be customized per installation
;

----------------------------------------------------------------------
-- 4. Lane/EcoUnit replay view:
--    fast join between lane_admissibility and steward_ecowealth_statement
--    for sub-second EcoUnit/KER dashboards.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_steward_lane_ecounit AS
SELECT
    s.stewarddid,
    s.region,
    s.basin,
    s.lane,
    s.windowstartutc,
    s.windowendutc,

    -- KER aggregates (from StewardEcoWealthStatement; must match vshardkerwindowhash)
    s.kmean,
    s.emean,
    s.rmean,
    s.vtmaxwindow,

    -- Ecowealth scalar components
    s.ecounitraw,
    s.mkeducation,
    s.deltaephys,
    s.ecounitaftereducation,
    s.representationweight,
    s.ecounitfinal,

    -- Lane admissibility and kerdeployable
    la.kernelid,
    la.lane_kmin,
    la.lane_emin,
    la.lane_rmax,
    la.lane_vttrend,
    la.lane_admissible,
    la.kerdeployable,

    -- Evidence for replay
    s.ecowealthkernelid,
    s.planecontractid,
    s.corridorsetid,
    s.lanepolicyid,
    s.vshardkerwindowhash,
    s.shardlisthash,
    s.evidencehex,
    s.signingdid,
    s.createdutc
FROM
    steward_ecowealth_statement AS s
LEFT JOIN
    lane_admissibility AS la
      ON la.stewarddid    = s.stewarddid
     AND la.region        = s.region
     AND la.lane          = s.lane
     AND la.windowendutc  = s.windowendutc
;

----------------------------------------------------------------------
-- 5. Invariants for education disinflation and representation caps
--
-- These cannot be expressed as full SQL CHECKs across joined tables,
-- but CI can enforce them via queries over v_steward_lane_ecounit:
--
--   - If deltaephys <= 0 then mkeducation <= 1
--     and ecounitfinal <= ecounitraw * representationweight.
--
--   - If deltaephys > 0 then mkeducation <= M_max
--     and ecounitfinal - ecounitraw * representationweight
--         <= (M_max - 1) * ecounitraw * representationweight.
--
--   - For each region, derived trust multiplier must not exceed
--     regionrepresentation.maxtrustweight; representation floors and
--     caps are enforced when aggregating over steward_ecowealth_statement.
--
-- These rules are executed in CI pipelines and governance queries,
-- keeping kernel logic in Rust/ALN but replayable via SQLite.
----------------------------------------------------------------------
