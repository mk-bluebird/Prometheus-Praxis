-- filename: dbecorewardgovernanceecokarma.sql
-- destination: ecorestorationshard/sql/governance/dbecorewardgovernanceecokarma.sql
PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Risk planes, capability floors, time-decay, and trust-plane residual
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS riskplane (
    planeid               INTEGER PRIMARY KEY AUTOINCREMENT,
    planename             TEXT NOT NULL UNIQUE, -- e.g. energy, carbon, biodiversity, topology, healthcarebiomass
    -- Non-offsettable planes cannot be compensated by other planes when computing residual.
    nonoffsettable        INTEGER NOT NULL DEFAULT 0 CHECK (nonoffsettable IN (0,1)),
    -- Capability floor minimum acceptable capability 0..1 for this plane.
    capabilityfloor       REAL NOT NULL DEFAULT 0.0,
    -- Hard lower bound for the floor prevents dilution over time.
    capabilityfloorhardmin REAL NOT NULL DEFAULT 0.0,
    -- Optional description.
    description           TEXT
);

CREATE INDEX IF NOT EXISTS idx_riskplane_nonoffsettable
    ON riskplane (nonoffsettable);

-- Capability floor history per plane and epoch to prevent dilution.
CREATE TABLE IF NOT EXISTS capabilityfloorhistory (
    floorid        INTEGER PRIMARY KEY AUTOINCREMENT,
    planeid        INTEGER NOT NULL REFERENCES riskplane(planeid) ON DELETE CASCADE,
    evolutionepochid INTEGER NOT NULL REFERENCES evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    -- Floor value for this plane and epoch 0..1.
    floorvalue     REAL NOT NULL,
    -- Flag indicating this row passed the anti-dilution check in CI (monotone non-decreasing).
    monotoneok     INTEGER NOT NULL DEFAULT 0 CHECK (monotoneok IN (0,1)),
    createdutc     TEXT NOT NULL,
    UNIQUE (planeid, evolutionepochid)
);

CREATE INDEX IF NOT EXISTS idx_capabilityfloor_plane_epoch
    ON capabilityfloorhistory (planeid, evolutionepochid);

-- Lyapunov residual snapshots including trust plane rtrust and time-decayed residual.
CREATE TABLE IF NOT EXISTS kerresidualsnapshot (
    snapshotid      INTEGER PRIMARY KEY AUTOINCREMENT,
    region          TEXT NOT NULL, -- e.g. Phoenix-AZ-US
    kernelid        TEXT NOT NULL, -- logical kernel identity
    epoch           INTEGER NOT NULL, -- evolution epoch at time of snapshot
    -- Aggregate KER for the window.
    kmean           REAL NOT NULL,
    emean           REAL NOT NULL,
    rmean           REAL NOT NULL,
    -- Plane-specific risk coordinates 0..1.
    renergy         REAL NOT NULL,
    rcarbon         REAL NOT NULL,
    rbiodiversity   REAL NOT NULL,
    rtopology       REAL NOT NULL,
    rtrust          REAL NOT NULL, -- trust-plane coordinate 0..1
    -- Time-decay effective residual reff with monotone non-increase constraint enforced in Rust.
    reff            REAL NOT NULL,
    windowstartutc  TEXT NOT NULL,
    windowendutc    TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_kerresidual_kernel_epoch
    ON kerresidualsnapshot (kernelid, epoch, region);

-------------------------------------------------------------------------------
-- 2. Evolution epochs and fork-binding
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS evolutionepoch (
    evolutionepochid INTEGER PRIMARY KEY AUTOINCREMENT,
    epochlabel       TEXT NOT NULL UNIQUE, -- e.g. PhoenixWater2026Q2
    -- Fork-binding identifies the policy fork this epoch belongs to (git tag, ALN policy id).
    forkid           TEXT NOT NULL,
    parentepochid    INTEGER REFERENCES evolutionepoch(evolutionepochid),
    -- Effective from (inclusive) and until (exclusive, NULL means open-ended).
    effectivefromutc TEXT NOT NULL,
    effectiveuntilutc TEXT,
    -- Notes about what changed: KE floors, reward split rules, etc.
    notes            TEXT
);

CREATE INDEX IF NOT EXISTS idx_evolutionepoch_fork
    ON evolutionepoch (forkid);

-------------------------------------------------------------------------------
-- 3. Boot reward audit and eco-split with responsibility deltas
-------------------------------------------------------------------------------

-- Boot reward events (initial eco-bootstrapping rewards).
CREATE TABLE IF NOT EXISTS bootrewardevent (
    bootrewardid    INTEGER PRIMARY KEY AUTOINCREMENT,
    evolutionepochid INTEGER NOT NULL REFERENCES evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    -- Actor portfolio identifier e.g. DID, Bostrom address.
    actordid        TEXT NOT NULL,
    -- Baseline responsibility metrics at boot time 0..1.
    rbaseline       REAL NOT NULL,
    kbaseline       REAL NOT NULL,
    ebaseline       REAL NOT NULL,
    -- Tokens awarded in this boot event.
    tokensawarded   REAL NOT NULL,
    -- Eco-split fractions sum ~1.0 between actors/planes (JSON mapping destination→fraction).
    ecosplitjson    TEXT NOT NULL,
    -- Responsibility delta target in subsequent audit windows (target residual reduction ≥ 0).
    rdeltatarget    REAL NOT NULL,
    -- Whether this boot reward is fully audited and closed.
    auditedclosed   INTEGER NOT NULL DEFAULT 0 CHECK (auditedclosed IN (0,1)),
    createdutc      TEXT NOT NULL,
    auditedutc      TEXT
);

CREATE INDEX IF NOT EXISTS idx_bootreward_actor_epoch
    ON bootrewardevent (actordid, evolutionepochid);

-- Responsibility delta audits per boot reward.
CREATE TABLE IF NOT EXISTS bootrewardresponsibilityaudit (
    auditid             INTEGER PRIMARY KEY AUTOINCREMENT,
    bootrewardid        INTEGER NOT NULL REFERENCES bootrewardevent(bootrewardid) ON DELETE CASCADE,
    auditwindowstartutc TEXT NOT NULL,
    auditwindowendutc   TEXT NOT NULL,
    -- Observed responsibility residual metrics after boot.
    robserved           REAL NOT NULL,
    -- Delta relative to baseline (positive means improvement).
    rdeltaobserved      REAL NOT NULL,
    -- Whether minimum responsibility improvement target was met.
    targetmet           INTEGER NOT NULL CHECK (targetmet IN (0,1)),
    -- Snapshot of trust-plane coordinate at audit time.
    rtrustobserved      REAL NOT NULL,
    createdutc          TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_bootreward_audit_brid
    ON bootrewardresponsibilityaudit (bootrewardid);

-------------------------------------------------------------------------------
-- 4. Quadratic oracle staking for ECOREPAIR_KARMA_DRAIN
-------------------------------------------------------------------------------

-- Staking pools for eco repair karma drain.
CREATE TABLE IF NOT EXISTS ecorepairstakepool (
    poolid             INTEGER PRIMARY KEY AUTOINCREMENT,
    poolname           TEXT NOT NULL UNIQUE, -- e.g. ECOREPAIR_KARMA_DRAIN
    evolutionepochid   INTEGER NOT NULL REFERENCES evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    -- Total stake (linear sum) for informational purposes.
    totalstake         REAL NOT NULL DEFAULT 0.0,
    -- Quadratic-weighted effective stake, recomputed by Rust oracle.
    totaleffectivestake REAL NOT NULL DEFAULT 0.0,
    createdutc         TEXT NOT NULL,
    updatedutc         TEXT NOT NULL
);

-- Individual stakes.
CREATE TABLE IF NOT EXISTS ecorepairstake (
    stakeid      INTEGER PRIMARY KEY AUTOINCREMENT,
    poolid       INTEGER NOT NULL REFERENCES ecorepairstakepool(poolid) ON DELETE CASCADE,
    actordid     TEXT NOT NULL,
    -- Raw stake amount.
    stakeamount  REAL NOT NULL,
    -- Quadratic oracle weight (sqrt-based or other monotone function).
    effectiveweight REAL NOT NULL,
    -- Karma drain repair direction: +1 repair, -1 drain.
    direction    INTEGER NOT NULL CHECK (direction IN (-1, 1)),
    createdutc   TEXT NOT NULL
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_ecorepairstake_actor_pool
    ON ecorepairstake (poolid, actordid);

-------------------------------------------------------------------------------
-- 5. Brain-bound one-time delegations
-------------------------------------------------------------------------------

-- One-time delegations for eco governance and staking (brain-bound, non-replayable).
CREATE TABLE IF NOT EXISTS brainbounddelegation (
    delegationid    INTEGER PRIMARY KEY AUTOINCREMENT,
    delegatordid    TEXT NOT NULL,
    delegateedid    TEXT NOT NULL,
    evolutionepochid INTEGER NOT NULL REFERENCES evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    -- Scope of delegation, e.g. ECOREPAIRSTAKE, LANEVOTE, PORTFOLIOVOTE.
    scope           TEXT NOT NULL,
    -- One-time use token (nonce hash string).
    delegationtoken TEXT NOT NULL UNIQUE,
    -- Whether this delegation has been consumed.
    consumed        INTEGER NOT NULL DEFAULT 0 CHECK (consumed IN (0,1)),
    createdutc      TEXT NOT NULL,
    consumedutc     TEXT
);

CREATE INDEX IF NOT EXISTS idx_brainbounddelegation_scope
    ON brainbounddelegation (scope, evolutionepochid);

-------------------------------------------------------------------------------
-- 6. EcoWealth, KER-aware view (ecowealthview)
-------------------------------------------------------------------------------

-- StewardEcoWealthStatement core table is assumed to exist; here we create a
-- view that joins it with KER residual snapshots for KER-aware eco-wealth.
-- This is a VIEW so agents can query without duplicating data.

DROP VIEW IF EXISTS ecowealthview;

CREATE VIEW ecowealthview AS
SELECT
    s.statementid,
    s.portfolioid,
    s.region,
    s.periodstartutc,
    s.periodendutc,
    s.biomasstotal,
    s.pollinatorindex,
    s.ecotokensearned,
    k.kmean,
    k.emean,
    k.rmean,
    k.rtrust,
    k.reff,
    k.epoch
FROM stewardecowealthstatement AS s
JOIN kerresidualsnapshot AS k
  ON k.region        = s.region
 AND k.windowstartutc = s.periodstartutc
 AND k.windowendutc   = s.periodendutc;

-------------------------------------------------------------------------------
-- 7. Portfolio plane beginnings (biomass, pollinators)
-------------------------------------------------------------------------------

-- Portfolio-level ecological holdings across biomass and pollinator planes.
CREATE TABLE IF NOT EXISTS ecoportfolioplane (
    portfolioplaneid INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolioid      TEXT NOT NULL, -- matches StewardEcoWealthStatement.portfolioid
    evolutionepochid INTEGER NOT NULL REFERENCES evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    region           TEXT NOT NULL,
    -- Biomass and pollinator coordinates normalized 0..1 for plane math, plus raw units.
    biomassnorm      REAL NOT NULL,
    biomassraw       REAL NOT NULL,
    pollinatornorm   REAL NOT NULL,
    pollinatorraw    REAL NOT NULL,
    -- Derived eco-wealth metric combining biomass and pollinators.
    ecowealthscore   REAL NOT NULL,
    -- Trust plane coordinate for this portfolio 0..1.
    rtrustportfolio  REAL NOT NULL,
    -- Time window.
    periodstartutc   TEXT NOT NULL,
    periodendutc     TEXT NOT NULL,
    createdutc       TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_ecoportfolioplane_portfolio_epoch
    ON ecoportfolioplane (portfolioid, evolutionepochid, region);
