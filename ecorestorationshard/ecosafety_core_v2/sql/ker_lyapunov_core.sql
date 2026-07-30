-- filename: ecorestorationshard/ecosafety_core_v2/sql/ker_lyapunov_core.sql
-- destination: ecorestorationshard/ecosafety_core_v2/sql/ker_lyapunov_core.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Canonical, non-actuating SQLite schema for:
--     - Risk planes and weights (Lyapunov KER core).[4]
--     - Residual windows V_t across domains.[4][6]
--     - K,E,R triads per lane and segment.[4]
--     - Global triggers enforcing corridor invariants and
--       "always improve" residual monotonicity on daily shards.[4][15]
--
--   All cyboquaticprogress shards (domains a–g) must reference this
--   core instead of redefining their own residual/KER logic.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Risk planes and Lyapunov weights
----------------------------------------------------------------------

-- Enumerated planes (energy, hydraulics, PFAS, cold-survival, BOD, TSS, CEC, carbon, biodiversity, materials, neurorights, topology, data, uncertainty).[4][6]
CREATE TABLE IF NOT EXISTS ker_risk_plane (
    plane_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    plane_code    TEXT    NOT NULL UNIQUE, -- e.g. 'ENERGY', 'PFAS', 'COLD', 'BOD', 'TSS', 'CEC'
    description   TEXT    NOT NULL
);

-- Lyapunov weights per domain/lane; w_j for each plane.[4]
CREATE TABLE IF NOT EXISTS ker_plane_weights (
    weights_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    domain        TEXT    NOT NULL, -- 'CYBOQUATIC', 'HYDRO', 'MATERIAL', 'GOV'
    lane          TEXT    NOT NULL, -- 'RESEARCH', 'PILOT', 'PROD'
    plane_code    TEXT    NOT NULL,
    weight_value  REAL    NOT NULL,
    CHECK (weight_value >= 0.0),
    FOREIGN KEY (plane_code) REFERENCES ker_risk_plane (plane_code)
        ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_ker_plane_weights_domain_lane
    ON ker_plane_weights (domain, lane, plane_code);

----------------------------------------------------------------------
-- 2. Residual windows and KER triads
----------------------------------------------------------------------

-- Canonical residual window per segment/node across any domain, with normalized
-- risk coordinates and V_t.[4][6]
CREATE TABLE IF NOT EXISTS ker_residual_window (
    window_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    yyyymmdd         TEXT    NOT NULL,
    segment_id       TEXT    NOT NULL,
    domain           TEXT    NOT NULL,
    lane             TEXT    NOT NULL, -- 'RESEARCH','PILOT','PROD'
    -- Normalized risk coordinates in [0,1] per plane.[4][6]
    r_energy         REAL    NOT NULL,
    r_hydraulics     REAL    NOT NULL,
    r_pfas           REAL    NOT NULL,
    r_cold           REAL    NOT NULL,
    r_bod            REAL    NOT NULL,
    r_tss            REAL    NOT NULL,
    r_cec            REAL    NOT NULL,
    r_carbon         REAL    NOT NULL,
    r_biodiversity   REAL    NOT NULL,
    r_materials      REAL    NOT NULL,
    r_neurorights    REAL    NOT NULL,
    r_topology       REAL    NOT NULL,
    r_dataquality    REAL    NOT NULL,
    r_uncertainty    REAL    NOT NULL,
    -- Lyapunov residual V_t (scalar).[4]
    vt_residual      REAL    NOT NULL,
    -- K,E,R triad.[4]
    k_knowledge      REAL    NOT NULL,
    e_ecoimpact      REAL    NOT NULL,
    r_risk           REAL    NOT NULL,
    -- Hex + DID bindings.[15]
    evidence_hex     TEXT    NOT NULL,
    prior_hex        TEXT,
    signing_did      TEXT    NOT NULL,
    created_utc      TEXT    NOT NULL,
    CHECK (r_energy       BETWEEN 0.0 AND 1.0),
    CHECK (r_hydraulics   BETWEEN 0.0 AND 1.0),
    CHECK (r_pfas         BETWEEN 0.0 AND 1.0),
    CHECK (r_cold         BETWEEN 0.0 AND 1.0),
    CHECK (r_bod          BETWEEN 0.0 AND 1.0),
    CHECK (r_tss          BETWEEN 0.0 AND 1.0),
    CHECK (r_cec          BETWEEN 0.0 AND 1.0),
    CHECK (r_carbon       BETWEEN 0.0 AND 1.0),
    CHECK (r_biodiversity BETWEEN 0.0 AND 1.0),
    CHECK (r_materials    BETWEEN 0.0 AND 1.0),
    CHECK (r_neurorights  BETWEEN 0.0 AND 1.0),
    CHECK (r_topology     BETWEEN 0.0 AND 1.0),
    CHECK (r_dataquality  BETWEEN 0.0 AND 1.0),
    CHECK (r_uncertainty  BETWEEN 0.0 AND 1.0),
    CHECK (k_knowledge    BETWEEN 0.0 AND 1.0),
    CHECK (e_ecoimpact    BETWEEN 0.0 AND 1.0),
    CHECK (r_risk         BETWEEN 0.0 AND 1.0)
);

CREATE INDEX IF NOT EXISTS idx_ker_residual_window_seg_date
    ON ker_residual_window (segment_id, yyyymmdd, lane);

----------------------------------------------------------------------
-- 3. Non-offsettable planes and global invariants
----------------------------------------------------------------------

-- Non-offsettable planes: violations cannot be traded off.[4]
CREATE TABLE IF NOT EXISTS ker_non_offsettable_plane (
    plane_code    TEXT    PRIMARY KEY,
    description   TEXT    NOT NULL
);

INSERT OR IGNORE INTO ker_non_offsettable_plane (plane_code, description) VALUES
    ('CARBON',      'Non-offsettable carbon corridor'),
    ('BIODIVERSITY','Non-offsettable biodiversity corridor'),
    ('NEURORIGHTS', 'Non-offsettable neurorights corridor');

-- View to inspect whether any non-offsettable plane breaches safe band.
CREATE VIEW IF NOT EXISTS v_ker_non_offsettable_breach AS
SELECT
    window_id,
    yyyymmdd,
    segment_id,
    domain,
    lane,
    r_carbon,
    r_biodiversity,
    r_neurorights
FROM ker_residual_window
WHERE r_carbon      > 0.5
   OR r_biodiversity > 0.5
   OR r_neurorights > 0.0;

----------------------------------------------------------------------
-- 4. Global triggers: residual monotonicity and KER corridor
----------------------------------------------------------------------

-- Enforce:
--   - Non-offsettable planes must never breach threshold in PROD lane.[4]
--   - Residual V_t must not increase compared to prior hex in same lane.[4]
--   - K,E,R corridor thresholds per lane.[4]

CREATE TRIGGER IF NOT EXISTS trg_ker_residual_window_insert
BEFORE INSERT ON ker_residual_window
FOR EACH ROW
BEGIN
    -- Domain check: must use canonical domains.
    SELECT
        CASE
            WHEN NEW.domain NOT IN ('CYBOQUATIC','HYDRO','MATERIAL','GOV')
            THEN RAISE(ABORT, 'ker_residual_window: invalid domain')
        END;

    -- Non-offsettable plane guard in PROD lane.[4]
    SELECT
        CASE
            WHEN NEW.lane = 'PROD'
                 AND (NEW.r_carbon      > 0.5
                  OR  NEW.r_biodiversity > 0.5
                  OR  NEW.r_neurorights  > 0.0)
            THEN RAISE(ABORT, 'ker_residual_window: non-offsettable plane breach in PROD lane')
        END;

    -- Residual monotonicity vs prior_hex for same segment/lane.[4]
    SELECT
        CASE
            WHEN NEW.prior_hex IS NOT NULL
             AND EXISTS (
                SELECT 1
                FROM ker_residual_window AS prev
                WHERE prev.evidence_hex = NEW.prior_hex
                  AND prev.segment_id  = NEW.segment_id
                  AND prev.lane        = NEW.lane
                  AND NEW.vt_residual  > prev.vt_residual + 1e-6
             )
            THEN RAISE(ABORT, 'ker_residual_window: V_t increased vs prior_hex')
        END;

    -- K,E,R corridor thresholds per lane (gold bands).[4]
    SELECT
        CASE
            WHEN NEW.lane = 'RESEARCH'
             AND (NEW.k_knowledge < 0.70
              OR  NEW.e_ecoimpact < 0.70
              OR  NEW.r_risk      > 0.30)
            THEN RAISE(ABORT, 'ker_residual_window: KER outside RESEARCH corridor')
        END;

    SELECT
        CASE
            WHEN NEW.lane = 'PILOT'
             AND (NEW.k_knowledge < 0.80
              OR  NEW.e_ecoimpact < 0.80
              OR  NEW.r_risk      > 0.20)
            THEN RAISE(ABORT, 'ker_residual_window: KER outside PILOT corridor')
        END;

    SELECT
        CASE
            WHEN NEW.lane = 'PROD'
             AND (NEW.k_knowledge < 0.90
              OR  NEW.e_ecoimpact < 0.90
              OR  NEW.r_risk      > 0.13)
            THEN RAISE(ABORT, 'ker_residual_window: KER outside PROD corridor')
        END;
END;
