-- filename: db_restoration_identity_binding.sql
-- destination: eco_restoration_shard/db/db_restoration_identity_binding.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard
-- logicalname: restoration.identitybinding.phoenix.2026v1
-- author: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

PRAGMA foreign_keys = ON;

-- =============================================================================
-- Restoration Identity Binding Table
-- Binds Bostrom addresses to governance artifacts for Phoenix-AZ
-- Ensures all governance files have explicit authorship and identity binding
-- =============================================================================

CREATE TABLE IF NOT EXISTS restoration_identity_binding (
    binding_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    bostrom_address  TEXT NOT NULL,  -- e.g., bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
    logicalname      TEXT NOT NULL,  -- Reference to definitionregistry_restoration.logicalname
    file_path        TEXT NOT NULL,  -- Relative path to bound file
    region           TEXT NOT NULL DEFAULT 'PHOENIX-AZ',
    scope            TEXT NOT NULL DEFAULT 'REGION',  -- REGION, CONSTELLATION, GLOBAL_PATTERN
    dbrole           TEXT NOT NULL,  -- GOVERNANCE, TOOLING, CI, AGENT_API, DOCS
    role_description TEXT NOT NULL,
    createdutc       TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc       TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (bostrom_address, logicalname, file_path)
);

CREATE INDEX IF NOT EXISTS idx_binding_bostrom ON restoration_identity_binding (bostrom_address);
CREATE INDEX IF NOT EXISTS idx_binding_logicalname ON restoration_identity_binding (logicalname);
CREATE INDEX IF NOT EXISTS idx_binding_file_path ON restoration_identity_binding (file_path);
CREATE INDEX IF NOT EXISTS idx_binding_region_scope ON restoration_identity_binding (region, scope);

-- =============================================================================
-- Populate identity bindings for all Phoenix governance artifacts
-- All bindings use bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7 as primary author
-- =============================================================================

-- 1. Repo Index binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.repoindex.phoenix',
    'db/db_repo_index.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author and maintainer of repo index schema for Phoenix-AZ governance artifact registration.'
);

-- 2. DefinitionRegistry binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.definitionregistry.phoenix',
    'db/db_definition_registry_restoration.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author and maintainer of DefinitionRegistry slice for restoration artifacts.'
);

-- 3. Blastradius restoration grammar binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.blastradius.phoenix.2026v1',
    'db/db_restoration_blastradius_phoenix.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author of restoration-aware blastradius grammar with Phoenix-scoped views.'
);

-- 4. Eco-per-joule policy grammar binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'energy.ecoperjoule.policy.phoenix.2026v1',
    'db/db_ecoperjoule_policy_energy.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author of eco-per-joule policy grammar for Phoenix energy planes.'
);

-- 5. MT6883 lane continuity grammar binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'mt6883.lane.continuity.phoenix.2026v1',
    'db/db_mt6883_lane_continuity.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author of MT6883 continuity and neuroethic radii integration grammar.'
);

-- 6. C++ governance tool binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.energy.mt6883.tool.cpp.phoenix',
    'src/tools/restoration_energy_mt6883_tool.cpp',
    'PHOENIX-AZ',
    'REGION',
    'TOOLING',
    'Primary author of non-actuating C++ governance tool for restoration, energy, and MT6883 queries.'
);

-- 7. C++ tool JSON mode binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.energy.mt6883.tool.cpp.jsonmode',
    'src/tools/restoration_energy_mt6883_tool.cpp',
    'PHOENIX-AZ',
    'REGION',
    'TOOLING',
    'Primary author of JSON output mode extension for C++ governance tool.'
);

-- 8. Rust governance spine client binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.governance.spine.client.rust',
    'src/lib/governance_spine_client.rs',
    'GLOBAL_PATTERN',
    'CONSTELLATION',
    'TOOLING',
    'Primary author of Rust governance spine client for cross-repo CI and agent access.'
);

-- 9. CI DefinitionRegistry enforcement binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'ci.definitionregistry.enforcement.phoenix',
    '.github/workflows/ci_definitionregistry.yml',
    'CONSTELLATION',
    'CONSTELLATION',
    'CI',
    'Primary author of CI enforcement for DefinitionRegistry registration of governance artifacts.'
);

-- 10. CI identity binding enforcement binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'ci.identitybinding.enforcement.phoenix',
    '.github/workflows/ci_identity_binding.yml',
    'CONSTELLATION',
    'CONSTELLATION',
    'CI',
    'Primary author of CI enforcement for identity bindings on governance artifacts.'
);

-- 11. Agent views binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'agent.views.phoenix.2026v1',
    'db/db_agent_views_phoenix.sql',
    'CONSTELLATION',
    'CONSTELLATION',
    'AGENT_API',
    'Primary author of agent-facing governance views for Phoenix with stable schemas.'
);

-- 12. Governance blueprint documentation binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'docs.monorepo.governance.blueprint',
    'docs/monorepo_governance_blueprint.md',
    'CONSTELLATION',
    'CONSTELLATION',
    'DOCS',
    'Primary author of monorepo governance blueprint documentation.'
);

-- 13. Monorepo task table binding (meta-binding for the task system itself)
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.taskindex.phoenix',
    'db/tasks_monorepo_consolidation.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author of monorepo_task table for tracking consolidation tasks.'
);

-- 14. Task insertions binding
INSERT OR REPLACE INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.taskinserts.phoenix',
    'db/insert_tasks_monorepo_consolidation.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author of seeded task insertions for monorepo consolidation.'
);

-- =============================================================================
-- Views for querying identity bindings
-- =============================================================================

-- View showing all active identity bindings
CREATE VIEW IF NOT EXISTS v_active_identity_bindings AS
SELECT 
    bostrom_address,
    logicalname,
    file_path,
    region,
    scope,
    dbrole,
    role_description,
    createdutc,
    updatedutc
FROM restoration_identity_binding
ORDER BY region, scope, dbrole, logicalname;

-- View showing bindings by Bostrom address
CREATE VIEW IF NOT EXISTS v_bindings_by_author AS
SELECT 
    bostrom_address,
    COUNT(*) as binding_count,
    GROUP_CONCAT(DISTINCT dbrole, ', ') as dbroles,
    GROUP_CONCAT(DISTINCT region, ', ') as regions
FROM restoration_identity_binding
GROUP BY bostrom_address
ORDER BY binding_count DESC;

-- View showing unbound governance files (placeholder for CI checks)
CREATE VIEW IF NOT EXISTS v_unbound_governance_files AS
SELECT 
    'SCAN_REQUIRED' AS note,
    'Use CI script to verify all governance files have corresponding restoration_identity_binding entries' AS action;

-- View summarizing bindings by role
CREATE VIEW IF NOT EXISTS v_bindings_summary_by_dbrole AS
SELECT 
    dbrole,
    COUNT(*) as binding_count,
    GROUP_CONCAT(logicalname, ', ') as logicalnames
FROM restoration_identity_binding
GROUP BY dbrole
ORDER BY binding_count DESC;
