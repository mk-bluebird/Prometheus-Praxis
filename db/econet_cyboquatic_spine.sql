-- filename: db/econet_cyboquatic_spine.sql
-- destination: eco_restoration_shard/db/econet_cyboquatic_spine.sql
-- target-repo: github.com/mk-bluebird/eco_restoration_shard
--
-- Purpose
-- - Provide a non-actuating SQLite spine for Cyboquatic eco-machinery inside EcoNet.
-- - Encode blast-radius surfaces, workload ledgers, and always-improve KER views
--   for energy-efficient, carbon-negative, ecologically-restorative Cyboquatic nodes.
-- - Integrate with existing econetrepoindex/econetlayer/econetrolehint grammar and
--   Phoenix Cyboquatic evidence tables already sketched in prior research.

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 0. Canonical econetfileindex entry for this spine
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS econetfileindex (
  fileindexid   INTEGER PRIMARY KEY AUTOINCREMENT,
  filename      TEXT NOT NULL,
  destination   TEXT NOT NULL,
  repotarget    TEXT NOT NULL, -- e.g. EcoNet, Eco-Fort, eco_restoration_shard
  roleband      TEXT NOT NULL, -- SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP
  lanedefault   TEXT NOT NULL, -- RESEARCH, EXPPROD, PROD
  description   TEXT NOT NULL,
  createdutc    TEXT NOT NULL,
  updatedutc    TEXT NOT NULL,
  CHECK (roleband IN ('SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP')),
  CHECK (lanedefault IN ('RESEARCH','EXPPROD','PROD')),
  UNIQUE (filename, destination, repotarget)
);

INSERT OR IGNORE INTO econetfileindex
  (filename,
   destination,
   repotarget,
   roleband,
   lanedefault,
   description,
   createdutc,
   updatedutc)
VALUES
  ('econet_cyboquatic_spine.sql',
   'eco_restoration_shard/db/econet_cyboquatic_spine.sql',
   'eco_restoration_shard',
   'SPINE',
   'RESEARCH',
   'Non-actuating Cyboquatic blast-radius and workload spine for EcoNet KER diagnostics.',
   datetime('now'),
   datetime('now'));

-------------------------------------------------------------------------------
-- 1. Blast-radius link table (non-actuating impact surfaces)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blastradiuslink (
  linkid        INTEGER PRIMARY KEY AUTOINCREMENT,
  sourcetype    TEXT NOT NULL CHECK (
                  sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','NODE')
                ),
  sourceid      TEXT NOT NULL,
  targettype    TEXT NOT NULL CHECK (
                  targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')
                ),
  targetid      TEXT NOT NULL,
  impacttype    TEXT NOT NULL, -- ENERGY, CARBON, MATERIALS, BIODIVERSITY, DATAQUALITY, HYDRAULICS
  impactscore   REAL NOT NULL, -- 0..1 normalized fraction of corridor influence
  vtsensitivity REAL,          -- approximate delta Vt for diagnostics (can be NULL)
  notes         TEXT
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source
  ON blastradiuslink (sourcetype, sourceid, impacttype);

CREATE INDEX IF NOT EXISTS idx_blastradius_target
  ON blastradiuslink (targettype, targetid, impacttype);

-------------------------------------------------------------------------------
-- 2. Cyboquatic workload ledger (diagnostic-only energy/carbon/material/bio)
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboworkloadledger (
  ledgerid      INTEGER PRIMARY KEY AUTOINCREMENT,
  shardid       TEXT NOT NULL,
  variantid     TEXT NOT NULL, -- ALN particle or routing kernel version
  nodeid        TEXT NOT NULL,
  channel       TEXT NOT NULL CHECK (
                  channel IN ('energy','carbon','materials','biodiversity')
                ),
  ereqj         REAL NOT NULL, -- requested energy J (or equivalent scalar)
  esurplusj     REAL NOT NULL, -- surplus/harvested energy J (tailwind)
  rcarbon       REAL,          -- 0..1 carbon risk coordinate
  rbiodiv       REAL,          -- 0..1 biodiversity risk coordinate
  vtbefore      REAL NOT NULL, -- Lyapunov residual before workload
  vtafter       REAL NOT NULL, -- Lyapunov residual after workload
  decision      TEXT NOT NULL CHECK (
                  decision IN ('ACCEPT','REJECT','REROUTE')
                ),
  timestamputc  TEXT NOT NULL  -- ISO8601 UTC timestamp
);

CREATE INDEX IF NOT EXISTS idx_cyboworkload_nodetime
  ON cyboworkloadledger (nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_cyboworkload_shard
  ON cyboworkloadledger (shardid, channel);

-------------------------------------------------------------------------------
-- 3. Always-improve diagnostic views for KER and cost-of-energy reduction
-------------------------------------------------------------------------------

-- View: blast-radius summary per node, by impact type.
CREATE VIEW IF NOT EXISTS vcybo_blastradius_per_node AS
SELECT
  sourceid              AS nodeid,
  impacttype,
  SUM(impactscore)      AS impactscoresum,
  AVG(COALESCE(vtsensitivity, 0.0)) AS vtsensitivity_mean,
  COUNT(*)              AS linkcount
FROM blastradiuslink
WHERE sourcetype = 'NODE'
GROUP BY sourceid, impacttype;

-- View: workload window summaries per node and channel.
-- This is for diagnostics only; it does not control any routing.
CREATE VIEW IF NOT EXISTS vcybo_workload_window AS
SELECT
  nodeid,
  channel,
  MIN(timestamputc)                                    AS window_start_utc,
  MAX(timestamputc)                                    AS window_end_utc,
  SUM(ereqj)                                           AS total_requests_j,
  SUM(esurplusj)                                       AS total_surplus_j,
  SUM(CASE WHEN decision = 'ACCEPT' THEN ereqj ELSE 0 END) AS accepted_requests_j,
  SUM(CASE WHEN decision = 'REJECT' THEN ereqj ELSE 0 END) AS rejected_requests_j,
  SUM(CASE WHEN decision = 'REROUTE' THEN ereqj ELSE 0 END) AS rerouted_requests_j,
  AVG(vtbefore)                                        AS mean_vt_before,
  AVG(vtafter)                                         AS mean_vt_after,
  AVG(vtafter - vtbefore)                              AS mean_delta_vt,
  AVG(rcarbon)                                         AS mean_rcarbon,
  AVG(rbiodiv)                                         AS mean_rbiodiv
FROM cyboworkloadledger
GROUP BY nodeid, channel;

-- View: cost-of-energy efficiency surface.
-- Captures how Cyboquatic workloads reduce net energy cost while keeping Vt stable or improving.
CREATE VIEW IF NOT EXISTS vcybo_energy_efficiency AS
SELECT
  nodeid,
  channel,
  total_requests_j,
  total_surplus_j,
  CASE
    WHEN total_requests_j > 0
    THEN total_surplus_j / total_requests_j
    ELSE 0.0
  END AS surplus_fraction,
  mean_vt_before,
  mean_vt_after,
  mean_delta_vt
FROM vcybo_workload_window
WHERE channel = 'energy';

-------------------------------------------------------------------------------
-- 4. Seed rows for Phoenix Cyboquatic MAR/FOG pilots (non-actuating evidence)
-------------------------------------------------------------------------------

-- Blast-radius links for Phoenix Cyboquatic MAR and FOG nodes.
DELETE FROM blastradiuslink
WHERE sourcetype = 'NODE'
  AND sourceid LIKE 'PHX-CYBOQ-%';

INSERT INTO blastradiuslink
  (sourcetype, sourceid, targettype, targetid,
   impacttype, impactscore, vtsensitivity, notes)
VALUES
  ('NODE','PHX-CYBOQ-MAR-001','REGION','Phoenix-AZ-Mar-West',
   'HYDRAULICS',0.32,0.08,'MAR node 001 hydraulic buffer influence for Phoenix West basin.'),
  ('NODE','PHX-CYBOQ-MAR-001','MATERIAL','BioSubstrateBatch-2026-03A',
   'MATERIALS',0.27,0.05,'Biodegradable tray batch 2026-03A leachate and micro-residue corridor.'),
  ('NODE','PHX-CYBOQ-MAR-001','REGION','Phoenix-AZ-Gila-Ecoli',
   'BIODIVERSITY',0.21,0.07,'E. coli buffering corridor for Gila reach under Cyboquatic MAR 001.'),
  ('NODE','PHX-CYBOQ-MAR-002','REGION','Phoenix-AZ-PFBS',
   'CARBON',0.29,0.06,'PFBS adsorption channel carbon plane with embodied substrate carbon and pump energy.'),
  ('NODE','PHX-CYBOQ-MAR-002','MATERIAL','BioSubstrateBatch-2026-03B',
   'MATERIALS',0.31,0.09,'Alternate biodegradable blend with higher micro-residue risk weight.'),
  ('NODE','PHX-CYBOQ-CHANNEL-FOG-01','REGION','Phoenix-AZ-Sewer-FOG',
   'HYDRAULICS',0.35,0.11,'Cyboquatic sewer FOG channel surcharge and fouling corridor for pilot interceptors.'),
  ('NODE','PHX-CYBOQ-CHANNEL-FOG-01','MATERIAL','BioSubstrateBatch-2026-02C',
   'MATERIALS',0.22,0.04,'Lower-mass biodegradable baffles moderating leachate risk with high FOG capture.');

-- Cyboquatic workload ledger evidence for Phoenix pilots.
DELETE FROM cyboworkloadledger
WHERE nodeid LIKE 'PHX-CYBOQ-%';

INSERT INTO cyboworkloadledger
  (shardid, variantid, nodeid, channel,
   ereqj, esurplusj, rcarbon, rbiodiv,
   vtbefore, vtafter, decision, timestamputc)
VALUES
  ('HydrologicalBufferPhoenix2026v1:PHX-MAR-001-2026-03-10T00:00Z',
   'CyboRoute-v1','PHX-CYBOQ-MAR-001','energy',
   8.5e5,3.1e5,0.18,NULL,
   0.42,0.39,'ACCEPT','2026-03-10T00:05:00Z'),
  ('HydrologicalBufferPhoenix2026v1:PHX-MAR-001-2026-03-10T00:00Z',
   'CyboRoute-v1','PHX-CYBOQ-MAR-001','carbon',
   8.5e5,3.1e5,0.19,NULL,
   0.39,0.38,'ACCEPT','2026-03-10T00:05:00Z'),
  ('PhoenixMarShard.v2:PFBS-Channel-2026-03-14',
   'CyboRoute-v1','PHX-CYBOQ-MAR-002','energy',
   9.2e5,2.0e5,0.21,NULL,
   0.47,0.46,'ACCEPT','2026-03-14T01:02:00Z'),
  ('PhoenixMarShard.v2:PFBS-Channel-2026-03-14',
   'CyboRoute-v1','PHX-CYBOQ-MAR-002','carbon',
   9.2e5,2.0e5,0.23,NULL,
   0.46,0.46,'REROUTE','2026-03-14T01:02:00Z'),
  ('BiodegradableSubstrateChannelKinetics2026v1:Batch-2026-03A',
   'MaterialKinetics-v1','PHX-CYBOQ-MAR-001','materials',
   0.0,0.0,0.17,0.09,
   0.33,0.31,'ACCEPT','2026-03-09T12:30:00Z'),
  ('BiodegradableSubstrateChannelKinetics2026v1:Batch-2026-03B',
   'MaterialKinetics-v1','PHX-CYBOQ-MAR-002','materials',
   0.0,0.0,0.23,0.11,
   0.34,0.35,'REJECT','2026-03-09T12:35:00Z'),
  ('BiodiversityRawPhoenix2026v1:Mar-Reach-Gila',
   'BiodiversityFrame-v1','PHX-CYBOQ-MAR-001','biodiversity',
   0.0,0.0,NULL,0.14,
   0.29,0.27,'ACCEPT','2026-03-20T08:00:00Z');
