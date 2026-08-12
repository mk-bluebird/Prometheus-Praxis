-- File: sql/eco_restoration/carbon_offset_allocation.sql

CREATE TABLE IF NOT EXISTS carbon_offset_certificate (
    certificate_id TEXT PRIMARY KEY,
    verified_g REAL NOT NULL CHECK(verified_g > 0),
    expires_unix_s INTEGER NOT NULL,
    state TEXT NOT NULL CHECK(state IN ('ACTIVE','EXPIRED','EXHAUSTED'))
) STRICT;

CREATE TABLE IF NOT EXISTS workload_emission (
    workload_id TEXT PRIMARY KEY,
    completed_unix_s INTEGER NOT NULL,
    carbon_g REAL NOT NULL CHECK(carbon_g >= 0)
) STRICT;

CREATE TABLE IF NOT EXISTS carbon_offset_allocation (
    allocation_id INTEGER PRIMARY KEY,
    certificate_id TEXT NOT NULL UNIQUE REFERENCES carbon_offset_certificate(certificate_id),
    workload_id TEXT NOT NULL UNIQUE REFERENCES workload_emission(workload_id),
    allocated_g REAL NOT NULL CHECK(allocated_g > 0),
    allocated_unix_s INTEGER NOT NULL
) STRICT;

CREATE TRIGGER IF NOT EXISTS carbon_offset_allocation_guard
BEFORE INSERT ON carbon_offset_allocation
FOR EACH ROW
BEGIN
    SELECT CASE
        WHEN COALESCE((
            SELECT state = 'ACTIVE' AND expires_unix_s >= NEW.allocated_unix_s
            FROM carbon_offset_certificate
            WHERE certificate_id = NEW.certificate_id
        ), 0) <> 1
        THEN RAISE(ABORT, 'offset certificate is inactive or expired')
    END;

    SELECT CASE
        WHEN NEW.allocated_g > (
            SELECT verified_g
            FROM carbon_offset_certificate
            WHERE certificate_id = NEW.certificate_id
        )
        THEN RAISE(ABORT, 'allocation exceeds verified certificate value')
    END;
END;

CREATE TRIGGER IF NOT EXISTS carbon_offset_exhaustion_state
AFTER INSERT ON carbon_offset_allocation
FOR EACH ROW
BEGIN
    UPDATE carbon_offset_certificate
    SET state = CASE
        WHEN verified_g <= NEW.allocated_g THEN 'EXHAUSTED'
        ELSE state
    END
    WHERE certificate_id = NEW.certificate_id;
END;

CREATE VIEW IF NOT EXISTS monthly_carbon_offset_audit AS
WITH months AS (
    SELECT strftime('%Y-%m', completed_unix_s, 'unixepoch') AS month
    FROM workload_emission
    UNION
    SELECT strftime('%Y-%m', allocated_unix_s, 'unixepoch')
    FROM carbon_offset_allocation
)
SELECT
    months.month,
    COALESCE((
        SELECT SUM(carbon_g)
        FROM workload_emission
        WHERE strftime('%Y-%m', completed_unix_s, 'unixepoch') = months.month
    ), 0.0) AS emitted_g,
    COALESCE((
        SELECT SUM(allocated_g)
        FROM carbon_offset_allocation
        WHERE strftime('%Y-%m', allocated_unix_s, 'unixepoch') = months.month
    ), 0.0) AS allocated_g,
    COALESCE((
        SELECT SUM(allocated_g)
        FROM carbon_offset_allocation
        WHERE strftime('%Y-%m', allocated_unix_s, 'unixepoch') = months.month
    ), 0.0) - COALESCE((
        SELECT SUM(carbon_g)
        FROM workload_emission
        WHERE strftime('%Y-%m', completed_unix_s, 'unixepoch') = months.month
    ), 0.0) AS net_offset_g
FROM months;
