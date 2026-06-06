-- filename: dataconstellation/econet_cyboquatic_machinery_spine.sql
-- destination: eco_restoration_shard/dataconstellation/econet_cyboquatic_machinery_spine.sql
-- Purpose:
-- - Specialize the EcoNet constellation index toward Cyboquatic industrial machinery.
-- - Track energy efficiency, carbon-negativity, biodegradable materials, and blast-radius for Cyboquatic nodes.
-- - Provide an "always-improve" scoring layer (K, E, R) for every artifact, node, and corridor change.
-- - Remain strictly non-actuating: evidence, diagnostics, and governance only.

PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

-------------------------------------------------------------------------------
-- 1. Repositories and roles (subset focused on Cyboquatic + eco-restoration)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_repositories (
    repoid          INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame        TEXT NOT NULL UNIQUE, -- e.g. "EcoNet", "eco_restoration_shard", "Cyboquatics"
    githubslug      TEXT NOT NULL,        -- e.g. "mk-bluebird/eco_restoration_shard"
    roleband        TEXT NOT NULL CHECK (roleband IN ('SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP')),
    lanedefault     TEXT NOT NULL CHECK (lanedefault IN ('RESEARCH','EXPPROD','PROD')),
    primarylanguage TEXT NOT NULL,        -- Rust, C, Lua, Kotlin, ALN, etc.
    description     TEXT,
    bostromdid      TEXT NOT NULL,        -- owner DID, e.g. bostrom18sd...
    ecosafetybinding TEXT NOT NULL,       -- e.g. "ecosafety.corridors.v2"
    shardprotocol   TEXT NOT NULL,        -- e.g. "EcoNetSchemaShard2026v1"
    createdutc      TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc      TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_cybo_repositories_name
    ON cybo_repositories (reponame);

-------------------------------------------------------------------------------
-- 2. Files / artifacts within repos
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_artifacts (
    artifactid      INTEGER PRIMARY KEY AUTOINCREMENT,
    repoid          INTEGER NOT NULL REFERENCES cybo_repositories(repoid) ON DELETE CASCADE,
    artifactpath    TEXT NOT NULL,    -- repo-relative path
    artifacttype    TEXT NOT NULL CHECK (
                         artifacttype IN ('source','schema','shard','kernel','test','doc','config','other')
                     ),
    language        TEXT,             -- Rust, C, Lua, Kotlin, SQL, ALN, etc.
    hexstamp        TEXT,             -- hex evidence for this artifact version
    loc             INTEGER DEFAULT 0,
    createdutc      TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc      TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (repoid, artifactpath)
);

CREATE INDEX IF NOT EXISTS idx_cybo_artifacts_repo
    ON cybo_artifacts (repoid, artifacttype);

CREATE INDEX IF NOT EXISTS idx_cybo_artifacts_hexstamp
    ON cybo_artifacts (hexstamp);

-------------------------------------------------------------------------------
-- 3. Cyboquatic nodes, channels, and materials
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_nodes (
    nodeid          TEXT PRIMARY KEY, -- e.g. "PHX-CYBOQ-MAR-001"
    region          TEXT NOT NULL,    -- e.g. "Phoenix-AZ"
    medium          TEXT NOT NULL,    -- "water","air","soil","mixed"
    nodetype        TEXT NOT NULL,    -- "MAR","FOG-CHANNEL","FLOWVAC","SEWER","PLANT"
    description     TEXT,
    commissioning_utc TEXT,
    decommission_utc TEXT
);

CREATE TABLE IF NOT EXISTS cybo_channels (
    channelid       INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid          TEXT NOT NULL REFERENCES cybo_nodes(nodeid) ON DELETE CASCADE,
    channel_name    TEXT NOT NULL,   -- e.g. "PFBS-MAR-PRIMARY", "FOG-SIDE-CHANNEL"
    ecoplane        TEXT NOT NULL CHECK (
                         ecoplane IN ('energy','hydraulics','carbon','materials','biodiversity','dataquality')
                     ),
    is_biodegradable INTEGER NOT NULL CHECK (is_biodegradable IN (0,1)) DEFAULT 0,
    tray_material_id TEXT,           -- foreign key string into substrate/batch IDs
    UNIQUE (nodeid, channel_name, ecoplane)
);

CREATE TABLE IF NOT EXISTS cybo_material_batches (
    materialid      TEXT PRIMARY KEY,  -- e.g. "BioSubstrateBatch-2026-03A"
    description     TEXT,
    manufacturer    TEXT,
    t90_days        REAL,         -- characteristic decay time
    rtox            REAL,         -- 0..1 toxicity risk
    rmicro          REAL,         -- 0..1 micro-residue risk
    rleach_cec      REAL,         -- 0..1 cation exchange leachate risk
    rpfas_resid     REAL,         -- 0..1 PFAS residual risk
    rcarbon         REAL,         -- 0..1 carbon footprint risk
    rbiodiv         REAL,         -- 0..1 biodiversity impact risk
    vtsubstrate     REAL,         -- Lyapunov residual for this batch
    kscore          REAL,         -- knowledge factor for this batch
    escore          REAL,         -- eco-impact factor for this batch
    rscore          REAL,         -- risk-of-harm factor for this batch
    kerdeployable   INTEGER NOT NULL CHECK(kerdeployable IN (0,1)) DEFAULT 0,
    evidencehex     TEXT,
    createdutc      TEXT NOT NULL DEFAULT (datetime('now'))
);

-------------------------------------------------------------------------------
-- 4. Cyboquatic workload ledger (energy, carbon, materials, biodiv)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_workload_ledger (
    ledgerid        INTEGER PRIMARY KEY AUTOINCREMENT,
    nodeid          TEXT NOT NULL REFERENCES cybo_nodes(nodeid) ON DELETE CASCADE,
    channel_name    TEXT NOT NULL,    -- link to cybo_channels
    ecoplane        TEXT NOT NULL CHECK (
                         ecoplane IN ('energy','hydraulics','carbon','materials','biodiversity')
                     ),
    shardid         INTEGER,         -- optional link to shardinstance in EcoNet spine
    variantid       TEXT NOT NULL,   -- e.g. "CyboRoute-v1"
    ereq_j          REAL NOT NULL,   -- requested energy (J)
    esurplus_j      REAL NOT NULL,   -- surplus energy (J)
    rplane          REAL,            -- risk coord on this plane (0..1)
    vt_before       REAL NOT NULL,
    vt_after        REAL NOT NULL,
    decision        TEXT NOT NULL CHECK (
                         decision IN ('ACCEPT','REJECT','REROUTE')
                     ),
    carbon_kg       REAL,            -- carbon footprint for this window (kg CO2e)
    carbon_offset_kg REAL,           -- offset provided for this workload (kg CO2e)
    timestamputc    TEXT NOT NULL,   -- ISO8601
    evidencehex     TEXT
);

CREATE INDEX IF NOT EXISTS idx_cybo_ledger_node_time
    ON cybo_workload_ledger (nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_cybo_ledger_plane
    ON cybo_workload_ledger (ecoplane, decision);

-------------------------------------------------------------------------------
-- 5. Blast-radius links at Cyboquatic scale
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_blast_radius (
    linkid          INTEGER PRIMARY KEY AUTOINCREMENT,
    sourcetype      TEXT NOT NULL CHECK (sourcetype IN ('NODE','SHARD','MATERIAL')),
    sourceid        TEXT NOT NULL, -- nodeid, shardid (string), or materialid
    targettype      TEXT NOT NULL CHECK (targettype IN ('NODE','REGION','MATERIAL','REPO')),
    targetid        TEXT NOT NULL,
    ecoplane        TEXT NOT NULL CHECK (
                         ecoplane IN ('energy','hydraulics','carbon','materials','biodiversity','dataquality')
                     ),
    impactscore     REAL NOT NULL, -- fraction of corridor width influenced (0..1)
    vt_sensitivity  REAL,          -- approx ΔVt for diagnostics
    notes           TEXT
);

CREATE INDEX IF NOT EXISTS idx_cybo_blast_source
    ON cybo_blast_radius (sourcetype, sourceid, ecoplane);

CREATE INDEX IF NOT EXISTS idx_cybo_blast_target
    ON cybo_blast_radius (targettype, targetid, ecoplane);

-------------------------------------------------------------------------------
-- 6. KER / eco-impact scoring and "always-improve" metrics
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_ker_scores (
    scoreid         INTEGER PRIMARY KEY AUTOINCREMENT,
    scopetype       TEXT NOT NULL CHECK (
                         scopetype IN ('REPO','ARTIFACT','NODE','CHANNEL','MATERIAL','CORRIDOR')
                     ),
    scoperefid      TEXT NOT NULL,          -- interpreted by scopetype
    kfactor         REAL NOT NULL CHECK (kfactor >= 0.0 AND kfactor <= 1.0),
    efactor         REAL NOT NULL CHECK (efactor >= 0.0 AND efactor <= 1.0),
    rfactor         REAL NOT NULL CHECK (rfactor >= 0.0 AND rfactor <= 1.0),
    evaluation_utc  TEXT NOT NULL,          -- evaluation timestamp
    evaluator_did   TEXT NOT NULL,          -- DID or agent id
    rationale       TEXT
);

CREATE INDEX IF NOT EXISTS idx_cybo_ker_scope
    ON cybo_ker_scores (scopetype, scoperefid);

-------------------------------------------------------------------------------
-- 7. Corridor bands (safe/gold/hard) for Cyboquatic metrics
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_corridors (
    corridorid      INTEGER PRIMARY KEY AUTOINCREMENT,
    varid           TEXT NOT NULL UNIQUE,  -- e.g. "CYBO.ENERGY.JPC", "CYBO.CARBON.INTENSITY"
    safemin         REAL NOT NULL,
    goldmin         REAL NOT NULL,
    goldmax         REAL NOT NULL,
    hardmax         REAL NOT NULL,
    unit            TEXT,
    ecoplane        TEXT NOT NULL CHECK (
                         ecoplane IN ('energy','hydraulics','carbon','materials','biodiversity','dataquality')
                     ),
    weight          REAL NOT NULL,
    mandatory       INTEGER NOT NULL CHECK (mandatory IN (0,1)) DEFAULT 1,
    createdutc      TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc      TEXT NOT NULL DEFAULT (datetime('now')),
    CHECK (safemin <= goldmin AND goldmin <= goldmax AND goldmax <= hardmax)
);

CREATE INDEX IF NOT EXISTS idx_cybo_corridor_plane
    ON cybo_corridors (ecoplane);

-------------------------------------------------------------------------------
-- 8. Derived views: energy efficiency, carbon-negativity, eco-restorative score
-------------------------------------------------------------------------------

-- Per-node, per-plane performance window
CREATE VIEW IF NOT EXISTS v_cybo_workload_window AS
SELECT
    wl.nodeid,
    wl.ecoplane,
    MIN(wl.timestamputc)          AS window_start_utc,
    MAX(wl.timestamputc)          AS window_end_utc,
    SUM(wl.ereq_j)                AS total_req_j,
    SUM(wl.esurplus_j)            AS total_surplus_j,
    AVG(wl.vt_before)             AS mean_vt_before,
    AVG(wl.vt_after)              AS mean_vt_after,
    AVG(wl.vt_after - wl.vt_before) AS mean_delta_vt,
    AVG(wl.rplane)                AS mean_rplane,
    SUM(CASE WHEN wl.decision = 'ACCEPT' THEN wl.ereq_j ELSE 0 END) AS accepted_req_j,
    SUM(CASE WHEN wl.decision = 'REJECT' THEN wl.ereq_j ELSE 0 END) AS rejected_req_j,
    SUM(CASE WHEN wl.decision = 'REROUTE' THEN wl.ereq_j ELSE 0 END) AS rerouted_req_j,
    CAST(SUM(CASE WHEN wl.decision = 'ACCEPT' THEN 1 ELSE 0 END) AS REAL) /
    NULLIF(COUNT(*), 0)           AS accept_fraction,
    SUM(wl.carbon_kg)             AS total_carbon_kg,
    SUM(wl.carbon_offset_kg)      AS total_offset_kg
FROM cybo_workload_ledger wl
GROUP BY wl.nodeid, wl.ecoplane;

-- Energy efficiency and carbon-negative classification per node
CREATE VIEW IF NOT EXISTS v_cybo_energy_carbon_score AS
SELECT
    w.nodeid,
    COALESCE(SUM(CASE WHEN w.ecoplane = 'energy' THEN w.total_req_j END), 0.0) AS energy_req_j,
    COALESCE(SUM(CASE WHEN w.ecoplane = 'energy' THEN w.total_surplus_j END), 0.0) AS energy_surplus_j,
    COALESCE(SUM(w.total_carbon_kg), 0.0)     AS carbon_kg,
    COALESCE(SUM(w.total_offset_kg), 0.0)     AS offset_kg,
    CASE
        WHEN SUM(w.total_carbon_kg) IS NULL THEN 0
        WHEN SUM(w.total_carbon_kg) <= SUM(w.total_offset_kg) THEN 1
        ELSE 0
    END AS is_carbon_negative
FROM v_cybo_workload_window w
GROUP BY w.nodeid;

-- Eco-restorative "always-improve" surface per node:
-- high E, low R, non-increasing Vt and carbon-negative.
CREATE VIEW IF NOT EXISTS v_cybo_ecorestorative_score AS
SELECT
    n.nodeid,
    n.region,
    n.medium,
    ec.energy_req_j,
    ec.energy_surplus_j,
    ec.carbon_kg,
    ec.offset_kg,
    ec.is_carbon_negative,
    AVG(w.mean_delta_vt) AS avg_delta_vt,
    AVG(w.mean_rplane)   AS avg_rplane,
    -- simple heuristic: better if carbon-negative, Vt decreasing, risk low
    CASE
        WHEN ec.is_carbon_negative = 1
             AND AVG(w.mean_delta_vt) <= 0.0
             AND AVG(w.mean_rplane) <= 0.20
        THEN 1
        ELSE 0
    END AS is_ecorestorative_candidate
FROM cybo_nodes n
JOIN v_cybo_workload_window w ON w.nodeid = n.nodeid
JOIN v_cybo_energy_carbon_score ec ON ec.nodeid = n.nodeid
GROUP BY n.nodeid;

-------------------------------------------------------------------------------
-- 9. Seed rows for key repos and corridor defaults (optional scaffolding)
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO cybo_repositories (
    reponame, githubslug, roleband, lanedefault,
    primarylanguage, description, bostromdid,
    ecosafetybinding, shardprotocol
) VALUES (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'RESEARCH',
    'RESEARCH',
    'Rust',
    'Ecological restoration research, Cyboquatic systems, biodegradable substrates.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'ecosafety.corridors.v2',
    'EcoNetSchemaShard2026v1'
);

INSERT OR IGNORE INTO cybo_repositories (
    reponame, githubslug, roleband, lanedefault,
    primarylanguage, description, bostromdid,
    ecosafetybinding, shardprotocol
) VALUES (
    'EcoNet',
    'mk-bluebird/EcoNet',
    'SPINE',
    'PROD',
    'Rust',
    'Ecosafety spine and KER governance kernel for the EcoNet constellation.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'ecosafety.corridors.v2',
    'EcoNetSchemaShard2026v1'
);

INSERT OR IGNORE INTO cybo_repositories (
    reponame, githubslug, roleband, lanedefault,
    primarylanguage, description, bostromdid,
    ecosafetybinding, shardprotocol
) VALUES (
    'Cyboquatics',
    'mk-bluebird/Cyboquatics',
    'ENGINE',
    'PROD',
    'Rust',
    'Cyboquatic industrial machinery and routing kernels under EcoNet governance.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'ecosafety.corridors.v2',
    'EcoNetSchemaShard2026v1'
);

-- Example corridor bands for Cyboquatic-specific metrics.
INSERT OR IGNORE INTO cybo_corridors (
    varid, safemin, goldmin, goldmax, hardmax, unit, ecoplane, weight, mandatory
) VALUES
    ('CYBO.ENERGY.JPC',        0.0,    0.0,    1.0e6,  3.0e6, 'J/cycle',  'energy',     1.0, 1),
    ('CYBO.CARBON.INTENSITY',  0.0,    0.0,    0.30,   0.60,  'kg/kWh',   'carbon',     1.5, 1),
    ('CYBO.MATERIAL.RTOX',     0.0,    0.0,    0.10,   0.30,  '0..1',     'materials',  1.2, 1),
    ('CYBO.MATERIAL.RMICRO',   0.0,    0.0,    0.05,   0.20,  '0..1',     'materials',  1.0, 1),
    ('CYBO.MATERIAL.RPFAS',    0.0,    0.0,    0.10,   0.30,  '0..1',     'materials',  1.5, 1),
    ('CYBO.BIODIV.RISK',       0.0,    0.0,    0.10,   0.30,  '0..1',     'biodiversity', 1.5, 1);
