-- Filename: sql/gov_blastradius_diag_schema.sql

CREATE TABLE IF NOT EXISTS gov_blastradius_diag (
    diag_id       TEXT PRIMARY KEY,
    canal_node_id TEXT NOT NULL,
    k             REAL NOT NULL,
    e             REAL NOT NULL,
    r             REAL NOT NULL,
    s             REAL NOT NULL,
    vt            REAL NOT NULL,
    did           TEXT NOT NULL,
    evidence_hex  TEXT NOT NULL,
    timestamp_utc TEXT NOT NULL,
    CHECK (k BETWEEN 0.0 AND 1.0),
    CHECK (e BETWEEN 0.0 AND 1.0),
    CHECK (r BETWEEN 0.0 AND 1.0),
    CHECK (s BETWEEN 0.0 AND 1.0),
    CHECK (vt >= 0.0),
    CHECK (did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7')
);

CREATE TRIGGER IF NOT EXISTS trg_gov_blastradius_diag_s_consistency
BEFORE INSERT ON gov_blastradius_diag
FOR EACH ROW
BEGIN
    -- Enforce s ≈ k * e - r within tolerance.
    SELECT
        CASE
            WHEN abs(NEW.k * NEW.e - NEW.r - NEW.s) > 0.000001 THEN
                RAISE(ABORT, 'kerScore inconsistent with k*e-r')
        END;
END;
