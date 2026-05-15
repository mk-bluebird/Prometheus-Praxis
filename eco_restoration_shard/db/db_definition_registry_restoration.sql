-- filename: db_definition_registry_restoration.sql
-- destination: eco_restoration_shard/db/db_definition_registry_restoration.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: restoration.definitionregistry.phoenix
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- =============================================================================
-- DefinitionRegistry for Restoration Artifacts
-- Tracks all Phoenix restoration/energy/MT6883 SQL, ALN, and tool artifacts
-- with logicalnames, hashes, and registration status.
-- =============================================================================

CREATE TABLE IF NOT EXISTS definitionregistry_restoration (
    definition_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    logicalname      TEXT NOT NULL UNIQUE,   -- e.g., restoration.blastradius.phoenix.2026v1
    versiontag       TEXT NOT NULL,          -- e.g., 2026v1
    artifact_type    TEXT NOT NULL,          -- SQL_SCHEMA, VIEW, TOOL_CPP, TOOL_RUST, CI_WORKFLOW, DOC
    file_path        TEXT NOT NULL,          -- Relative path to artifact file
    hash_sha256      TEXT,                   -- SHA256 hash of artifact content
    status           TEXT NOT NULL DEFAULT 'REGISTERED',  -- REGISTERED, ACTIVE, DEPRECATED, REVOKED
    contractid       TEXT,                   -- Optional smart contract reference
    repofileid       INTEGER,                -- Reference to repofile.repofile_id
    region           TEXT NOT NULL DEFAULT 'PHOENIX-AZ',
    scope            TEXT NOT NULL,          -- PHOENIX-AZ, GLOBAL_PATTERN, CONSTELLATION
    description      TEXT NOT NULL,
    author_bostrom   TEXT NOT NULL,
    registered_utc   TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc      TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (repofileid) REFERENCES repofile(repofile_id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_defreg_logicalname ON definitionregistry_restoration (logicalname);
CREATE INDEX IF NOT EXISTS idx_defreg_status ON definitionregistry_restoration (status);
CREATE INDEX IF NOT EXISTS idx_defreg_artifact_type ON definitionregistry_restoration (artifact_type);
CREATE INDEX IF NOT EXISTS idx_defreg_region_scope ON definitionregistry_restoration (region, scope);

-- =============================================================================
-- Register Phoenix governance artifacts in DefinitionRegistry
-- =============================================================================

-- 1. Repo Index (self-referential registration)
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'restoration.repoindex.phoenix',
    '2026v1',
    'SQL_SCHEMA',
    'db/db_repo_index.sql',
    NULL,
    'ACTIVE',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'Repo and repofile index tables for registering all governance artifacts with metadata.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 2. Blastradius restoration grammar
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'restoration.blastradius.phoenix.2026v1',
    '2026v1',
    'SQL_SCHEMA',
    'db/db_restoration_blastradius_phoenix.sql',
    NULL,
    'REGISTERED',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'Restoration-aware blastradius grammar with Phoenix-scoped views for blast radius and restoration nodes.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 3. Eco-per-joule policy grammar
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'energy.ecoperjoule.policy.phoenix.2026v1',
    '2026v1',
    'SQL_SCHEMA',
    'db/db_ecoperjoule_policy_energy.sql',
    NULL,
    'REGISTERED',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'Eco-per-joule policy grammar for Phoenix energy planes with Cyboquatic views.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 4. MT6883 lane continuity grammar
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'mt6883.lane.continuity.phoenix.2026v1',
    '2026v1',
    'SQL_SCHEMA',
    'db/db_mt6883_lane_continuity.sql',
    NULL,
    'REGISTERED',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'MT6883 continuity and neuroethic radii integration into lane and blast grammar.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 5. Identity binding schema
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'restoration.identitybinding.phoenix.2026v1',
    '2026v1',
    'SQL_SCHEMA',
    'db/db_restoration_identity_binding.sql',
    NULL,
    'REGISTERED',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'Identity bindings for all Phoenix governance artifacts linking Bostrom addresses to files.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 6. C++ governance tool
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'restoration.energy.mt6883.tool.cpp.phoenix',
    '2026v1',
    'TOOL_CPP',
    'src/tools/restoration_energy_mt6883_tool.cpp',
    NULL,
    'REGISTERED',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'Non-actuating C++ governance tool for querying restoration, energy, and MT6883 views.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 7. C++ tool JSON mode extension
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'restoration.energy.mt6883.tool.cpp.jsonmode',
    '2026v1',
    'TOOL_CPP',
    'src/tools/restoration_energy_mt6883_tool.cpp',
    NULL,
    'REGISTERED',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'JSON output mode extension for C++ governance tool with --format=json flag.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 8. Rust governance spine client
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'restoration.governance.spine.client.rust',
    '2026v1',
    'TOOL_RUST',
    'src/lib/governance_spine_client.rs',
    NULL,
    'REGISTERED',
    'GLOBAL_PATTERN',
    'CONSTELLATION',
    'Rust governance spine client wrapping restorationindex.sqlite3 with typed accessors.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 9. CI DefinitionRegistry enforcement
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'ci.definitionregistry.enforcement.phoenix',
    '2026v1',
    'CI_WORKFLOW',
    '.github/workflows/ci_definitionregistry.yml',
    NULL,
    'REGISTERED',
    'CONSTELLATION',
    'CONSTELLATION',
    'CI enforcement for DefinitionRegistry registration of all governance artifacts.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 10. CI identity binding enforcement
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'ci.identitybinding.enforcement.phoenix',
    '2026v1',
    'CI_WORKFLOW',
    '.github/workflows/ci_identity_binding.yml',
    NULL,
    'REGISTERED',
    'CONSTELLATION',
    'CONSTELLATION',
    'CI enforcement for identity bindings on all governance artifacts.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 11. Agent views
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'agent.views.phoenix.2026v1',
    '2026v1',
    'VIEW',
    'db/db_agent_views_phoenix.sql',
    NULL,
    'REGISTERED',
    'CONSTELLATION',
    'CONSTELLATION',
    'Agent-facing governance views for Phoenix with stable schemas for AI agents.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- 12. Governance blueprint documentation
INSERT OR REPLACE INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, hash_sha256, status,
    region, scope, description, author_bostrom
) VALUES (
    'docs.monorepo.governance.blueprint',
    '2026v1',
    'DOC',
    'docs/monorepo_governance_blueprint.md',
    NULL,
    'REGISTERED',
    'CONSTELLATION',
    'CONSTELLATION',
    'Monorepo governance blueprint documentation explaining consolidation patterns and usage.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

-- =============================================================================
-- Views for querying DefinitionRegistry
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_active_definitions AS
SELECT 
    logicalname,
    versiontag,
    artifact_type,
    file_path,
    region,
    scope,
    description,
    author_bostrom,
    registered_utc
FROM definitionregistry_restoration
WHERE status = 'ACTIVE' OR status = 'REGISTERED'
ORDER BY region, scope, artifact_type, logicalname;

CREATE VIEW IF NOT EXISTS v_definitions_by_type AS
SELECT 
    artifact_type,
    COUNT(*) as count,
    GROUP_CONCAT(logicalname, ', ') as logicalnames
FROM definitionregistry_restoration
WHERE status IN ('ACTIVE', 'REGISTERED')
GROUP BY artifact_type
ORDER BY count DESC;

CREATE VIEW IF NOT EXISTS v_unregistered_artifacts AS
-- Placeholder for CI to detect artifacts not in registry
SELECT 
    'SCAN_REQUIRED' AS note,
    'Use CI script to scan for SQL/CPP/RUST/YML files not in definitionregistry_restoration' AS action;
