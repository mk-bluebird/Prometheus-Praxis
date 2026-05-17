-- filename: dbdefinitionregistry.sql
-- destination: Eco-Fort/db/dbdefinitionregistry.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Definition registry core tables
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS definition_scope (
    scope_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    scope_name      TEXT NOT NULL UNIQUE,      -- e.g. 'LANE_POLICY', 'KER_KERNEL', 'ROH_MODEL'
    description     TEXT
);

CREATE TABLE IF NOT EXISTS definition_registry (
    definition_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Logical name of the definition (stable across versions).
    def_name        TEXT NOT NULL,            -- e.g. 'EcosafetyGrammar2026v1', 'PlaneWeightsShard2026v1'
    -- Short scope identifier.
    scope_id        INTEGER NOT NULL REFERENCES definition_scope(scope_id) ON DELETE CASCADE,
    -- Version tag or label for this definition.
    version_label   TEXT NOT NULL,            -- e.g. '2026.v1', 'v1'
    -- Type of artifact.
    artifact_kind   TEXT NOT NULL,            -- 'SQL', 'ALN', 'RUST', 'VIEW'
    -- Repository target for this artifact.
    repo_target     TEXT NOT NULL,            -- 'Eco-Fort', 'Virta-Sys', 'EcoNet-CEIM-PhoenixWater'
    -- Destination path within repo (directory).
    destination_path TEXT NOT NULL,           -- e.g. 'db', 'schemas', 'src/ker'
    -- Filename of the artifact.
    filename        TEXT NOT NULL,
    -- Optional ALN-style model or kernel identifier.
    kernel_id       TEXT,                     -- e.g. 'ecosafety.Vt.core2026v1', 'roh.model.mt6883.detox.v1'
    -- Attribution DID or Bostrom address.
    owner_alnid     TEXT NOT NULL DEFAULT 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    -- Whether this definition is active for new workloads.
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    -- Monotone non-regression flag (KER monotone_ok).
    monotone_ok     INTEGER NOT NULL DEFAULT 0 CHECK (monotone_ok IN (0,1)),
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    UNIQUE (def_name, version_label, artifact_kind, repo_target)
);

CREATE INDEX IF NOT EXISTS idx_definition_registry_scope_name
    ON definition_registry(scope_id, def_name);

-------------------------------------------------------------------------------
-- 2. Lane promotion and corridor tightening harness definitions
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO definition_scope (scope_name, description) VALUES
    ('LANE_POLICY',         'Lane governance policies and lane status shards'),
    ('KER_KERNEL',          'KER and Lyapunov residual kernels'),
    ('ROH_MODEL',           'Risk-of-Harm models and healthcare corridors'),
    ('PLACEMENT_POLICY',    'Placement cost and eco routing policies'),
    ('TOPOLOGY_AUDIT',      'Topology audit kernels and status shards'),
    ('DETOX_CORRIDOR',      'MT6883 detox corridor models'),
    ('QPU_CATALOG',         'QPU shard catalogs and virtual hardware'),
    ('ECO_WEALTH',          'Ecowealth views and EcoUnit scalars'),
    ('LANE_HARNESS',        'Kani harnesses for lane promotion and corridor tightening'),
    ('VIEW_GRAMMAR',        'Views over shardinstance, KER, grammar tables');

-------------------------------------------------------------------------------
-- 3. Seed rows for SQL / ALN / Rust artifacts (selected set)
-------------------------------------------------------------------------------

-- 3.1 Ecosafety grammar and plane weights (Eco-Fort)
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'EcosafetyGrammar2026v1',
    scope_id, '2026.v1', 'ALN',
    'Eco-Fort', 'schemas', 'EcosafetyGrammar2026v1.aln',
    'ecosafety.Vt.core2026v1',
    1, 1, '2026-05-02T20:20:00Z', '2026-05-02T20:20:00Z'
FROM definition_scope WHERE scope_name = 'KER_KERNEL';

INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'PlaneWeightsShard2026v1',
    scope_id, '2026.v1', 'SQL',
    'Eco-Fort', 'db', 'dbplaneweightsschema.sql',
    'ecosafety.Vt.core2026v1',
    1, 1, '2026-05-02T20:25:00Z', '2026-05-02T20:25:00Z'
FROM definition_scope WHERE scope_name = 'KER_KERNEL';

-------------------------------------------------------------------------------
-- 3.2 Lane governance and status (Virta-Sys / Eco-Fort)
-------------------------------------------------------------------------------

-- Virta-Sys lane governance SQL.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dbvirtalanegovernance.sql',
    scope_id, '2026.v1', 'SQL',
    'Virta-Sys', 'db', 'dbvirtalanegovernance.sql',
    'lane.policy.global2026v1',
    1, 1, '2026-05-02T21:00:00Z', '2026-05-02T21:00:00Z'
FROM definition_scope WHERE scope_name = 'LANE_POLICY';

-- Eco-Fort LaneStatus shard schema.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dblanestatusshardschema.sql',
    scope_id, '2026.v1', 'SQL',
    'Eco-Fort', 'db', 'dblanestatusshardschema.sql',
    'lane.status.shard2026v1',
    1, 1, '2026-05-02T21:05:00Z', '2026-05-02T21:05:00Z'
FROM definition_scope WHERE scope_name = 'LANE_POLICY';

-- LaneStatusShard2026v1 ALN particle.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'LaneStatusShard2026v1',
    scope_id, '2026.v1', 'ALN',
    'Eco-Fort', 'schemas', 'LaneStatusShard2026v1.aln',
    'lane.status.shard2026v1',
    1, 1, '2026-05-02T21:10:00Z', '2026-05-02T21:10:00Z'
FROM definition_scope WHERE scope_name = 'LANE_POLICY';

-------------------------------------------------------------------------------
-- 3.3 KER views over shardinstance (Eco-Fort)
-------------------------------------------------------------------------------

-- vshardkergrammar view.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'vshardkergrammar',
    scope_id, '2026.v1', 'VIEW',
    'Eco-Fort', 'db', 'dbgrammar_ker_views.sql',
    'ker.shard.grammar.view2026v1',
    1, 1, '2026-05-02T21:20:00Z', '2026-05-02T21:20:00Z'
FROM definition_scope WHERE scope_name = 'VIEW_GRAMMAR';

-- vshardresidual view (shardresidualview).
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'shardresidualview',
    scope_id, '2026.v1', 'VIEW',
    'Eco-Fort', 'db', 'dbgrammar_ker_views.sql',
    'ker.shard.residual.view2026v1',
    1, 1, '2026-05-02T21:22:00Z', '2026-05-02T21:22:00Z'
FROM definition_scope WHERE scope_name = 'VIEW_GRAMMAR';

-------------------------------------------------------------------------------
-- 3.4 Topology audit and blastradius (Virta-Sys / Eco-Fort)
-------------------------------------------------------------------------------

-- Topology audit SQL (Virta-Sys).
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dbvirtatopologyaudit.sql',
    scope_id, '2026.v1', 'SQL',
    'Virta-Sys', 'db', 'dbvirtatopologyaudit.sql',
    'topology.audit.kernel2026v1',
    1, 1, '2026-05-02T21:30:00Z', '2026-05-02T21:30:00Z'
FROM definition_scope WHERE scope_name = 'TOPOLOGY_AUDIT';

-- Blastradius object table (Eco-Fort).
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dbblastradiusindex.sql',
    scope_id, '2026.v1', 'SQL',
    'Eco-Fort', 'db', 'dbblastradiusindex.sql',
    'blast.radius.template2026v1',
    1, 1, '2026-05-02T21:35:00Z', '2026-05-02T21:35:00Z'
FROM definition_scope WHERE scope_name = 'TOPOLOGY_AUDIT';

-------------------------------------------------------------------------------
-- 3.5 MT6883 healthcare / detox RoH models (EcoNet-CEIM-PhoenixWater)
-------------------------------------------------------------------------------

-- Corridor vault objects (MT6883 healthcare).
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dbcorridorvaultobjects.sql',
    scope_id, '2026.v1', 'SQL',
    'EcoNet-CEIM-PhoenixWater', 'db', 'dbcorridorvaultobjects.sql',
    'roh.model.mt6883.corridorvault2026v1',
    1, 1, '2026-05-02T21:40:00Z', '2026-05-02T21:40:00Z'
FROM definition_scope WHERE scope_name = 'ROH_MODEL';

-- Detox state snapshot (EcoNet-CEIM-PhoenixWater).
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dbcorridorvaultdetoxstate.sql',
    scope_id, '2026.v1', 'SQL',
    'EcoNet-CEIM-PhoenixWater', 'db', 'dbcorridorvaultdetoxstate.sql',
    'roh.model.mt6883.detoxstate2026v1',
    1, 1, '2026-05-02T21:42:00Z', '2026-05-02T21:42:00Z'
FROM definition_scope WHERE scope_name = 'ROH_MODEL';

-- Detox corridor ALN particle nanoswarm.detox.corridor.v1.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'nanoswarm.detox.corridor.v1',
    scope_id, 'v1', 'ALN',
    'EcoNet-CEIM-PhoenixWater', 'policies', 'nanoswarm.detox.corridor.v1.aln',
    'roh.model.mt6883.detox.v1',
    1, 1, '2026-05-02T21:45:00Z', '2026-05-02T21:45:00Z'
FROM definition_scope WHERE scope_name = 'DETOX_CORRIDOR';

-------------------------------------------------------------------------------
-- 3.6 QPU catalog and MT6883 shard catalogs (Eco-Fort / MT6883 repo)
-------------------------------------------------------------------------------

-- qpushardcatalog core schema.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dbqpushardcatalog.sql',
    scope_id, '2026.v1', 'SQL',
    'Eco-Fort', 'db', 'dbqpushardcatalog.sql',
    'qpu.catalog.core2026v1',
    1, 1, '2026-05-02T21:50:00Z', '2026-05-02T21:50:00Z'
FROM definition_scope WHERE scope_name = 'QPU_CATALOG';

-- qpuvirtualhardware.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'dbqpuvirtualhardware.sql',
    scope_id, '2026.v1', 'SQL',
    'Eco-Fort', 'db', 'dbqpuvirtualhardware.sql',
    'qpu.hardware.catalog2026v1',
    1, 1, '2026-05-02T21:55:00Z', '2026-05-02T21:55:00Z'
FROM definition_scope WHERE scope_name = 'QPU_CATALOG';

-------------------------------------------------------------------------------
-- 3.7 Ecowealth / EcoUnit views (Eco-Fort / EcoNet-CEIM-PhoenixWater)
-------------------------------------------------------------------------------

-- ecowealthview view.
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'ecowealthview',
    scope_id, '2026.v1', 'VIEW',
    'EcoNet-CEIM-PhoenixWater', 'db', 'dbcorridorvaultecorewardsledger.sql',
    'ecowealth.view.Phoenix2026v1',
    1, 1, '2026-05-02T22:00:00Z', '2026-05-02T22:00:00Z'
FROM definition_scope WHERE scope_name = 'ECO_WEALTH';

-------------------------------------------------------------------------------
-- 3.8 Kani harness / lane promotion harnesses (Rust) registry only
-------------------------------------------------------------------------------

-- Lane promotion harness (Virta-Sys).
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'lane_promotion_kani.rs',
    scope_id, '2026.v1', 'RUST',
    'Virta-Sys', 'src/lanes', 'lane_promotion_kani.rs',
    'lane.promotion.harness2026v1',
    1, 1, '2026-05-02T22:05:00Z', '2026-05-02T22:05:00Z'
FROM definition_scope WHERE scope_name = 'LANE_HARNESS';

-- Corridor tightening harness (Virta-Sys).
INSERT OR IGNORE INTO definition_registry (
    def_name, scope_id, version_label, artifact_kind,
    repo_target, destination_path, filename, kernel_id,
    active, monotone_ok, created_utc, updated_utc
)
SELECT
    'corridor_tightening_kani.rs',
    scope_id, '2026.v1', 'RUST',
    'Virta-Sys', 'src/corridors', 'corridor_tightening_kani.rs',
    'corridor.tightening.harness2026v1',
    1, 1, '2026-05-02T22:10:00Z', '2026-05-02T22:10:00Z'
FROM definition_scope WHERE scope_name = 'LANE_HARNESS';
