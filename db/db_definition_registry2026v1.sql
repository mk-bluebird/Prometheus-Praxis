-- filename db_definition_registry2026v1.sql
-- destination Eco-Fort/db/db_definition_registry2026v1.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. DefinitionRegistry2026v1 SQL mirror table
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS definition_registry (
    defid            INTEGER PRIMARY KEY AUTOINCREMENT,
    particlename     TEXT NOT NULL,  -- e.g. HydrologicalBufferPhoenix2026v1
    category         TEXT NOT NULL,  -- e.g. ECOSAFETY, HYDROLOGY, BIODIVERSITY
    primaryplane     TEXT NOT NULL,  -- e.g. hydrologyMAR, biodiversity, carbon
    role             TEXT NOT NULL,  -- QPUDATASHARD, SUBSTRATE, CORRIDOR, GOVERNANCE
    lyapchannel      TEXT NOT NULL,  -- hydraulics, materials, topology, etc.
    alnfile          TEXT NOT NULL,  -- repo-relative path to ALN definition
    sqlfile          TEXT,           -- repo-relative path to SQL mirror, if any
    rustmodule       TEXT,           -- crate::module::path if mirrored in Rust
    repotarget       TEXT NOT NULL,  -- target repository name, e.g. eco_restoration_shard
    shardprotocol    TEXT NOT NULL,  -- e.g. EcoNetSchemaShard2026v1
    versiontag       TEXT NOT NULL,  -- e.g. 2026v1
    description      TEXT,
    evidencehex      TEXT NOT NULL,  -- hash of ALN+SQL+Rust bundle
    signingdid       TEXT NOT NULL,  -- e.g. bostrom18sd2u...
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    UNIQUE (particlename, primaryplane, repotarget)
);

CREATE INDEX IF NOT EXISTS idx_definition_registry_plane
    ON definition_registry(primaryplane, lyapchannel);

CREATE INDEX IF NOT EXISTS idx_definition_registry_repo
    ON definition_registry(repotarget, category, role);
