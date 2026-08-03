-- File: sql/mcp_file_ker_neurorights.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/mcp_file_ker_neurorights.sql

PRAGMA foreign_keys = ON;

-- Per-file KER triad and scalar.
ALTER TABLE mcp_file ADD COLUMN ker_k        REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_file ADD COLUMN ker_e        REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_file ADD COLUMN ker_r        REAL NOT NULL DEFAULT 0.0;
ALTER TABLE mcp_file ADD COLUMN ker_s        REAL NOT NULL DEFAULT 0.0;

-- Neurorights-related flags per file.
ALTER TABLE mcp_file ADD COLUMN neuroflag         INTEGER NOT NULL DEFAULT 0 CHECK (neuroflag IN (0, 1));
ALTER TABLE mcp_file ADD COLUMN nonactuatingonly  INTEGER NOT NULL DEFAULT 1 CHECK (nonactuatingonly IN (0, 1));
ALTER TABLE mcp_file ADD COLUMN citizen_ready     INTEGER NOT NULL DEFAULT 0 CHECK (citizen_ready IN (0, 1));
ALTER TABLE mcp_file ADD COLUMN consent_level     TEXT    NOT NULL DEFAULT 'NONE'
    CHECK (consent_level IN ('NONE', 'IMPLICIT', 'EXPLICIT'));

-- Logging table for neurorights attempts without proper consent.
CREATE TABLE IF NOT EXISTS neuroflag_attempt_log (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    object_kind         TEXT    NOT NULL, -- 'FILE' or 'TOOL'
    relpath             TEXT    NOT NULL,
    attempted_neuroflag INTEGER NOT NULL,
    consent_level       TEXT,
    timestamp_utc       TEXT    NOT NULL,
    reason              TEXT    NOT NULL
);

-- Neurorights health overview view.
DROP VIEW IF EXISTS v_neurorights_health;

CREATE VIEW v_neurorights_health AS
SELECT
    date(timestamp_utc) AS day,
    object_kind,
    COUNT(*)            AS attempts_without_consent,
    SUM(CASE WHEN reason LIKE '%EXPLICIT%' THEN 1 ELSE 0 END) AS explicit_required_violations
FROM neuroflag_attempt_log
GROUP BY day, object_kind;

-- Neurorights corridor: enforce consent on mcp_file neuroflag changes.
DROP TRIGGER IF EXISTS mcp_file_neuroflag_enforce;

CREATE TRIGGER mcp_file_neuroflag_enforce
BEFORE UPDATE ON mcp_file
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND (NEW.consent_level IS NULL OR NEW.consent_level NOT IN ('EXPLICIT'))
        THEN
            INSERT INTO neuroflag_attempt_log(
                object_kind, relpath, attempted_neuroflag,
                consent_level, timestamp_utc, reason
            )
            VALUES(
                'FILE', NEW.relpath, NEW.neuroflag,
                NEW.consent_level, datetime('now'),
                'Neuroflag=1 attempted without EXPLICIT consent'
            );
            RAISE(ABORT, 'Neuro-adjacent files require EXPLICIT consent before neuroflag=1')
    END;
END;

-- Neurorights corridor: neuro-adjacent files must be non-actuating and require explicit consent.
DROP TRIGGER IF EXISTS trg_mcp_file_neuro_corridor;

CREATE TRIGGER trg_mcp_file_neuro_corridor
BEFORE INSERT ON mcp_file
FOR EACH ROW
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
FOR EACH ROW
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

-- KER bounds and scalar consistency on files.
DROP TRIGGER IF EXISTS trg_mcp_file_ker_invariants;

CREATE TRIGGER trg_mcp_file_ker_invariants
BEFORE INSERT ON mcp_file
FOR EACH ROW
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
FOR EACH ROW
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

-- Neurorights corridor: enforce consent on mcp_tool neuroflag changes and log attempts.
DROP TRIGGER IF EXISTS mcp_tool_neuroflag_enforce;

CREATE TRIGGER mcp_tool_neuroflag_enforce
BEFORE UPDATE ON mcp_tool
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN NEW.neuroflag = 1 AND (NEW.consent_level IS NULL OR NEW.consent_level NOT IN ('EXPLICIT'))
        THEN
            INSERT INTO neuroflag_attempt_log(
                object_kind, relpath, attempted_neuroflag,
                consent_level, timestamp_utc, reason
            )
            VALUES(
                'TOOL', NEW.relpath, NEW.neuroflag,
                NEW.consent_level, datetime('now'),
                'Neuroflag=1 attempted without EXPLICIT consent'
            );
            RAISE(ABORT, 'Neuro-adjacent tools require EXPLICIT consent before neuroflag=1')
    END;
END;
