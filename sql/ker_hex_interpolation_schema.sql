-- Filename: sql/ker_hex_interpolation_schema.sql
-- Destination: Prometheus-Praxis/sql/ker_hex_interpolation_schema.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS hex_ker_sample (
    sample_id      TEXT PRIMARY KEY,
    hex_id         TEXT NOT NULL,
    x_coord_m      REAL NOT NULL,
    y_coord_m      REAL NOT NULL,
    k_factor       REAL NOT NULL CHECK (k_factor >= 0.0 AND k_factor <= 1.0),
    e_factor       REAL NOT NULL CHECK (e_factor >= 0.0 AND e_factor <= 1.0),
    r_factor       REAL NOT NULL CHECK (r_factor >= 0.0 AND r_factor <= 1.0),
    scalar_ker     REAL NOT NULL,
    timestamputc   TEXT NOT NULL,
    evidence_hex   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_hex_ker_sample_hex
    ON hex_ker_sample (hex_id, timestamputc);

CREATE TABLE IF NOT EXISTS hex_ker_interpolated (
    interp_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    target_hex_id  TEXT NOT NULL,
    x_coord_m      REAL NOT NULL,
    y_coord_m      REAL NOT NULL,
    k_factor       REAL NOT NULL CHECK (k_factor >= 0.0 AND k_factor <= 1.0),
    e_factor       REAL NOT NULL CHECK (e_factor >= 0.0 AND e_factor <= 1.0),
    r_factor       REAL NOT NULL CHECK (r_factor >= 0.0 AND r_factor <= 1.0),
    scalar_ker     REAL NOT NULL,
    neighbour_count INTEGER NOT NULL,
    timestamputc   TEXT NOT NULL,
    evidence_hex   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_hex_ker_interpolated_hex
    ON hex_ker_interpolated (target_hex_id, timestamputc);

DROP TRIGGER IF EXISTS trg_hex_ker_interpolated_ker_consistency;

CREATE TRIGGER trg_hex_ker_interpolated_ker_consistency
BEFORE INSERT ON hex_ker_interpolated
BEGIN
    SELECT CASE
        WHEN NEW.scalar_ker <= 0.0 THEN
            RAISE(ABORT, 'scalar_ker must be positive')
    END;

    SELECT CASE
        WHEN ABS(NEW.k_factor * NEW.e_factor - NEW.r_factor - NEW.scalar_ker) > 0.000001 THEN
            RAISE(ABORT, 'scalar_ker inconsistent with KER triad')
    END;
END;
