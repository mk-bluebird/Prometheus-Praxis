-- filename: db/db_dkg_spine.sql
-- destination: eco_restoration_shard/db/db_dkg_spine.sql
-- Purpose:
--   - Non-actuating mirror of Bostrom/IPFS content into EcoNet-style tables.
--   - Links: neurons, particles (CIDs), cyberlinks, and EcoNet repos/shards.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS dkg_neuron (
    neuron_id      TEXT PRIMARY KEY,        -- e.g. 'bostrom18sd2...'
    alias          TEXT,
    w_volt         REAL,                    -- effective Volt balance snapshot
    w_ampere       REAL,                    -- effective Ampere balance snapshot
    w_energy       REAL,                    -- W = Volt * Ampere at snapshot
    last_update_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS dkg_particle (
    particle_cid   TEXT PRIMARY KEY,        -- IPFS CID
    content_type   TEXT NOT NULL,           -- 'markdown','aln','rust','sql'
    size_bytes     INTEGER,
    created_by     TEXT REFERENCES dkg_neuron(neuron_id),
    created_utc    TEXT NOT NULL,
    eco_tag        TEXT,                    -- e.g. 'eco_restoration_shard'
    repo_hint      TEXT                     -- GitHub slug if applicable
);

CREATE TABLE IF NOT EXISTS dkg_cyberlink (
    link_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    from_cid       TEXT NOT NULL REFERENCES dkg_particle(particle_cid)
                   ON DELETE CASCADE,
    to_cid         TEXT NOT NULL REFERENCES dkg_particle(particle_cid)
                   ON DELETE CASCADE,
    neuron_id      TEXT NOT NULL REFERENCES dkg_neuron(neuron_id)
                   ON DELETE CASCADE,
    weight         REAL NOT NULL,           -- Ampere-weighted influence
    created_utc    TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_dkg_cyberlink_from
    ON dkg_cyberlink (from_cid);

CREATE INDEX IF NOT EXISTS idx_dkg_cyberlink_to
    ON dkg_cyberlink (to_cid);

CREATE TABLE IF NOT EXISTS dkg_particle_rank (
    particle_cid   TEXT PRIMARY KEY REFERENCES dkg_particle(particle_cid)
                   ON DELETE CASCADE,
    cyberrank      REAL NOT NULL,
    updated_utc    TEXT NOT NULL
);

-- Mapping from EcoNet repo/revision to DKG particles
CREATE TABLE IF NOT EXISTS dkg_repo_binding (
    binding_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame       TEXT NOT NULL,           -- 'eco_restoration_shard'
    git_commit     TEXT NOT NULL,
    particle_cid   TEXT NOT NULL REFERENCES dkg_particle(particle_cid)
                   ON DELETE CASCADE,
    created_utc    TEXT NOT NULL,
    UNIQUE (reponame, git_commit)
);
