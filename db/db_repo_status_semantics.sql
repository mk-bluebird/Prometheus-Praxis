-- filename db_repo_status_semantics.sql
-- destination Eco-Fort/db/db_repo_status_semantics.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 45. Repo status semantics for deleted GitHub repositories
-------------------------------------------------------------------------------

-- Extend repo with status column.
ALTER TABLE repo
ADD COLUMN status TEXT NOT NULL DEFAULT 'ACTIVE'
CHECK (status IN ('ACTIVE','ARCHIVED','DELETED'));

-- Successor resolution view that skips DELETED repos.
CREATE VIEW IF NOT EXISTS v_repo_successor_resolved AS
WITH RECURSIVE chain AS (
    SELECT
        r.repoid,
        r.name,
        r.githubslug,
        r.roleband,
        r.status,
        r.successor_repo,
        r.repoid AS root_repoid
    FROM repo r
    WHERE r.status <> 'DELETED'
    UNION ALL
    SELECT
        r2.repoid,
        r2.name,
        r2.githubslug,
        r2.roleband,
        r2.status,
        r2.successor_repo,
        c.root_repoid
    FROM repo r2
    JOIN chain c ON c.successor_repo = r2.repoid
    WHERE r2.status <> 'DELETED'
)
SELECT DISTINCT
    root_repoid,
    repoid,
    name,
    githubslug,
    roleband,
    status
FROM chain;

-- Lane assignment semantics:
--   - ACTIVE: normal use; lanes may be RESEARCH, EXPPROD, PROD per repo design.
--   - ARCHIVED: repo remains as historical evidence; new lanes default to RESEARCH only.
--   - DELETED: repo is excluded from successor chains and lane resolution;
--              shards remain in shardinstance for evidence, but new bindings
--              must point to a non-deleted successor.
