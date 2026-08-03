-- File: sql/mcp_repo_ker_governance.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/mcp_repo_ker_governance.sql

PRAGMA foreign_keys = ON;

-- Add KER fields for repositories (high-level band scoring).
ALTER TABLE mcp_repo ADD COLUMN ker_k       REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_repo ADD COLUMN ker_e       REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_repo ADD COLUMN ker_r       REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_repo ADD COLUMN ker_s       REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_repo ADD COLUMN neuroflag   INTEGER NOT NULL DEFAULT 0 CHECK (neuroflag IN (0, 1));
ALTER TABLE mcp_repo ADD COLUMN citizen_ready INTEGER NOT NULL DEFAULT 0 CHECK (citizen_ready IN (0, 1));

-- KER bounds and scalar consistency at repo level.
DROP TRIGGER IF EXISTS trg_mcp_repo_ker_invariants;

CREATE TRIGGER trg_mcp_repo_ker_invariants
BEFORE INSERT ON mcp_repo
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'mcp_repo.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'mcp_repo.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'mcp_repo.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'mcp_repo.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_repo_ker_invariants_update;

CREATE TRIGGER trg_mcp_repo_ker_invariants_update
BEFORE UPDATE ON mcp_repo
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'mcp_repo.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'mcp_repo.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'mcp_repo.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'mcp_repo.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;
