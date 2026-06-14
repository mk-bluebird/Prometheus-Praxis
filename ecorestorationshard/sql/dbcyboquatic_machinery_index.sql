-- filename: dbcyboquatic_machinery_index.sql
-- destination: ecorestorationshard/sql/dbcyboquatic_machinery_index.sql
PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Cyboquatic node registry (logical eco-machinery units)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_node (
    nodeid              TEXT PRIMARY KEY,
    -- short human-readable descriptor, e.g. "PHX-CYBO-NODE-01-FLOWVAC"
    displayname         TEXT NOT NULL,
    -- region code, e.g. "Phoenix-AZ"
    region              TEXT NOT NULL,
    -- physical medium primarily influenced by this node: water, soil, air, bio
    medium              TEXT NOT NULL,
    -- high-level role: CHANNEL, MAR_VAULT, FILTRATION, ENERGY_BANK, CONTROL
    noderole            TEXT NOT NULL CHECK (
        noderole IN ('CHANNEL','MAR_VAULT','FILTRATION','ENERGY_BANK','CONTROL')
    ),
    -- Biodegradable / composable machinery class (e.g. FLOWVAC, BIOCHANNEL, SUBSTRATE_TRAY)
    machineryclass      TEXT NOT NULL,
    -- correlation to shardinstance.nodeid where KER windows live
    shard_node_ref      TEXT,
    -- governance DID anchoring authorship/ownership
    stewarddid          TEXT NOT NULL,
    -- lane hint for planning: RESEARCH, EXPPROD, PROD
    lanehint            TEXT NOT NULL CHECK (
        lanehint IN ('RESEARCH','EXPPROD','PROD')
    ),
    created_utc         TEXT NOT NULL DEFAULT strftime('%Y-%m-%dT%H:%M:%SZ','now'),
    updated_utc         TEXT NOT NULL DEFAULT strftime('%Y-%m-%dT%H:%M:%SZ','now')
);

CREATE TRIGGER IF NOT EXISTS trg_cybo_node_updated
AFTER UPDATE ON cybo_node
BEGIN
    UPDATE cybo_node
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE nodeid = NEW.nodeid;
END;

CREATE INDEX IF NOT EXISTS idx_cybo_node_region
    ON cybo_node(region, medium, noderole);

----------------------------------------------------------------------
-- 2. Biodegradable substrate batches used by Cyboquatic nodes
--    (links to qpudatashards like CyboSubstrateFlowVac2026v1.aln)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_substrate_batch (
    batchid             TEXT PRIMARY KEY,
    -- e.g. FLOWVAC-BATCH-2026-01
    machineryclass      TEXT NOT NULL,
    material_family     TEXT NOT NULL,
    -- manufacturing or deployment region
    region              TEXT NOT NULL,
    -- deployment status: PLANNED, DEPLOYED, RETIRED
    status              TEXT NOT NULL CHECK (
        status IN ('PLANNED','DEPLOYED','RETIRED')
    ),
    -- normalized 0..1 risk coordinates from materials ecosafety shard
    rmassloss           REAL NOT NULL CHECK (rmassloss >= 0.0 AND rmassloss <= 1.0),
    rtox                REAL NOT NULL CHECK (rtox      >= 0.0 AND rtox      <= 1.0),
    rmicro              REAL NOT NULL CHECK (rmicro    >= 0.0 AND rmicro    <= 1.0),
    rcarbon             REAL NOT NULL CHECK (rcarbon   >= 0.0 AND rcarbon   <= 1.0),
    rbiodiv             REAL NOT NULL CHECK (rbiodiv   >= 0.0 AND rbiodiv   <= 1.0),
    -- eco-impact score band 0..1 (benefit direction)
    ecoimpactscore      REAL NOT NULL CHECK (ecoimpactscore >= 0.0 AND ecoimpactscore <= 1.0),
    -- foreign key into shardinstance.shardid where metrics originated
    shardid             INTEGER,
    evidencehex         TEXT,
    signingdid          TEXT NOT NULL,
    created_utc         TEXT NOT NULL DEFAULT strftime('%Y-%m-%dT%H:%M:%SZ','now'),
    updated_utc         TEXT NOT NULL DEFAULT strftime('%Y-%m-%dT%H:%M:%SZ','now')
);

CREATE TRIGGER IF NOT EXISTS trg_cybo_substrate_batch_updated
AFTER UPDATE ON cybo_substrate_batch
BEGIN
    UPDATE cybo_substrate_batch
    SET updated_utc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
    WHERE batchid = NEW.batchid;
END;

CREATE INDEX IF NOT EXISTS idx_cybo_substrate_region
    ON cybo_substrate_batch(region, machineryclass, status);

----------------------------------------------------------------------
-- 3. Node–substrate linkage (many-to-many mapping)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cybo_node_substrate (
    nodeid              TEXT NOT NULL REFERENCES cybo_node(nodeid) ON DELETE CASCADE,
    batchid             TEXT NOT NULL REFERENCES cybo_substrate_batch(batchid) ON DELETE CASCADE,
    role                TEXT NOT NULL CHECK (
        role IN ('PRIMARY_CHANNEL','LINER','VAULT_FILL','TRAY_MEDIA')
    ),
    installed_utc       TEXT NOT NULL,
    removed_utc         TEXT,
    PRIMARY KEY (nodeid, batchid, role)
);

CREATE INDEX IF NOT EXISTS idx_cybo_node_substrate_batch
    ON cybo_node_substrate(batchid);

----------------------------------------------------------------------
-- 4. Cyboquatic eco-metrics view (join with blastradiuslink & workload)
--    Requires existing tables: shardinstance, blastradiuslink,
--    cyboworkloadledger as created in previous spine migration work.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS vcybo_node_eco_metrics AS
SELECT
    n.nodeid,
    n.displayname,
    n.region,
    n.medium,
    n.noderole,
    n.machineryclass,
    -- windowed workload trends from vcyboworkloadwindow
    w.windowstartutc,
    w.windowendutc,
    w.totalrequestsj,
    w.totalsurplusj,
    w.acceptfraction,
    w.meanvtbefore,
    w.meanvtafter,
    w.meandeltavt,
    w.meanrcarbon,
    w.meanrbiodiv,
    -- summarized blast radius impacts (per impact type)
    br.impacttype,
    br.impactscoresum,
    br.vtsensitivitymean,
    br.linkcount
FROM cybo_node AS n
LEFT JOIN vcyboworkloadwindow AS w
    ON w.nodeid = n.nodeid
LEFT JOIN vshardblastradius AS br
    ON br.nodeid = n.shard_node_ref;

----------------------------------------------------------------------
-- 5. Seed rows (example Cyboquatic node & biodegradable batch for CI)
--    Aligns with prior seeds PHX-CYBO-NODE-01 and FLOWVAC-BATCH-2026-01.
----------------------------------------------------------------------

INSERT OR IGNORE INTO cybo_node (
    nodeid, displayname, region, medium, noderole,
    machineryclass, shard_node_ref, stewarddid, lanehint
) VALUES (
    'PHX-CYBO-NODE-01',
    'Phoenix Cyboquatic FlowVac Node 01',
    'Phoenix-AZ',
    'water',
    'CHANNEL',
    'FLOWVAC',
    'PHX-CYBO-NODE-01',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'EXPPROD'
);

INSERT OR IGNORE INTO cybo_substrate_batch (
    batchid, machineryclass, material_family, region, status,
    rmassloss, rtox, rmicro, rcarbon, rbiodiv, ecoimpactscore,
    shardid, evidencehex, signingdid
) VALUES (
    'FLOWVAC-BATCH-2026-01',
    'FLOWVAC',
    'BIODEGRADABLE_POLYMER_V1',
    'Phoenix-AZ',
    'DEPLOYED',
    0.20,  -- mass loss risk
    0.10,  -- toxicity risk
    0.15,  -- micro-residue risk
    0.05,  -- carbon risk (non-offsettable plane)
    0.08,  -- biodiversity risk
    0.92,  -- eco-impact benefit band
    NULL,
    'a1b2c3d4e5f67890',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

INSERT OR IGNORE INTO cybo_node_substrate (
    nodeid, batchid, role, installed_utc, removed_utc
) VALUES (
    'PHX-CYBO-NODE-01',
    'FLOWVAC-BATCH-2026-01',
    'PRIMARY_CHANNEL',
    '2026-01-01T00:00:00Z',
    NULL
);
