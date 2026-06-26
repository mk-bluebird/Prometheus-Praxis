-- filename: db/db_eco_restoration_index.sql
-- destination: eco_restoration_shard/db/db_eco_restoration_index.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Repo + file index (local to eco_restoration_shard)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS repo (
  repoid      INTEGER PRIMARY KEY AUTOINCREMENT,
  name        TEXT NOT NULL UNIQUE,
  roleband    TEXT NOT NULL,   -- e.g. RESTORATIONMONO, RESEARCH
  description TEXT NOT NULL,
  region      TEXT NOT NULL,   -- e.g. Phoenix-AZ, Global
  createdutc  TEXT NOT NULL,
  updatedutc  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS repofile (
  fileid      INTEGER PRIMARY KEY AUTOINCREMENT,
  repoid      INTEGER NOT NULL,
  relpath     TEXT NOT NULL,   -- path relative to repo root
  purpose     TEXT NOT NULL,   -- GOVERNANCEDB, SCHEMASQL, TOOLBIN, DOCSPEC, DATASHARD, etc.
  language    TEXT NOT NULL,   -- sqlite3, Rust, CPP, ALN, CSV, MD, PDF
  createdutc  TEXT NOT NULL,
  updatedutc  TEXT NOT NULL,
  UNIQUE (repoid, relpath),
  FOREIGN KEY (repoid) REFERENCES repo(repoid) ON DELETE CASCADE
);

INSERT OR IGNORE INTO repo (name, roleband, description, region, createdutc, updatedutc)
VALUES (
  'eco_restoration_shard',
  'RESTORATIONMONO',
  'Mono-repo for EcoNet eco-restoration and Cyboquatic machinery research; non-actuating, KER-scored, Phoenix-AZ anchored.',
  'Phoenix-AZ',
  datetime('now'),
  datetime('now')
);

INSERT OR IGNORE INTO repofile (repoid, relpath, purpose, language, createdutc, updatedutc)
SELECT r.repoid, 'db/db_eco_restoration_index.sql', 'SCHEMASQL', 'sqlite3', datetime('now'), datetime('now')
FROM repo AS r WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO repofile (repoid, relpath, purpose, language, createdutc, updatedutc)
SELECT r.repoid, 'db/eco_restoration_index.sqlite3', 'GOVERNANCEDB', 'sqlite3', datetime('now'), datetime('now')
FROM repo AS r WHERE r.name = 'eco_restoration_shard';

----------------------------------------------------------------------
-- 2. Bostrom identity + contracts (restoration-scoped)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS bostromaddress (
  addressid   INTEGER PRIMARY KEY AUTOINCREMENT,
  addresstext TEXT NOT NULL UNIQUE,
  label       TEXT NOT NULL,   -- PRIMARY, ALTERNATE, SAFEALT, ERC20
  description TEXT NOT NULL,
  createdutc  TEXT NOT NULL,
  updatedutc  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS restorationcontract (
  contractid  INTEGER PRIMARY KEY AUTOINCREMENT,
  logicalname TEXT NOT NULL,   -- e.g. eco_restoration.index.2026v1
  versiontag  TEXT NOT NULL,   -- e.g. 2026v1
  description TEXT NOT NULL,
  region      TEXT NOT NULL,
  status      TEXT NOT NULL,   -- FROZENACTIVE, FROZENDEPRECATED, EXPERIMENTAL
  createdutc  TEXT NOT NULL,
  updatedutc  TEXT NOT NULL,
  UNIQUE (logicalname, versiontag, region)
);

CREATE TABLE IF NOT EXISTS bostromcontractbinding (
  bindingid   INTEGER PRIMARY KEY AUTOINCREMENT,
  addressid   INTEGER NOT NULL,
  contractid  INTEGER NOT NULL,
  role        TEXT NOT NULL,   -- AUTHOR, STEWARD, FUNDER, REVIEWER
  evidencehex TEXT NOT NULL,
  createdutc  TEXT NOT NULL,
  UNIQUE (addressid, contractid, role),
  FOREIGN KEY (addressid) REFERENCES bostromaddress(addressid) ON DELETE CASCADE,
  FOREIGN KEY (contractid) REFERENCES restorationcontract(contractid) ON DELETE CASCADE
);

INSERT OR IGNORE INTO bostromaddress (addresstext, label, description, createdutc, updatedutc)
VALUES
  ('bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', 'PRIMARY',
   'Primary Bostrom address for eco-restoration and Cyboquatic governance.', datetime('now'), datetime('now')),
  ('bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc', 'ALTERNATE',
   'Alternate, secure, Google-linked Bostrom address (active RT monitoring).', datetime('now'), datetime('now')),
  ('zeta12x0up66pzyeretzyku8p4ccuxrjqtqpdc4y4x8', 'SAFEALT',
   'Safe alternate address for eco-restoration shards.', datetime('now'), datetime('now')),
  ('0x519fC0eB4111323Cac44b70e1aE31c30e405802D', 'ERC20',
   'ERC-20-compatible address for eco-restoration tokens and contracts.', datetime('now'), datetime('now'));

INSERT OR IGNORE INTO restorationcontract (logicalname, versiontag, description, region, status, createdutc, updatedutc)
VALUES
  ('eco_restoration.index.2026v1', '2026v1',
   'Index + blastradius contract for eco_restoration_shard.', 'Phoenix-AZ', 'FROZENACTIVE', datetime('now'), datetime('now')),
  ('eco_restoration.blastradius.2026v1', '2026v1',
   'Blast-radius and restoration-radius grammar for Cyboquatic + substrate nodes.', 'Phoenix-AZ',
   'EXPERIMENTAL', datetime('now'), datetime('now')),
  ('eco_restoration.energycarbon.ledger.2026v1', '2026v1',
   'Energy and carbon ledger contract for Cyboquatic eco-restoration workloads.', 'Phoenix-AZ',
   'EXPERIMENTAL', datetime('now'), datetime('now'));

INSERT OR IGNORE INTO bostromcontractbinding (addressid, contractid, role, evidencehex, createdutc)
SELECT a.addressid, c.contractid, 'AUTHOR',
       '0000000000000000000000000000000000000000000000000000000000000000',
       datetime('now')
FROM bostromaddress AS a
JOIN restorationcontract AS c
WHERE a.addresstext = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
  AND c.logicalname IN (
    'eco_restoration.index.2026v1',
    'eco_restoration.blastradius.2026v1',
    'eco_restoration.energycarbon.ledger.2026v1'
  );

----------------------------------------------------------------------
-- 3. Shardinstance, eco K/E/R, and knowledge factors
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS shardinstance (
  shardid      INTEGER PRIMARY KEY AUTOINCREMENT,
  repofileid   INTEGER NOT NULL,      -- FK to repofile
  particle     TEXT NOT NULL,         -- ALN particle name
  nodeid       TEXT NOT NULL,
  assettype    TEXT NOT NULL,         -- e.g. FogRouterCluster, SubstrateBatch
  medium       TEXT NOT NULL,         -- water, material, air, soil, bio
  region       TEXT NOT NULL,         -- Phoenix-AZ etc.
  lane         TEXT NOT NULL,         -- RESEARCH, EXPPROD, PROD
  tstartutc    TEXT NOT NULL,
  tendutc      TEXT NOT NULL,
  kmetric      REAL NOT NULL,         -- windowed K
  emetric      REAL NOT NULL,         -- windowed E
  rmetric      REAL NOT NULL,         -- windowed R
  vtmax        REAL NOT NULL,         -- max Vt over window
  kerdeployable INTEGER NOT NULL,     -- 0/1
  evidencehex  TEXT NOT NULL,
  signingdid   TEXT NOT NULL,
  createdutc   TEXT NOT NULL,
  updatedutc   TEXT NOT NULL,
  FOREIGN KEY (repofileid) REFERENCES repofile(fileid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_shard_node_lane
  ON shardinstance(nodeid, lane);

CREATE INDEX IF NOT EXISTS idx_shard_region_medium
  ON shardinstance(region, medium);

CREATE TABLE IF NOT EXISTS knowledgeecoscore (
  scoreid    INTEGER PRIMARY KEY AUTOINCREMENT,
  scopetype  TEXT NOT NULL,     -- SHARD, REPO, MATERIAL
  scoperefid INTEGER NOT NULL,  -- shardid, repoid, etc.
  kfactor    REAL NOT NULL,     -- knowledge factor
  efactor    REAL NOT NULL,     -- eco-impact factor
  rfactor    REAL NOT NULL,     -- risk-of-harm factor
  createdutc TEXT NOT NULL,
  updatedutc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_knowledge_scope
  ON knowledgeecoscore(scopetype, scoperefid);

----------------------------------------------------------------------
-- 4. Blast-radius and restoration impact metadata
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blastradiuslink (
  linkid       INTEGER PRIMARY KEY AUTOINCREMENT,
  sourcetype   TEXT NOT NULL CHECK (sourcetype IN ('REPO','SCHEMA','PARTICLE','SHARD','FILE')),
  sourceid     INTEGER NOT NULL,
  targettype   TEXT NOT NULL CHECK (targettype IN ('NODE','SHARD','MACHINE','MATERIAL','REGION')),
  targetid     TEXT NOT NULL,
  impacttype   TEXT NOT NULL,   -- HYDRAULIC,ENERGY,CARBON,BIODIVERSITY,MATERIAL,DATAQUALITY,GOVERNANCE
  impactscore  REAL NOT NULL,   -- 0..1 fraction of corridor width influenced
  vtsensitivity REAL,           -- delta-Vt per unit perturbation (dimensionless)
  notes        TEXT
);

CREATE INDEX IF NOT EXISTS idx_blastradius_source
  ON blastradiuslink(sourcetype, sourceid, impacttype);

CREATE INDEX IF NOT EXISTS idx_blastradius_target
  ON blastradiuslink(targettype, targetid, impacttype);

----------------------------------------------------------------------
-- 5. Cyboquatic energy + carbon ledger (non-actuating)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS workloadledger (
  ledgerid     INTEGER PRIMARY KEY AUTOINCREMENT,
  shardid      INTEGER NOT NULL REFERENCES shardinstance(shardid) ON DELETE CASCADE,
  variantid    TEXT NOT NULL,   -- CyboVariant id or ALN variant particle
  nodeid       TEXT NOT NULL,
  channel      TEXT NOT NULL CHECK (channel IN ('energy','carbon','materials','biodiversity')),
  ereqj        REAL NOT NULL,   -- requested energy (joules)
  esurplusj    REAL NOT NULL,   -- surplus energy at dispatch (joules)
  rcarbon      REAL,            -- normalized carbon risk 0..1
  rbiodiv      REAL,            -- normalized biodiversity risk 0..1
  vtbefore     REAL NOT NULL,
  vtafter      REAL NOT NULL,
  decision     TEXT NOT NULL CHECK (decision IN ('ACCEPT','REJECT','REROUTE')),
  timestamputc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_workload_node_time
  ON workloadledger(nodeid, timestamputc);

CREATE INDEX IF NOT EXISTS idx_workload_shard_channel
  ON workloadledger(shardid, channel);

----------------------------------------------------------------------
-- 6. Optional: lightweight eco corridor reference (summary only)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ecocorridorvar (
  varid       TEXT PRIMARY KEY,
  lyapchannel TEXT NOT NULL,
  safelo      REAL NOT NULL,
  safehi      REAL NOT NULL,
  goldlo      REAL NOT NULL,
  goldhi      REAL NOT NULL,
  hardlo      REAL NOT NULL,
  hardhi      REAL NOT NULL,
  weight      REAL NOT NULL
);
