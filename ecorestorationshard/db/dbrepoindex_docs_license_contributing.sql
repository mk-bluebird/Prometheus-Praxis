-- filename: dbrepoindex_docs_license_contributing.sql
-- destination: ecorestorationshard/db/dbrepoindex_docs_license_contributing.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 45. Objection:
-- The eco_restoration_shard repo lacks a LICENSE file and a CONTRIBUTING.md
-- that specify governance contracts and expectations for external contributors.
-- Without these, legal terms and governance rules are implicit, and the
-- repo index lacks DOC_SPEC entries for these foundational documents.

-- Step 1: Add LICENSE and CONTRIBUTING.md to repofile as DOC_SPEC.

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language
)
SELECT
    r.repoid,
    'LICENSE',
    'DOC_SPEC',
    'TEXT'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

INSERT OR IGNORE INTO repofile (
    repoid,
    relpath,
    purpose,
    language
)
SELECT
    r.repoid,
    'CONTRIBUTING.md',
    'DOC_SPEC',
    'MARKDOWN'
FROM repo AS r
WHERE r.name = 'eco_restoration_shard';

-- Step 2: Bind CONTRIBUTING.md into restorationidentitybinding with governance contract.

INSERT OR IGNORE INTO restorationidentitybinding (
    bostromaddress,
    logicalname,
    repotarget,
    filepath,
    region,
    scope,
    dbrole,
    contractid,
    comment,
    createdutc,
    updatedutc
)
VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.contributing.phoenix.2026v1',
    'github.commk-bluebirdecorestorationshard',
    'CONTRIBUTING.md',
    'Phoenix-AZ',
    'REGION',
    'DOC_SPEC',
    'RestorationContributingPhoenix2026v1',
    'Contribution guidelines bound to Phoenix restoration governance contracts.',
    '2026-01-01T00:00:00Z',
    '2026-01-01T00:00:00Z'
);

-- Step 3: Optionally register CONTRIBUTING.md in definitionregistryrestoration
-- as a governance doc spec (commented here for explicitness).

-- INSERT OR IGNORE INTO definitionregistryrestoration (
--     logicalname,
--     versiontag,
--     hash,
--     status,
--     contractid,
--     repofileid
-- )
-- SELECT
--     'restoration.contributing.phoenix.2026v1',
--     '2026v1',
--     '0x' || lower(hex(randomblob(16))),
--     'ACTIVE',
--     'RestorationContributingPhoenix2026v1',
--     rf.fileid
-- FROM repofile AS rf
-- JOIN repo AS r
--   ON r.repoid = rf.repoid
-- WHERE r.name   = 'eco_restoration_shard'
--   AND rf.relpath = 'CONTRIBUTING.md';
