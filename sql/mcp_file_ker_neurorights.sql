-- File: sql/mcp_file_ker_neurorights.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/mcp_file_ker_neurorights.sql

PRAGMA foreign_keys = ON;

-- Per-file KER triad and scalar.
ALTER TABLE mcp_file ADD COLUMN ker_k        REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_file ADD COLUMN ker_e        REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_file ADD COLUMN ker_r        REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_file ADD COLUMN ker_s        REAL NOT NULL DEFAULT 0.0;

-- Neurorights-related flags per file.
ALTER TABLE mcp_file ADD COLUMN neuroflag    INTEGER NOT NULL DEFAULT 0 CHECK (neuroflag IN (0, 1));
ALTER TABLE mcp_file ADD COLUMN nonactuatingonly INTEGER NOT NULL DEFAULT 1 CHECK (nonactuatingonly IN (0, 1));
ALTER TABLE mcp_file ADD COLUMN citizen_ready INTEGER NOT NULL DEFAULT 0 CHECK (citizen_ready IN (0, 1));
ALTER TABLE mcp_file ADD COLUMN consent_level TEXT NOT NULL DEFAULT 'NONE'
    CHECK (consent_level IN ('NONE', 'IMPLICIT', 'EXPLICIT'));

-- KER bounds and scalar consistency on files.
DROP TRIGGER IF EXISTS trg_mcp_file_ker_invariants;

CREATE TRIGGER trg_mcp_file_ker_invariants
BEFORE INSERT ON mcp_file
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'mcp_file.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'mcp_file.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'mcp_file.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'mcp_file.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_file_ker_invariants_update;

CREATE TRIGGER trg_mcp_file_ker_invariants_update
BEFORE UPDATE ON mcp_file
BEGIN
    SELECT CASE
        WHEN NEW.ker_k < 0.0 OR NEW.ker_k > 1.0 THEN
            RAISE(ABORT, 'mcp_file.ker_k must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_e < 0.0 OR NEW.ker_e > 1.0 THEN
            RAISE(ABORT, 'mcp_file.ker_e must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_r < 0.0 OR NEW.ker_r > 1.0 THEN
            RAISE(ABORT, 'mcp_file.ker_r must be in [0,1]')
    END;
    SELECT CASE
        WHEN NEW.ker_s != NEW.ker_k * NEW.ker_e - NEW.ker_r THEN
            RAISE(ABORT, 'mcp_file.ker_s must equal ker_k * ker_e - ker_r')
    END;
END;

-- Neurorights corridor: neuro-adjacent files must be non-actuating and require explicit consent.
DROP TRIGGER IF EXISTS trg_mcp_file_neuro_corridor;

CREATE TRIGGER trg_mcp_file_neuro_corridor
BEFORE INSERT ON mcp_file
BEGIN
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.nonactuatingonly = 0 THEN
            RAISE(ABORT, 'Neuro-adjacent files must be nonactuatingonly = 1')
    END;
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.consent_level != 'EXPLICIT' THEN
            RAISE(ABORT, 'Neuro-adjacent files require consent_level = EXPLICIT')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_file_neuro_corridor_update;

CREATE TRIGGER trg_mcp_file_neuro_corridor_update
BEFORE UPDATE ON mcp_file
BEGIN
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.nonactuatingonly = 0 THEN
            RAISE(ABORT, 'Neuro-adjacent files must be nonactuatingonly = 1')
    END;
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND NEW.consent_level != 'EXPLICIT' THEN
            RAISE(ABORT, 'Neuro-adjacent files require consent_level = EXPLICIT')
    END;
END;
