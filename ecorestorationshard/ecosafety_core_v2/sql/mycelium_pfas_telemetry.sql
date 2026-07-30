-- filename: ecorestorationshard/ecosafety_core_v2/sql/mycelium_pfas_telemetry.sql
-- destination: ecorestorationshard/ecosafety_core_v2/sql/mycelium_pfas_telemetry.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS mycelium_pfas_telemetry (
    segment_id    TEXT    NOT NULL,
    yyyymmdd      TEXT    NOT NULL,
    osc_feature   REAL    NOT NULL,
    r_pfas_est    REAL    NOT NULL,
    evidence_hex  TEXT    NOT NULL,
    created_utc   TEXT    NOT NULL,
    PRIMARY KEY (segment_id, yyyymmdd),
    CHECK (r_pfas_est BETWEEN 0.0 AND 1.0)
);
