-- ================================================================
-- GITHUB ACCOUNT MIGRATION SCRIPT
-- Updates all Doctor0Evil references to mk-bluebird
-- Date: 2026-05-12
-- Target: constellation_index.sql
-- ================================================================

PRAGMA foreign_keys = ON;

-- 1. Update all repository GitHub slugs
UPDATE repo 
SET github_slug = REPLACE(github_slug, 'Doctor0Evil/', 'mk-bluebird/eco_restoration_shard/tree/main/')
WHERE github_slug LIKE 'Doctor0Evil/%';

-- 2. Add migration note to description
UPDATE repo
SET description = description || ' [Migrated from Doctor0Evil/' || name || ' on 2026-05-12]'
WHERE github_slug LIKE '%mk-bluebird%' AND description NOT LIKE '%Migrated from%';

-- 3. Update did_owner if previously unset
UPDATE repo
SET did_owner = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
WHERE did_owner IS NULL OR did_owner = '';

-- 4. Create migration audit record
CREATE TABLE IF NOT EXISTS migration_audit (
    audit_id INTEGER PRIMARY KEY AUTOINCREMENT,
    migration_date TEXT NOT NULL,
    old_account TEXT NOT NULL,
    new_account TEXT NOT NULL,
    repos_affected INTEGER NOT NULL,
    notes TEXT,
    evidence_hex TEXT
);

INSERT INTO migration_audit (migration_date, old_account, new_account, repos_affected, notes, evidence_hex)
SELECT 
    '2026-05-12T17:15:00-07:00',
    'Doctor0Evil',
    'mk-bluebird',
    COUNT(*),
    'GitHub account suspension; consolidated all repositories into eco_restoration_shard monorepo',
    '8a7f3c9e1d2b5f4a'
FROM repo
WHERE github_slug LIKE '%mk-bluebird%';

-- 5. Verify migration
SELECT 
    'Migration Summary' AS report_section,
    COUNT(*) AS total_repos,
    SUM(CASE WHEN github_slug LIKE '%mk-bluebird%' THEN 1 ELSE 0 END) AS migrated_repos,
    SUM(CASE WHEN did_owner = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7' THEN 1 ELSE 0 END) AS did_updated
FROM repo;
