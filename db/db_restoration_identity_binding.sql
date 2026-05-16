-- filename: db/db_restoration_identity_binding.sql
-- destination: eco_restoration_shard/db/db_restoration_identity_binding.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Identity binding table for Bostrom authorship (normalized)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS restoration_identity_binding (
    binding_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    bostrom_address TEXT NOT NULL,
    logical_name    TEXT NOT NULL,   -- e.g. restoration.governance.index.2026v1
    repo_target     TEXT NOT NULL,   -- e.g. github.com/mk-bluebird/eco_restoration_shard
    file_path       TEXT NOT NULL,   -- path relative to repo root
    region          TEXT NOT NULL,   -- e.g. Phoenix-AZ
    scope           TEXT NOT NULL,   -- CONSTELLATION, REGION, NODE, DOC
    db_role         TEXT NOT NULL,   -- GOVERNANCE_DB, SCHEMA, TOOL, DOC
    contract_id     TEXT NOT NULL,   -- logical contract identifier
    comment         TEXT NOT NULL,
    evidence_hex    TEXT NOT NULL,
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    UNIQUE (bostrom_address, logical_name, file_path)
);

CREATE INDEX IF NOT EXISTS idx_restoration_identity_binding_addr
ON restoration_identity_binding (
    bostrom_address,
    region,
    scope,
    db_role
);

CREATE INDEX IF NOT EXISTS idx_restoration_identity_binding_file
ON restoration_identity_binding (
    file_path,
    region
);

----------------------------------------------------------------------
-- 2. Register eco_restoration_shard governance DB and Phoenix artifacts
--    as authored by PRIMARY Bostrom address.
----------------------------------------------------------------------

INSERT OR IGNORE INTO restoration_identity_binding (
    bostrom_address,
    logical_name,
    repo_target,
    file_path,
    region,
    scope,
    db_role,
    contract_id,
    comment,
    evidence_hex,
    created_utc,
    updated_utc
) VALUES
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.governance.index.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/restorationindex.sqlite3',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE_DB',
    'EcosafetyContinuityPhoenix2026v1',
    'Primary author and steward for Phoenix restorationindex governance DB.',
    '0000000000000000000000000000000000000000000000000000000000000001',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.repoindex.schema.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/dbrepoindexrestoration.sql',
    'Phoenix-AZ',
    'REGION',
    'SCHEMA',
    'EcosafetyContinuityPhoenix2026v1',
    'Repository and file index schema for eco_restoration_shard.',
    '0000000000000000000000000000000000000000000000000000000000000002',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.bostrom.provenance.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/dbbostromprovenance.sql',
    'Phoenix-AZ',
    'REGION',
    'SCHEMA',
    'EcosafetyContinuityPhoenix2026v1',
    'Bostrom address and restoration contract provenance schema for eco_restoration_shard.',
    '0000000000000000000000000000000000000000000000000000000000000003',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.identity.binding.schema.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_restoration_identity_binding.sql',
    'Phoenix-AZ',
    'REGION',
    'SCHEMA',
    'EcosafetyContinuityPhoenix2026v1',
    'Identity-binding schema linking eco_restoration_shard artifacts to Bostrom addresses.',
    '0000000000000000000000000000000000000000000000000000000000000004',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.blastradius.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_restoration_blastradius_phoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE_DB',
    'EcosafetyContinuityPhoenix2026v1',
    'Author of restoration-aware blast-radius schema and views for Phoenix.',
    '0000000000000000000000000000000000000000000000000000000000000005',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'energy.ecoperjoule.policy.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_ecoperjoule_policy_energy.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE_DB',
    'EcoplaneEnergyPhoenix2026v1',
    'Author of eco-per-joule policy and governance views for Phoenix energy planes.',
    '0000000000000000000000000000000000000000000000000000000000000006',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'mt6883.lane.continuity.phoenix.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_mt6883_lane_continuity.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE_DB',
    'Mt6883ContinuityPhoenix2026v1',
    'Author of MT6883 continuity and neuroethic lane views.',
    '0000000000000000000000000000000000000000000000000000000000000007',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.energy.mt6883.tool.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'src/tools/restoration_energy_mt6883_tool.cpp',
    'Phoenix-AZ',
    'REGION',
    'TOOL',
    'RestorationToolingPhoenix2026v1',
    'Author of non-actuating governance tool for restoration, energy, and MT6883 queries.',
    '0000000000000000000000000000000000000000000000000000000000000008',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.index.cli.rust.2026v1',
    'github.com/mk-bluebird/eco_restoration_shard',
    'src/bin/restorationindextool.rs',
    'Phoenix-AZ',
    'REGION',
    'TOOL',
    'EcosafetyContinuityPhoenix2026v1',
    'Non-actuating Rust tool for querying restorationindex.sqlite3 governance surfaces.',
    '0000000000000000000000000000000000000000000000000000000000000009',
    datetime('now'),
    datetime('now')
);
