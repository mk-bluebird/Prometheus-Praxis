-- File: sql/mcp_endpoint_transport_governance.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/mcp_endpoint_transport_governance.sql

PRAGMA foreign_keys = ON;

ALTER TABLE mcp_endpoint ADD COLUMN transport_kind TEXT NOT NULL DEFAULT 'CLI'
    CHECK (transport_kind IN ('CLI', 'SQL', 'RUST_FN', 'LUA', 'KOTLIN', 'JNI'));

ALTER TABLE mcp_endpoint ADD COLUMN nonactuatingonly INTEGER NOT NULL DEFAULT 1
    CHECK (nonactuatingonly IN (0, 1));

ALTER TABLE mcp_endpoint ADD COLUMN allows_actuation INTEGER NOT NULL DEFAULT 0
    CHECK (allows_actuation IN (0, 1));

-- CLI endpoints must be non-actuating.
DROP TRIGGER IF EXISTS trg_mcp_endpoint_cli_nonactuating;

CREATE TRIGGER trg_mcp_endpoint_cli_nonactuating
BEFORE INSERT ON mcp_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.transport_kind = 'CLI' AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'CLI endpoints must not allow actuation')
    END;
    SELECT CASE
        WHEN NEW.transport_kind = 'CLI' AND NEW.nonactuatingonly = 0 THEN
            RAISE(ABORT, 'CLI endpoints must be nonactuatingonly = 1')
    END;
END;

DROP TRIGGER IF EXISTS trg_mcp_endpoint_cli_nonactuating_update;

CREATE TRIGGER trg_mcp_endpoint_cli_nonactuating_update
BEFORE UPDATE ON mcp_endpoint
BEGIN
    SELECT CASE
        WHEN NEW.transport_kind = 'CLI' AND NEW.allows_actuation = 1 THEN
            RAISE(ABORT, 'CLI endpoints must not allow actuation')
    END;
    SELECT CASE
        WHEN NEW.transport_kind = 'CLI' AND NEW.nonactuatingonly = 0 THEN
            RAISE(ABORT, 'CLI endpoints must be nonactuatingonly = 1')
    END;
END;
