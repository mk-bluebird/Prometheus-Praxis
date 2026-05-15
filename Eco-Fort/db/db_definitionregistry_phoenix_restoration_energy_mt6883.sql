-- filename: db_definitionregistry_phoenix_restoration_energy_mt6883.sql
-- destination: Eco-Fort/db/db_definitionregistry_phoenix_restoration_energy_mt6883.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- This script registers the Phoenix restoration / ecoperjoule / MT6883 / identity
-- artifacts in the constellation-wide definitionregistry, using region-parameterized
-- logicalnames and binding them to concrete files.

-- Assumes:
--   - definitionregistry from governance-spine DDL is already present.[file:2]
--   - artifactregistry and artifactprovenance are used for hash + RoH anchors.[file:2]

----------------------------------------------------------------------
-- 1. Phoenix restoration blast-radius grammar
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defid,
    logicalname,
    versiontag,
    repo,
    filepath,
    category,
    description,
    frozen
) VALUES (
    'DR-PHX-REST-BLAST-2026v1',
    'restoration.blastradius.phoenix',
    '2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_restoration_blastradius_phoenix.sql',
    'BLASTRADIUS',
    'Phoenix-AZ restoration-aware blast-radius schema and views (restorationradius_m, deltamass_window_kg, deltakarma_window, gw_risk_max, restoration_ok) for blastradiusindex.',
    1
);

----------------------------------------------------------------------
-- 2. Phoenix eco-per-joule policy and energy plane grammar
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defid,
    logicalname,
    versiontag,
    repo,
    filepath,
    category,
    description,
    frozen
) VALUES (
    'DR-PHX-ECO-PER-J-2026v1',
    'energy.ecoperjoule.policy.phoenix',
    '2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_ecoperjoule_policy_energy.sql',
    'ENERGY',
    'Phoenix-AZ eco-per-joule policy table and governance views (ecoperjoule_policy, v_cyboquatic_ecoperjoule, v_cyboquatic_ecoperjoule_prod_phx) that set carbonnegativeok for Cyboquatic energy planes.',
    1
);

----------------------------------------------------------------------
-- 3. Phoenix MT6883 continuity and neurorights lane grammar
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defid,
    logicalname,
    versiontag,
    repo,
    filepath,
    category,
    description,
    frozen
) VALUES (
    'DR-PHX-MT6883-LANE-2026v1',
    'mt6883.lane.continuity.phoenix',
    '2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_mt6883_lane_continuity.sql',
    'MT6883',
    'Phoenix-AZ MT6883 lane continuity and neurorights radius extensions (lanestatusshard mt6883_ok, neuroethic_ok; blastradiusindex neuroethic_radius_hours) exposed through v_mt6883_lane_continuity and v_blastradius_mt6883_neuro.',
    1
);

----------------------------------------------------------------------
-- 4. Phoenix restoration identity binding grammar
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defid,
    logicalname,
    versiontag,
    repo,
    filepath,
    category,
    description,
    frozen
) VALUES (
    'DR-PHX-REST-ID-BIND-2026v1',
    'restoration.identity.binding.phoenix',
    '2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_restoration_identity_binding.sql',
    'IDENTITY',
    'Phoenix-AZ restoration identity binding schema tying Bostrom addresses to governance artifacts and DB shards for restoration, energy, and MT6883 grammar.',
    1
);

----------------------------------------------------------------------
-- 5. Phoenix restoration/energy/MT6883 governance tool
----------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    defid,
    logicalname,
    versiontag,
    repo,
    filepath,
    category,
    description,
    frozen
) VALUES (
    'DR-PHX-REST-ENERGY-MT6883-TOOL-2026v1',
    'restoration.energy.mt6883.tool.phoenix',
    '2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'src/tools/restoration_energy_mt6883_tool.cpp',
    'TOOL',
    'Phoenix-AZ non-actuating governance CLI for restoration, eco-per-joule, and MT6883 lane continuity over the governance-spine DB.',
    0
);

----------------------------------------------------------------------
-- 6. Optional: shard-catalog binding for Phoenix governance DB
--    (ties dbshardcatalogecosafetydbs.sql objects to these definitions).[file:2]
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS ecosafetydbdefinitionbinding (
    bindingid       INTEGER PRIMARY KEY AUTOINCREMENT,
    dbshardid       INTEGER NOT NULL,
    definitionid    TEXT NOT NULL,
    bindingrole     TEXT NOT NULL,
    createdutc      TEXT NOT NULL,
    UNIQUE (dbshardid, definitionid),
    FOREIGN KEY (definitionid) REFERENCES definitionregistry(defid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_ecosafetydbdefinitionbinding_def
ON ecosafetydbdefinitionbinding (definitionid);

-- NOTE:
-- dbshardid values are managed by dbshardcatalogecosafetydbs.sql; this block
-- only prepares the binding table and will be populated by region-specific
-- shard catalog scripts.
