-- filename: db_eco_master_index.pg.sql
-- destination: ecorestorationshard/sql/spine/db_eco_master_index.pg.sql
--
-- Purpose:
-- - Provide a PostgreSQL master index for EcoNet / Eco-Fort / Cyboquatics /
--   ecorestorationshard artifacts, governance specs, and eco-wealth tables.
-- - Unify ALN/SAI/MAI, econet repo manifests, KER/Lyapunov math, and
--   eco-wealth / portfolio data into one queryable schema.
-- - Strictly non-actuating: index, evidence, and diagnostics only.

CREATE SCHEMA IF NOT EXISTS eco_idx;

----------------------------------------------------------------------
-- 1. Repository and layer manifests (PostgreSQL mirror)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_idx.master_repo (
    reponame           TEXT PRIMARY KEY,
    githubslug         TEXT NOT NULL,
    roleband           TEXT NOT NULL CHECK (roleband IN ('SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP')),
    visibility         TEXT NOT NULL CHECK (visibility IN ('Public','Private')),
    languageprimary    TEXT NOT NULL,
    description        TEXT,
    ecosafetybinding   TEXT NOT NULL,
    shardprotocol      TEXT NOT NULL,
    lanedefault        TEXT NOT NULL CHECK (lanedefault IN ('RESEARCH','EXPPROD','PROD')),
    -- KER targets for this repo (0..1 bands)
    kertargetk         REAL NOT NULL CHECK (kertargetk >= 0.0 AND kertargetk <= 1.0),
    kertargete         REAL NOT NULL CHECK (kertargete >= 0.0 AND kertargete <= 1.0),
    kertargetr         REAL NOT NULL CHECK (kertargetr >= 0.0 AND kertargetr <= 1.0),
    nonactuatingonly   BOOLEAN NOT NULL,
    manifestschemaver  INTEGER NOT NULL DEFAULT 1,
    didowner           TEXT NOT NULL,
    signingdid         TEXT,
    evidencehex        TEXT,
    createdutc         TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updatedutc         TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE OR REPLACE FUNCTION eco_idx.touch_master_repo()
RETURNS TRIGGER LANGUAGE plpgsql AS $$
BEGIN
  NEW.updatedutc := NOW();
  RETURN NEW;
END;
$$;

CREATE TRIGGER trg_master_repo_updated
BEFORE UPDATE ON eco_idx.master_repo
FOR EACH ROW EXECUTE FUNCTION eco_idx.touch_master_repo();

CREATE INDEX IF NOT EXISTS idx_master_repo_role
    ON eco_idx.master_repo (roleband, visibility);

CREATE INDEX IF NOT EXISTS idx_master_repo_lang
    ON eco_idx.master_repo (languageprimary);

-- Layers (GRAMMAR, KERNEL, EDGESCRIPT, UI, MATERIAL, OTHER)
CREATE TABLE IF NOT EXISTS eco_idx.master_layer (
    layerid            BIGSERIAL PRIMARY KEY,
    reponame           TEXT NOT NULL REFERENCES eco_idx.master_repo(reponame) ON DELETE CASCADE,
    layername          TEXT NOT NULL,
    layertier          TEXT NOT NULL,
    languages          TEXT NOT NULL,
    description        TEXT,
    contracts          TEXT,
    createdutc         TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_master_layer_repo
    ON eco_idx.master_layer (reponame, layertier);

CREATE INDEX IF NOT EXISTS idx_master_layer_name
    ON eco_idx.master_layer (layername);

-- Role hints (AI/CI discovery surface)
CREATE TABLE IF NOT EXISTS eco_idx.master_rolehint (
    hintid             BIGSERIAL PRIMARY KEY,
    reponame           TEXT NOT NULL REFERENCES eco_idx.master_repo(reponame) ON DELETE CASCADE,
    hintkey            TEXT NOT NULL,
    hintval            TEXT NOT NULL,
    createdutc         TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_master_rolehint_repo
    ON eco_idx.master_rolehint (reponame, hintkey);

----------------------------------------------------------------------
-- 2. Knowledge shards (ALN / SAI / MAI) and path map
--    (mirror of ecoknowledgeshardindex + ecorepopathmap)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_idx.master_artifact (
    artifactid         BIGSERIAL PRIMARY KEY,
    shardname          TEXT NOT NULL,               -- e.g. ecolakekergovernancev1.aln
    shardtype          TEXT NOT NULL CHECK (shardtype IN ('ALN','SAI','MAI','SQL','RUST','CONFIG','WORKFLOW','CSV','DOC')),
    schemaid           TEXT NOT NULL,               -- e.g. ALN-ECO-LAKE-KER-GOV-1
    version            TEXT NOT NULL,               -- semver or tag
    filename           TEXT NOT NULL,               -- repo-relative path
    scope              TEXT NOT NULL,               -- high-level scope string
    repotarget         TEXT NOT NULL,               -- matches eco_idx.master_repo.reponame
    description        TEXT NOT NULL,
    authordid          TEXT NOT NULL,
    createdat          TIMESTAMPTZ NOT NULL,
    boundschemas       TEXT NOT NULL,               -- CSV of bound DB schemas/tables
    primarydid         TEXT NOT NULL,
    altdid             TEXT NOT NULL,
    walletevm          TEXT NOT NULL,
    facebookprofile    TEXT NOT NULL,
    keraxistag         TEXT NOT NULL,               -- e.g. lakeker, lakehumanloopRguardrail
    safetyprofile      TEXT NOT NULL,               -- e.g. nopersonalreidnoprivateinference
    active             BOOLEAN NOT NULL DEFAULT TRUE,
    -- Knowledge, eco-impact, and risk-of-harm bands (0..1) for this shard
    knowledgefactor    REAL NOT NULL DEFAULT 0.0 CHECK (knowledgefactor >= 0.0 AND knowledgefactor <= 1.0),
    ecoimpactvalue     REAL NOT NULL DEFAULT 0.0 CHECK (ecoimpactvalue >= 0.0 AND ecoimpactvalue <= 1.0),
    riskofharmvalue    REAL NOT NULL DEFAULT 0.0 CHECK (riskofharmvalue >= 0.0 AND riskofharmvalue <= 1.0),
    UNIQUE (schemaid, version, shardtype, repotarget)
);

CREATE INDEX IF NOT EXISTS idx_master_artifact_type
    ON eco_idx.master_artifact (shardtype, scope);

CREATE INDEX IF NOT EXISTS idx_master_artifact_repo
    ON eco_idx.master_artifact (repotarget, active);

CREATE INDEX IF NOT EXISTS idx_master_artifact_eco
    ON eco_idx.master_artifact (ecoimpactvalue DESC, riskofharmvalue ASC);

-- Logical -> physical path mapping
CREATE TABLE IF NOT EXISTS eco_idx.master_pathmap (
    mapid              BIGSERIAL PRIMARY KEY,
    repotarget         TEXT NOT NULL,
    filename           TEXT NOT NULL,
    destpath           TEXT NOT NULL,
    lanescope          TEXT NOT NULL,              -- e.g. RESEARCH, EXPPROD, PROD, GOVERNANCE
    active             BOOLEAN NOT NULL DEFAULT TRUE,
    createdutc         TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (repotarget, filename)
);

CREATE INDEX IF NOT EXISTS idx_master_pathmap_repo
    ON eco_idx.master_pathmap (repotarget, lanescope);

----------------------------------------------------------------------
-- 3. KER, risk planes, and capability floors (governance spine)
--    (mirror of riskplane, capabilityfloorhistory, kerbandversion, evolutionepoch)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_idx.riskplane (
    planeid            BIGSERIAL PRIMARY KEY,
    planename          TEXT NOT NULL UNIQUE,       -- energy, carbon, biodiversity, topology, etc.
    nonoffsettable     BOOLEAN NOT NULL DEFAULT FALSE,
    capabilityfloor    REAL NOT NULL DEFAULT 0.0,
    capabilityfloorhardmin REAL NOT NULL DEFAULT 0.0,
    description        TEXT
);

CREATE INDEX IF NOT EXISTS idx_riskplane_nonoffset
    ON eco_idx.riskplane (nonoffsettable);

CREATE TABLE IF NOT EXISTS eco_idx.evolutionepoch (
    evolutionepochid   BIGSERIAL PRIMARY KEY,
    epochlabel         TEXT NOT NULL UNIQUE,       -- e.g. PhoenixWater2026Q2
    forkid             TEXT NOT NULL,
    parentepochid      BIGINT REFERENCES eco_idx.evolutionepoch(evolutionepochid),
    effectivefromutc   TIMESTAMPTZ NOT NULL,
    effectiveuntilutc  TIMESTAMPTZ,
    notes              TEXT
);

CREATE INDEX IF NOT EXISTS idx_evolutionepoch_fork
    ON eco_idx.evolutionepoch (forkid);

CREATE TABLE IF NOT EXISTS eco_idx.capabilityfloorhistory (
    floorid            BIGSERIAL PRIMARY KEY,
    planeid            BIGINT NOT NULL REFERENCES eco_idx.riskplane(planeid) ON DELETE CASCADE,
    evolutionepochid   BIGINT NOT NULL REFERENCES eco_idx.evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    floorvalue         REAL NOT NULL,
    monotoneok         BOOLEAN NOT NULL DEFAULT FALSE,
    createdutc         TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (planeid, evolutionepochid)
);

CREATE INDEX IF NOT EXISTS idx_capabilityfloor_plane_epoch
    ON eco_idx.capabilityfloorhistory (planeid, evolutionepochid);

CREATE TABLE IF NOT EXISTS eco_idx.kerbandversion (
    kerbandid          BIGSERIAL PRIMARY KEY,
    bandname           TEXT NOT NULL,              -- RESEARCH, EXPPROD, PROD
    versionepoch       BIGINT NOT NULL REFERENCES eco_idx.evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    kfloor             REAL NOT NULL,
    efloor             REAL NOT NULL,
    rceiling           REAL NOT NULL,
    UNIQUE (bandname, versionepoch)
);

CREATE INDEX IF NOT EXISTS idx_kerbandversion_band_epoch
    ON eco_idx.kerbandversion (bandname, versionepoch);

----------------------------------------------------------------------
-- 4. Eco-wealth and steward portfolio surfaces (summary index)
--    (lightweight mirror; full tables remain in SQLite or separate PG schemas)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_idx.steward_portfolio_index (
    statementid        BIGINT PRIMARY KEY,
    stewarddid         TEXT NOT NULL,
    region             TEXT NOT NULL,
    lane               TEXT NOT NULL,
    windowstartutc     TIMESTAMPTZ NOT NULL,
    windowendutc       TIMESTAMPTZ NOT NULL,
    kmean              REAL NOT NULL,
    emean              REAL NOT NULL,
    rmean              REAL NOT NULL,
    vtmaxwindow        REAL NOT NULL,
    ecounitfinal       REAL NOT NULL,
    knowledgefactorbefore REAL NOT NULL,
    knowledgefactorafter  REAL NOT NULL,
    ecoimpactbefore    REAL NOT NULL,
    ecoimpactafter     REAL NOT NULL,
    riskofharmbefore   REAL NOT NULL,
    riskofharmafter    REAL NOT NULL,
    kernelid           TEXT NOT NULL,
    planecontractid    TEXT NOT NULL,
    corridorsetid      TEXT NOT NULL,
    vshardkerwindowhash TEXT NOT NULL,
    shardlisthash      TEXT NOT NULL,
    evidencehex        TEXT NOT NULL,
    signingdid         TEXT NOT NULL,
    createdutc         TIMESTAMPTZ NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_steward_portfolio_latest
    ON eco_idx.steward_portfolio_index (stewarddid, windowendutc DESC);

CREATE INDEX IF NOT EXISTS idx_steward_portfolio_region
    ON eco_idx.steward_portfolio_index (region, lane, windowendutc DESC);

----------------------------------------------------------------------
-- 5. Cross-artifact eco-impact and blast-radius views
--    (soft dependencies: Eco-Fort blastradiusindex / cyboblastradius)
----------------------------------------------------------------------

-- Basic blast-radius evidence surface.
-- This is a lightweight mirror that can be populated from Eco-Fort / EcoNet
-- blastradius tables via ETL; we do not re-encode full details here.
CREATE TABLE IF NOT EXISTS eco_idx.master_blastradius (
    linkid             BIGSERIAL PRIMARY KEY,
    sourcetype         TEXT NOT NULL,              -- REPO, SCHEMA, PARTICLE, SHARD, NODE
    sourceid           TEXT NOT NULL,
    targettype         TEXT NOT NULL,              -- NODE, SHARD, MACHINE, MATERIAL, REGION
    targetid           TEXT NOT NULL,
    impactplane        TEXT NOT NULL,              -- ENERGY, CARBON, MATERIALS, BIODIVERSITY, DATAQUALITY, TOPOLOGY, RESPONSIBILITY
    impactscore        REAL NOT NULL,              -- 0..1
    vtsensitivity      REAL,
    notes              TEXT,
    createdutc         TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_master_blastradius_source
    ON eco_idx.master_blastradius (sourcetype, sourceid, impactplane);

CREATE INDEX IF NOT EXISTS idx_master_blastradius_target
    ON eco_idx.master_blastradius (targettype, targetid, impactplane);

-- View: eco-eco scoring per artifact + blast-radius rollup.
CREATE OR REPLACE VIEW eco_idx.v_master_eco_score AS
SELECT
    a.artifactid,
    a.shardname,
    a.shardtype,
    a.schemaid,
    a.version,
    a.filename,
    a.repotarget,
    a.scope,
    a.keraxistag,
    a.safetyprofile,
    a.knowledgefactor,
    a.ecoimpactvalue,
    a.riskofharmvalue,
    r.roleband,
    r.languageprimary,
    -- Aggregate blast-radius by carbon/biodiversity/materials planes where present.
    COALESCE(SUM(CASE WHEN b.impactplane = 'CARBON' THEN b.impactscore END), 0.0) AS carbon_impact_sum,
    COALESCE(SUM(CASE WHEN b.impactplane = 'BIODIVERSITY' THEN b.impactscore END), 0.0) AS biodiversity_impact_sum,
    COALESCE(SUM(CASE WHEN b.impactplane = 'MATERIALS' THEN b.impactscore END), 0.0) AS materials_impact_sum,
    COALESCE(SUM(CASE WHEN b.impactplane = 'ENERGY' THEN b.impactscore END), 0.0) AS energy_impact_sum
FROM eco_idx.master_artifact AS a
LEFT JOIN eco_idx.master_repo AS r
  ON r.reponame = a.repotarget
LEFT JOIN eco_idx.master_blastradius AS b
  ON b.sourcetype = 'PARTICLE'
 AND b.sourceid  = a.shardname
GROUP BY
    a.artifactid,
    a.shardname,
    a.shardtype,
    a.schemaid,
    a.version,
    a.filename,
    a.repotarget,
    a.scope,
    a.keraxistag,
    a.safetyprofile,
    a.knowledgefactor,
    a.ecoimpactvalue,
    a.riskofharmvalue,
    r.roleband,
    r.languageprimary;

CREATE INDEX IF NOT EXISTS idx_v_master_eco_score_rank
    ON eco_idx.v_master_eco_score (ecoimpactvalue DESC, riskofharmvalue ASC, carbon_impact_sum ASC, biodiversity_impact_sum ASC);

----------------------------------------------------------------------
-- 6. Always-improve / safestep scoring hints
--    (non-actuating helper for CI / AI routing)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_idx.safestep_hint (
    hintid             BIGSERIAL PRIMARY KEY,
    artifactid         BIGINT NOT NULL REFERENCES eco_idx.master_artifact(artifactid) ON DELETE CASCADE,
    evolutionepochid   BIGINT NOT NULL REFERENCES eco_idx.evolutionepoch(evolutionepochid) ON DELETE CASCADE,
    -- Boolean that CI sets when replay confirms K↑, E↑, R↓, Vt↓ under new policy.
    safestep_monotone_ok BOOLEAN NOT NULL DEFAULT FALSE,
    -- Optional scalar summary in 0..1 (1 = strongest monotone improvement).
    safestep_score     REAL NOT NULL DEFAULT 0.0 CHECK (safestep_score >= 0.0 AND safestep_score <= 1.0),
    -- Free-form notes for why this artifact is a good candidate for expansion.
    notes              TEXT,
    createdutc         TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (artifactid, evolutionepochid)
);

CREATE INDEX IF NOT EXISTS idx_safestep_hint_rank
    ON eco_idx.safestep_hint (safestep_monotone_ok DESC, safestep_score DESC);

----------------------------------------------------------------------
-- 7. Seed: register master-index itself as an artifact (idempotent)
----------------------------------------------------------------------

INSERT INTO eco_idx.master_repo (
    reponame,
    githubslug,
    roleband,
    visibility,
    languageprimary,
    description,
    ecosafetybinding,
    shardprotocol,
    lanedefault,
    kertargetk,
    kertargete,
    kertargetr,
    nonactuatingonly,
    manifestschemaver,
    didowner,
    signingdid,
    evidencehex
)
VALUES (
    'ecorestorationshard',
    'mk-bluebird/eco_restoration_shard',
    'RESEARCH',
    'Public',
    'Rust',
    'Ecological restoration research, biodegradable substrates, and Cyboquatic materials for carbon-negative, water-safe machinery.',
    'ecosafety.corridors.v2',
    'EcoNetSchemaShard2026v1',
    'RESEARCH',
    0.94,
    0.90,
    0.12,
    TRUE,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    NULL,
    NULL
)
ON CONFLICT (reponame) DO NOTHING;

INSERT INTO eco_idx.master_artifact (
    shardname,
    shardtype,
    schemaid,
    version,
    filename,
    scope,
    repotarget,
    description,
    authordid,
    createdat,
    boundschemas,
    primarydid,
    altdid,
    walletevm,
    facebookprofile,
    keraxistag,
    safetyprofile,
    active,
    knowledgefactor,
    ecoimpactvalue,
    riskofharmvalue
)
VALUES (
    'db_eco_master_index.pg.sql',
    'SQL',
    'ECO-MASTER-INDEX-PG-1',
    '2026.1.0',
    'sql/spine/db_eco_master_index.pg.sql',
    'ecoconstellationmasterindex',
    'ecorestorationshard',
    'PostgreSQL master index for EcoNet/Eco-Fort/ecorestorationshard artifacts, KER governance, and eco-wealth summaries.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    NOW(),
    'ecoknowledgeshardindex,econetrepoindex,definitionregistry,stewardecowealthstatement',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
    '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
    'https://facebook.com/profile.php?id=61583146843874',
    'eco_master_index',
    'nopersonalreidnoprivateinference',
    TRUE,
    0.96,
    0.93,
    0.11
)
ON CONFLICT (schemaid, version, shardtype, repotarget) DO NOTHING;
