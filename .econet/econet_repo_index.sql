-- filename: .econet/econet_repo_index.sql
-- destination: eco_restoration_shard/.econet/econet_repo_index.sql
-- repo: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Canonical per-repo index (normalized, DID-anchored)
-------------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS econet_repo_index (
    index_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_name           TEXT    NOT NULL,
    github_slug         TEXT    NOT NULL,
    github_url          TEXT    NOT NULL,
    owner_did           TEXT    NOT NULL,
    role_band           TEXT    NOT NULL,  -- SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP
    visibility          TEXT    NOT NULL,  -- Public, Private
    language_primary    TEXT    NOT NULL,
    description         TEXT,
    ecosafety_binding   TEXT    NOT NULL,  -- e.g. ecosafety.corridors.v2
    shard_protocol      TEXT    NOT NULL,  -- e.g. EcoNetSchemaShard2026v1
    lane_default        TEXT    NOT NULL,  -- RESEARCH, EXPPROD, PROD
    ker_target_k        REAL    NOT NULL,
    ker_target_e        REAL    NOT NULL,
    ker_target_r        REAL    NOT NULL,
    roh_ceiling         REAL    NOT NULL,
    non_actuating_only  INTEGER NOT NULL CHECK (non_actuating_only IN (0,1)),
    rust_edition        TEXT    NOT NULL,
    rust_version_min    TEXT    NOT NULL,
    region              TEXT    NOT NULL,
    manifest_schema_ver INTEGER NOT NULL DEFAULT 1,
    version_seq         INTEGER NOT NULL DEFAULT 1,
    did_owner           TEXT,
    signing_did         TEXT,
    evidence_hex        TEXT,
    created_utc         TEXT    NOT NULL,
    CHECK (ker_target_k BETWEEN 0.0 AND 1.0 AND ker_target_k >= 0.90),
    CHECK (ker_target_e BETWEEN 0.0 AND 1.0 AND ker_target_e >= 0.85),
    CHECK (ker_target_r BETWEEN 0.0 AND 1.0 AND ker_target_r <= 0.20),
    CHECK (roh_ceiling   >= 0.0 AND roh_ceiling   <= 1.0)
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_econet_repo_index_name_ver
    ON econet_repo_index (repo_name, version_seq);

CREATE INDEX IF NOT EXISTS idx_econet_repo_index_roleband
    ON econet_repo_index (role_band, visibility);

CREATE INDEX IF NOT EXISTS idx_econet_repo_index_visibility
    ON econet_repo_index (visibility);

-------------------------------------------------------------------------------
-- 1.a Lint view for manifest violations (used by econet_repo_manifest_lint)
-------------------------------------------------------------------------------
CREATE VIEW IF NOT EXISTS vw_repo_manifest_violations AS
SELECT
    index_id,
    repo_name,
    'ker_target_k below 0.90' AS violation
FROM econet_repo_index
WHERE ker_target_k < 0.90
UNION ALL
SELECT
    index_id,
    repo_name,
    'ker_target_e below 0.85'
FROM econet_repo_index
WHERE ker_target_e < 0.85
UNION ALL
SELECT
    index_id,
    repo_name,
    'ker_target_r above 0.20'
FROM econet_repo_index
WHERE ker_target_r > 0.20
UNION ALL
SELECT
    index_id,
    repo_name,
    'roh_ceiling above 0.30'
FROM econet_repo_index
WHERE roh_ceiling > 0.30
UNION ALL
SELECT
    index_id,
    repo_name,
    'non_actuating_only is 0 for RESEARCH repo'
FROM econet_repo_index
WHERE role_band = 'RESEARCH'
  AND non_actuating_only = 0
UNION ALL
SELECT
    index_id,
    repo_name,
    'role_band is not RESEARCH for eco_restoration_shard'
FROM econet_repo_index
WHERE repo_name = 'eco_restoration_shard'
  AND role_band != 'RESEARCH';

-------------------------------------------------------------------------------
-- 2. Per-repo programming layers
-------------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS econet_layer (
    layer_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_name   TEXT NOT NULL,
    layer_name  TEXT NOT NULL,
    layer_tier  TEXT NOT NULL,  -- GRAMMAR, KERNEL, EDGESCRIPT, UI, GOVERNANCE, MATERIAL, OTHER
    languages   TEXT NOT NULL,  -- comma-separated list
    description TEXT,
    contracts   TEXT,
    FOREIGN KEY (repo_name)
        REFERENCES econet_repo_index(repo_name)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_econet_layer_repo
    ON econet_layer (repo_name, layer_tier);

CREATE INDEX IF NOT EXISTS idx_econet_layer_tier
    ON econet_layer (layer_tier);

-------------------------------------------------------------------------------
-- 3. Optional free-form hints for agents
-------------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS econet_role_hint (
    hint_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_name TEXT NOT NULL,
    hint_key  TEXT NOT NULL,
    hint_val  TEXT NOT NULL,
    FOREIGN KEY (repo_name)
        REFERENCES econet_repo_index(repo_name)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_econet_role_hint_repo
    ON econet_role_hint (repo_name, hint_key);

-------------------------------------------------------------------------------
-- 4. Seed manifest for eco_restoration_shard (RESEARCH, non-actuating)
-------------------------------------------------------------------------------
INSERT OR REPLACE INTO econet_repo_index (
    repo_name,
    github_slug,
    github_url,
    owner_did,
    role_band,
    visibility,
    language_primary,
    description,
    ecosafety_binding,
    shard_protocol,
    lane_default,
    ker_target_k,
    ker_target_e,
    ker_target_r,
    roh_ceiling,
    non_actuating_only,
    rust_edition,
    rust_version_min,
    region,
    manifest_schema_ver,
    version_seq,
    did_owner,
    signing_did,
    evidence_hex,
    created_utc
) VALUES (
    'eco_restoration_shard',
    'mk-bluebird/eco_restoration_shard',
    'https://github.com/mk-bluebird/eco_restoration_shard',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
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
    0.30,
    1,
    '2024',
    '1.85',
    'Phoenix-AZ-US',
    1,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    NULL,
    '0xECOREPOINDEX2026V1_bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    strftime('%Y-%m-%dT%H:%M:%SZ','now')
);

DELETE FROM econet_layer
WHERE repo_name = 'eco_restoration_shard';

INSERT INTO econet_layer (
    repo_name, layer_name, layer_tier, languages, description, contracts
) VALUES
    (
        'eco_restoration_shard',
        'Eco-restoration grammar client',
        'GRAMMAR',
        'Rust',
        'Imports ecosafety risk vectors, KER residuals, and biodegradable material schemas as a strict client of the EcoNet spine.',
        'Must import ecosafety.corridors.v2 and EcoNetSchemaShard2026v1; no new risk planes; no actuator bindings.'
    ),
    (
        'eco_restoration_shard',
        'Biodegradable substrate kinetics kernels',
        'KERNEL',
        'Rust',
        'Non-actuating kinetics kernels for biodegradable substrates and Cyboquatic channels, emitting risk vectors and KER diagnostics only.',
        'NonActuatingWorkload; read/write QPU data shards only; no PLC or actuator crates; V_t must be non-increasing under gated workloads.'
    ),
    (
        'eco_restoration_shard',
        'Materials eco-risk combiners',
        'KERNEL',
        'Rust',
        'Non-actuating materials risk aggregation for carbon, leachate, micro-residue, and biodiversity planes.',
        'NonActuatingWorkload; carbon and biodiversity non-offsettable; K and E must not increase when R worsens on these planes.'
    ),
    (
        'eco_restoration_shard',
        'Lua edge diagnostics harness',
        'EDGESCRIPT',
        'Lua',
        'Read-only Lua harness for diagnostics and visualizations over biodegradable substrate shards.',
        'ReadOnlyClient; may call only non-actuating FFI; no hardware drivers; no lane promotion.'
    ),
    (
        'eco_restoration_shard',
        'Android KER visualizer',
        'UI',
        'Kotlin',
        'Kotlin/Android visualizer for KER maps, blast-radius overlays, and energy/carbon trends for Cyboquatic pilots.',
        'VisualizationOnly; uses read-only JSON APIs from Rust cdylib; no actuator controls; no lane changes.'
    );

DELETE FROM econet_role_hint
WHERE repo_name = 'eco_restoration_shard';

INSERT INTO econet_role_hint (repo_name, hint_key, hint_val) VALUES
    ('eco_restoration_shard', 'domain', 'eco-restoration'),
    ('eco_restoration_shard', 'medium', 'water,soil'),
    ('eco_restoration_shard', 'ecoplane', 'materials,carbon,biodiversity,dataquality'),
    ('eco_restoration_shard', 'pilot_region', 'Phoenix-AZ'),
    ('eco_restoration_shard', 'primary_particles', 'BiodegradableSubstrateChannelKinetics2026v1,BiodegradableSubstrateChannelEcosafety2026v1'),
    ('eco_restoration_shard', 'shard_types', 'QPUDATASHARD,SCHEMA,EVIDENCE'),
    ('eco_restoration_shard', 'lanes', 'RESEARCH'),
    ('eco_restoration_shard', 'non_actuating', 'true'),
    ('eco_restoration_shard', 'bostrom_did_primary', 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
    ('eco_restoration_shard', 'bostrom_did_alt', 'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc');

-------------------------------------------------------------------------------
-- 5. Seed manifest for EcoNet-CEIM-PhoenixWater (ENGINE band cyboquatic repo)
-------------------------------------------------------------------------------
INSERT OR REPLACE INTO econet_repo_index (
    repo_name,
    github_slug,
    github_url,
    owner_did,
    role_band,
    visibility,
    language_primary,
    description,
    ecosafety_binding,
    shard_protocol,
    lane_default,
    ker_target_k,
    ker_target_e,
    ker_target_r,
    roh_ceiling,
    non_actuating_only,
    rust_edition,
    rust_version_min,
    region,
    manifest_schema_ver,
    version_seq,
    did_owner,
    signing_did,
    evidence_hex,
    created_utc
) VALUES (
    'EcoNet-CEIM-PhoenixWater',
    'mk-bluebird/EcoNet-CEIM-PhoenixWater',
    'https://github.com/mk-bluebird/EcoNet-CEIM-PhoenixWater',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'ENGINE',
    'Public',
    'Rust',
    'Cyboquatic CEIM/CPVM kernels and ecosafety-gated routing for Phoenix water nodes.',
    'cyboquatic-ecosafety-coreEcosafetyGrammar2026v1.aln',
    'ALN-RFC4180EcoNetSchemaShard2026v1',
    'EXPPROD',
    0.94,
    0.90,
    0.13,
    0.30,
    0,
    '2024',
    '1.85',
    'Phoenix-AZ-US',
    1,
    1,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    NULL,
    NULL,
    strftime('%Y-%m-%dT%H:%M:%SZ','now')
);

DELETE FROM econet_layer
WHERE repo_name = 'EcoNet-CEIM-PhoenixWater';

INSERT INTO econet_layer (
    repo_name, layer_name, layer_tier, languages, description, contracts
) VALUES
    (
        'EcoNet-CEIM-PhoenixWater',
        'Ecosafety spine client',
        'GRAMMAR',
        'Rust',
        'Imports RiskCoord, RiskVector, residual, and KER from cyboquatic ecosafety core.',
        'Must not redefine residual or KER; V(t+1) <= V(t) for all replayed workloads.'
    ),
    (
        'EcoNet-CEIM-PhoenixWater',
        'Hydrology kernels',
        'KERNEL',
        'Rust,C',
        'Implements CEIM mass-load and CPVM viability kernels for Phoenix hydrological basins.',
        'Non-actuating: kernels compute r_x, V_t, and KER only; actuation handled by fenced gateway crates.'
    ),
    (
        'EcoNet-CEIM-PhoenixWater',
        'Hydrological buffer atlas',
        'KERNEL',
        'Rust',
        'Maintains rFOG, rTDS, rEcoli, and V_t for hydrological reaches using qpudatashards.',
        'Outputs lane-tagged; corridors may tighten but never loosen hazard bands.'
    );

DELETE FROM econet_role_hint
WHERE repo_name = 'EcoNet-CEIM-PhoenixWater';

INSERT INTO econet_role_hint (repo_name, hint_key, hint_val) VALUES
    (
        'EcoNet-CEIM-PhoenixWater',
        'shard_types',
        'HydrologicalBufferShard,PhoenixMarShard,FOGRoutingDecision'
    ),
    (
        'EcoNet-CEIM-PhoenixWater',
        'primary_plane',
        'hydraulics'
    ),
    (
        'EcoNet-CEIM-PhoenixWater',
        'pilot_domains',
        'Phoenix-AZ;Gila River;Lake Pleasant PFBS'
    );
