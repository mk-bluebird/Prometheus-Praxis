-- filename: db_repo_index.sql
-- destination: eco_restoration_shard/db/db_repo_index.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: restoration.repoindex.phoenix
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- =============================================================================
-- Repo and Repofile Index Tables
-- Tracks all governance-relevant files in eco_restoration_shard with metadata
-- for Phoenix-AZ constellation governance.
-- =============================================================================

CREATE TABLE IF NOT EXISTS repo (
    repo_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_target      TEXT NOT NULL UNIQUE,  -- e.g., github.com/mk-bluebird/eco_restoration_shard
    region           TEXT NOT NULL DEFAULT 'PHOENIX-AZ',
    purpose          TEXT NOT NULL,         -- GOVERNANCE_DB, TOOLING, CI, AGENT_API, DOCS
    description      TEXT NOT NULL,
    createdutc       TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc       TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS repofile (
    repofile_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id          INTEGER NOT NULL,
    file_path        TEXT NOT NULL,         -- Relative path within repo
    language         TEXT NOT NULL,         -- SQL, CPP, RUST, MD, YML, ALN
    purpose          TEXT NOT NULL,         -- GOVERNANCE_DB, TOOLING, CI, AGENT_API, DOCS
    description      TEXT NOT NULL,
    hash_sha256      TEXT,                  -- SHA256 hash of file content
    registered_utc   TEXT NOT NULL DEFAULT (datetime('now')),
    updated_utc      TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (repo_id) REFERENCES repo(repo_id) ON DELETE CASCADE,
    UNIQUE (repo_id, file_path)
);

CREATE INDEX IF NOT EXISTS idx_repofile_repo_id ON repofile (repo_id);
CREATE INDEX IF NOT EXISTS idx_repofile_language ON repofile (language);
CREATE INDEX IF NOT EXISTS idx_repofile_purpose ON repofile (purpose);
CREATE INDEX IF NOT EXISTS idx_repofile_file_path ON repofile (file_path);

-- =============================================================================
-- Register the eco_restoration_shard repository
-- =============================================================================

INSERT OR IGNORE INTO repo (repo_target, region, purpose, description)
VALUES (
    'github.com/mk-bluebird/eco_restoration_shard',
    'PHOENIX-AZ',
    'GOVERNANCE_DB',
    'Monorepo shard consolidating restoration, energy, and MT6883 governance functions for the Phoenix-AZ constellation.'
);

-- =============================================================================
-- Register all governance-relevant files
-- =============================================================================

-- Get the repo_id for eco_restoration_shard
-- Note: Using a subquery to reference the repo_id

-- DB schema files
INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/tasks_monorepo_consolidation.sql',
    'SQL',
    'GOVERNANCE_DB',
    'Defines monorepo_task table for tracking consolidation tasks across governance categories.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/insert_tasks_monorepo_consolidation.sql',
    'SQL',
    'GOVERNANCE_DB',
    'Seeds 13 concrete tasks for monorepo consolidation including DB, tooling, CI, agent API, and docs.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/monorepo_task_index.csv',
    'CSV',
    'GOVERNANCE_DB',
    'Lightweight CSV index mirroring monorepo_task entries for GitHub-friendly human and agent access.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/db_repo_index.sql',
    'SQL',
    'GOVERNANCE_DB',
    'Repo and repofile index tables for registering all governance artifacts with metadata.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/db_definition_registry_restoration.sql',
    'SQL',
    'GOVERNANCE_DB',
    'DefinitionRegistry slice for restoration artifacts with logicalnames, hashes, and status tracking.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/db_restoration_blastradius_phoenix.sql',
    'SQL',
    'GOVERNANCE_DB',
    'Restoration-aware blastradius grammar with Phoenix-scoped views for blast radius and restoration nodes.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/db_ecoperjoule_policy_energy.sql',
    'SQL',
    'GOVERNANCE_DB',
    'Eco-per-joule policy grammar for Phoenix energy planes with Cyboquatic views.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/db_mt6883_lane_continuity.sql',
    'SQL',
    'GOVERNANCE_DB',
    'MT6883 continuity and neuroethic radii integration into lane and blast grammar.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/db_restoration_identity_binding.sql',
    'SQL',
    'GOVERNANCE_DB',
    'Identity bindings for all Phoenix governance artifacts linking Bostrom addresses to files.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'db/db_agent_views_phoenix.sql',
    'SQL',
    'AGENT_API',
    'Agent-facing governance views for Phoenix with stable schemas for AI agents.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

-- Tooling files
INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'src/tools/restoration_energy_mt6883_tool.cpp',
    'CPP',
    'TOOLING',
    'Non-actuating C++ governance tool for querying restoration, energy, and MT6883 views.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'src/lib/governance_spine_client.rs',
    'RUST',
    'TOOLING',
    'Rust governance spine client wrapping restorationindex.sqlite3 with typed accessors.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

-- CI workflow files
INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    '.github/workflows/ci_definitionregistry.yml',
    'YML',
    'CI',
    'CI enforcement for DefinitionRegistry registration of all governance artifacts.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    '.github/workflows/ci_identity_binding.yml',
    'YML',
    'CI',
    'CI enforcement for identity bindings on all governance artifacts.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

-- Documentation files
INSERT OR REPLACE INTO repofile (repo_id, file_path, language, purpose, description, hash_sha256)
SELECT 
    r.repo_id,
    'docs/monorepo_governance_blueprint.md',
    'MD',
    'DOCS',
    'Monorepo governance blueprint documentation explaining consolidation patterns and usage.',
    NULL
FROM repo r WHERE r.repo_target = 'github.com/mk-bluebird/eco_restoration_shard';

-- =============================================================================
-- Views for querying repo index
-- =============================================================================

CREATE VIEW IF NOT EXISTS v_registered_files AS
SELECT 
    r.repo_target,
    rf.file_path,
    rf.language,
    rf.purpose,
    rf.description,
    rf.hash_sha256,
    rf.registered_utc
FROM repofile rf
JOIN repo r ON rf.repo_id = r.repo_id
ORDER BY rf.purpose, rf.file_path;

CREATE VIEW IF NOT EXISTS v_unregistered_governance_files AS
-- Placeholder view for CI to identify files that should be registered but aren't
-- This would be populated by scanning the filesystem at CI runtime
SELECT 
    'SCAN_REQUIRED' AS file_path,
    'Use CI script to scan db/, src/, and docs/ for unregistered files' AS note;
