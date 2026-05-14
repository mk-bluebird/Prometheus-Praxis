-- filename db_repo_doctor0evil_to_ecorestoration_deprecate.sql
-- destination Eco-Fort/db/db_repo_doctor0evil_to_ecorestoration_deprecate.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 29. Deprecate Doctor0Evil repos in favor of eco_restoration_shard
-------------------------------------------------------------------------------

-- Assumes:
--   repo:
--     repoid INTEGER PRIMARY KEY
--     name TEXT UNIQUE
--     githubslug TEXT NOT NULL
--     roleband TEXT NOT NULL
--     successor_repo INTEGER

UPDATE repo
SET successor_repo = (
        SELECT repoid
        FROM repo
        WHERE name = 'eco_restoration_shard'
    ),
    roleband = 'RESEARCH'
WHERE githubslug LIKE 'Doctor0Evil/%'
  AND name <> 'eco_restoration_shard';
