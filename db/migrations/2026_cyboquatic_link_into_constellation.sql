-- filename: db/migrations/2026_cyboquatic_link_into_constellation.sql
-- destination: eco_restoration_shard/db/migrations/2026_cyboquatic_link_into_constellation.sql
-- Purpose:
-- - Register the Cyboquatic spine DB and primary machinery artifacts
--   into the higher-level EcoNet constellation index.
-- - This lets agents correlate repo-level KER, blast radius, and energy
--   metrics with Cyboquatic machinery evidence.

PRAGMA foreign_keys = ON;

-- Assume main constellation index tables from econetconstellationindex.sql:
--   repositories, artifacts, kerscores, blastradius, energymetrics

INSERT OR IGNORE INTO repositories (
    reponame, githuburl, primarylanguage, description, bostromanchor
) VALUES (
    'eco_restoration_shard',
    'https://github.com/mk-bluebird/eco_restoration_shard',
    'Rust',
    'Eco-restoration research and Cyboquatic materials/evidence spine.',
    '0xECO2026RESTORATIONSHARDbostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

INSERT OR IGNORE INTO repositories (
    reponame, githuburl, primarylanguage, description, bostromanchor
) VALUES (
    'Cyboquatics',
    'https://github.com/mk-bluebird/Cyboquatics',
    'Rust',
    'Cyboquatic industrial machinery kernels, governed by EcoNet and eco_restoration_shard.',
    '0xECO2026RESTORATIONSHARDbostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

INSERT OR IGNORE INTO artifacts (
    repoid, artifactpath, artifacttype, language, hexstamp
)
SELECT
    r.repoid,
    'dataconstellation/econet_cyboquatic_machinery_spine.sql',
    'schema',
    'SQL',
    '0xCYBOQUATICSPINE2026'
FROM repositories r
WHERE r.reponame = 'eco_restoration_shard';

INSERT OR IGNORE INTO artifacts (
    repoid, artifactpath, artifacttype, language, hexstamp
)
SELECT
    r.repoid,
    'src/econet_cyboquatic_index.rs',
    'source',
    'Rust',
    '0xCYBOQUATICINDEXRS2026'
FROM repositories r
WHERE r.reponame = 'eco_restoration_shard';
