-- filename: tasks_monorepo_consolidation.sql
-- destination: eco_restoration_shard/db/tasks_monorepo_consolidation.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS monorepo_task (
    task_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    category       TEXT NOT NULL,  -- GOVERNANCE_DB, TOOLING, CI, AGENT_API, DOCS
    scope          TEXT NOT NULL,  -- PHOENIX-AZ, GLOBAL_PATTERN, CONSTELLATION
    title          TEXT NOT NULL,
    description    TEXT NOT NULL,
    repo_target    TEXT NOT NULL,
    file_path      TEXT NOT NULL,
    status         TEXT NOT NULL DEFAULT 'OPEN',  -- OPEN, IN_PROGRESS, DONE
    priority       TEXT NOT NULL DEFAULT 'HIGH',  -- HIGH, MEDIUM, LOW
    depends_on     TEXT,       -- comma-separated task_ids or logicalnames
    logicalname    TEXT NOT NULL,
    author_bostrom TEXT NOT NULL,
    createdutc     TEXT NOT NULL,
    updatedutc     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_monorepo_task_category
ON monorepo_task (category, scope, status);

CREATE INDEX IF NOT EXISTS idx_monorepo_task_logicalname
ON monorepo_task (logicalname);
