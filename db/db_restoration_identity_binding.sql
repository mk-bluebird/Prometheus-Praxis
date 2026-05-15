-- filename: db_restoration_identity_binding.sql
-- destination: eco_restoration_shard/db/db_restoration_identity_binding.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Identity binding table for Bostrom authorship

CREATE TABLE IF NOT EXISTS restoration_identity_binding (
    binding_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    bostrom_address  TEXT NOT NULL,
    logicalname      TEXT NOT NULL,
    repo_target      TEXT NOT NULL,
    file_path        TEXT NOT NULL,
    region           TEXT NOT NULL,
    scope            TEXT NOT NULL,
    dbrole           TEXT NOT NULL,
    contractid       TEXT NOT NULL,
    comment          TEXT,
    createdutc       TEXT NOT NULL,
    updatedutc       TEXT NOT NULL,
    UNIQUE (bostrom_address, logicalname, file_path)
);

CREATE INDEX IF NOT EXISTS idx_restoration_identity_binding_addr
ON restoration_identity_binding (
    bostrom_address,
    region,
    scope,
    dbrole
);

-- 2. Register eco_restoration_shard governance DB and Phoenix artifacts
--    as authored by PRIMARY Bostrom address.

INSERT OR IGNORE INTO restoration_identity_binding (
    bostrom_address,
    logicalname,
    repo_target,
    file_path,
    region,
    scope,
    dbrole,
    contractid,
    comment,
    createdutc,
    updatedutc
) VALUES
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restorationindex.governance.sqlite3',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/restorationindex.sqlite3',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    'EcosafetyContinuityPhoenix2026v1',
    'Primary author and steward for Phoenix restorationindex governance DB.',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.blastradius.phoenix',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_restoration_blastradius_phoenix.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    'EcosafetyContinuityPhoenix2026v1',
    'Author of restoration-aware blast-radius schema and views for Phoenix.',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'energy.ecoperjoule.policy.phoenix',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_ecoperjoule_policy_energy.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    'EcoplaneEnergyPhoenix2026v1',
    'Author of eco-per-joule policy and governance views for Phoenix energy planes.',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'mt6883.lane.continuity',
    'github.com/mk-bluebird/eco_restoration_shard',
    'db/db_mt6883_lane_continuity.sql',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    'Mt6883ContinuityPhoenix2026v1',
    'Author of MT6883 continuity and neuroethic lane views.',
    datetime('now'),
    datetime('now')
),
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.energy.mt6883.tool',
    'github.com/mk-bluebird/eco_restoration_shard',
    'src/tools/restoration_energy_mt6883_tool.cpp',
    'Phoenix-AZ',
    'REGION',
    'GOVERNANCE',
    'RestorationToolingPhoenix2026v1',
    'Author of non-actuating governance tool for restoration, energy, and MT6883 queries.',
    datetime('now'),
    datetime('now')
);
