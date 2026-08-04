-- File: sql/cyboquatic_ker_triggers.sql
PRAGMA foreign_keys = ON;

-- Governance particle table.
CREATE TABLE IF NOT EXISTS governance_particle (
    did TEXT PRIMARY KEY,
    ker_k REAL NOT NULL,
    ker_e REAL NOT NULL,
    ker_r REAL NOT NULL,
    CHECK (ker_k >= 0.0 AND ker_k <= 1.0),
    CHECK (ker_r >= 0.0 AND ker_r <= 1.0),
    CHECK (ker_e <= 0.0)
);

INSERT OR IGNORE INTO governance_particle (did, ker_k, ker_e, ker_r)
VALUES ('bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', 0.9, -1.0, 0.2);

-- Telemetry table with KER fields and DID reference.
CREATE TABLE IF NOT EXISTS cyboquatic_workload_telemetry (
    telemetry_id INTEGER PRIMARY KEY AUTOINCREMENT,
    did TEXT NOT NULL,
    basin_id TEXT NOT NULL,
    timestamp_s REAL NOT NULL,
    flow_rate_m3_s REAL NOT NULL,
    head_m REAL NOT NULL,
    motor_efficiency REAL NOT NULL,
    aeration_factor REAL NOT NULL,
    energyreq_j REAL NOT NULL,
    delta_vt_m_s REAL NOT NULL,
    ker_k REAL NOT NULL,
    ker_e REAL NOT NULL,
    ker_r REAL NOT NULL,
    FOREIGN KEY(did) REFERENCES governance_particle(did),
    CHECK (ker_k >= 0.0 AND ker_k <= 1.0),
    CHECK (ker_r >= 0.0 AND ker_r <= 1.0)
);

CREATE INDEX IF NOT EXISTS idx_cybo_ker_did_time
    ON cyboquatic_workload_telemetry(did, timestamp_s);

-- Eco-audit log table.
CREATE TABLE IF NOT EXISTS eco_audit_log (
    audit_id INTEGER PRIMARY KEY AUTOINCREMENT,
    did TEXT NOT NULL,
    telemetry_id INTEGER NOT NULL,
    timestamp_s REAL NOT NULL,
    basin_id TEXT NOT NULL,
    ker_e REAL NOT NULL,
    message TEXT NOT NULL,
    FOREIGN KEY(did) REFERENCES governance_particle(did),
    FOREIGN KEY(telemetry_id) REFERENCES cyboquatic_workload_telemetry(telemetry_id)
);

-- Trigger that enforces ker_e <= 0 and logs violations.
CREATE TRIGGER IF NOT EXISTS trg_cybo_ker_e_check
AFTER INSERT ON cyboquatic_workload_telemetry
FOR EACH ROW
BEGIN
    -- If ker_e > 0, log violation and raise an error to trigger rollback.
    WHEN NEW.ker_e > 0.0 BEGIN
        INSERT INTO eco_audit_log (did, telemetry_id, timestamp_s, basin_id, ker_e, message)
        VALUES (NEW.did, NEW.telemetry_id, NEW.timestamp_s, NEW.basin_id, NEW.ker_e,
                'KER eco-impact violation: ker_e > 0');
        SELECT RAISE(ABORT, 'ker_e must be <= 0 for carbon-negative compliance');
    END;
END;
