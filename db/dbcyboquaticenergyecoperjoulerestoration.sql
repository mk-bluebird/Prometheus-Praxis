-- filename: db/dbcyboquaticenergyecoperjoulerestoration.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_energy_ecoperjoule_restoration (
    frameid             TEXT PRIMARY KEY,
    nodeid              TEXT NOT NULL,
    date_utc            TEXT NOT NULL,
    window_start_utc    TEXT NOT NULL,
    window_end_utc      TEXT NOT NULL,
    energy_req_j        REAL NOT NULL CHECK (energy_req_j >= 0.0),
    ecoperjoule         REAL NOT NULL CHECK (ecoperjoule >= 0.0 AND ecoperjoule <= 1000.0),
    restoration_flag    INTEGER NOT NULL CHECK (restoration_flag IN (0, 1)),
    carbon_negative_ok  INTEGER NOT NULL CHECK (carbon_negative_ok IN (0, 1)),
    k_knowledge         REAL NOT NULL CHECK (k_knowledge >= 0.0 AND k_knowledge <= 1.0),
    e_ecoimpact         REAL NOT NULL CHECK (e_ecoimpact >= 0.0 AND e_ecoimpact <= 1.0),
    r_risk              REAL NOT NULL CHECK (r_risk >= 0.0 AND r_risk <= 1.0),
    ker_score           REAL NOT NULL,
    evidence_hex        TEXT NOT NULL,
    signing_did         TEXT NOT NULL,
    created_at_utc      TEXT NOT NULL,
    last_updated_utc    TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_energy_ecoper_node_date
    ON cyboquatic_energy_ecoperjoule_restoration (nodeid, date_utc);

CREATE INDEX IF NOT EXISTS idx_energy_ecoper_ker_carbon
    ON cyboquatic_energy_ecoperjoule_restoration (k_knowledge, e_ecoimpact, r_risk, ker_score, carbon_negative_ok);

DROP TRIGGER IF EXISTS trg_energy_ecoper_ker_invariant;

CREATE TRIGGER trg_energy_ecoper_ker_invariant
BEFORE INSERT ON cyboquatic_energy_ecoperjoule_restoration
BEGIN
    SELECT
        CASE
            WHEN ABS(NEW.k_knowledge * NEW.e_ecoimpact - NEW.r_risk - NEW.ker_score) > 0.000001
            THEN RAISE(ABORT, 'ker_score inconsistent with KER triad for energy/ecoperJoule restoration')
        END;

    SELECT
        CASE
            WHEN NEW.carbon_negative_ok = 0
            THEN RAISE(ABORT, 'carbon_negative_ok must be true for cyboquatic energy restoration frames')
        END;
END;
