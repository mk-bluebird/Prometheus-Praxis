-- filename: ecorestorationshard/cyboquaticprogress/20260729/sql/cyboquatic_pfas_cold_corridor_20260729.sql
-- destination: ecorestorationshard/cyboquaticprogress/20260729/sql/cyboquatic_pfas_cold_corridor_20260729.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Non-actuating SQLite shard for PFAS fate + cold-survival Lyapunov corridor
--   in Phoenix canals, aligned with qpudatashard-style residual modeling and
--   the Phoenix Hex Anchors registry.[file:15][file:4]
--
--   This shard:
--     - Defines state tables for PFAS mass in water/sediments and cold-survival factor.[file:6]
--     - Provides a WITH RECURSIVE corridor evaluator that computes V_t across steps.[file:6]
--     - Binds rows to a Phoenix hex anchor and Bostrom DID for governance.[file:4][file:15]
--
--   It is strictly diagnostic and append-only at the logical level.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. PFAS Lyapunov corridor state tables
----------------------------------------------------------------------

-- Canonical state per canal segment and timestep.
-- r_pfas and r_cold are normalized risk coordinates in [0,1],
-- compatible with the existing KER/Lyapunov grammar.[file:4][file:6]
CREATE TABLE IF NOT EXISTS pfas_corridor_state (
    segment_id        TEXT    NOT NULL,
    step_k            INTEGER NOT NULL,
    -- PFAS mass (mobile phase) in mg
    m_mobile_mg       REAL    NOT NULL,
    -- PFAS mass (sorbed phase) in mg
    m_sorbed_mg       REAL    NOT NULL,
    -- Cold-survival factor x3 in [0,1]; higher means slower degradation.[file:6]
    cold_survival     REAL    NOT NULL,
    -- Cumulative degraded fraction x4 in [0,1].[file:6]
    degraded_fraction REAL    NOT NULL,
    -- Normalized risk coordinates in [0,1].[file:4][file:6]
    r_pfas            REAL    NOT NULL,
    r_cold            REAL    NOT NULL,
    -- Lyapunov residual V_t for this step (PFAS + cold plane).[file:4][file:6]
    vt_residual       REAL    NOT NULL,
    -- Evidence binding: Phoenix hex anchor + Bostrom DID.[file:15][file:4]
    evidence_hex      TEXT    NOT NULL,
    signing_did       TEXT    NOT NULL,
    created_utc       TEXT    NOT NULL,
    PRIMARY KEY (segment_id, step_k),
    CHECK (cold_survival >= 0.0 AND cold_survival <= 1.0),
    CHECK (degraded_fraction >= 0.0 AND degraded_fraction <= 1.0),
    CHECK (r_pfas >= 0.0 AND r_pfas <= 1.0),
    CHECK (r_cold >= 0.0 AND r_cold <= 1.0)
);

-- Per-segment coefficients for discrete-time PFAS update, derived from physics
-- but stored as normalized parameters to keep this shard non-actuating.[file:6]
CREATE TABLE IF NOT EXISTS pfas_corridor_coeffs (
    segment_id          TEXT    PRIMARY KEY,
    -- Base degradation rate in mobile phase (warm conditions).
    k_deg_mobile_base   REAL    NOT NULL,
    -- Base degradation rate in sediments.
    k_deg_sed_base      REAL    NOT NULL,
    -- Sorption coefficient from mobile to sorbed phase.
    k_sorb              REAL    NOT NULL,
    -- Desorption coefficient from sorbed to mobile phase.
    k_desorb            REAL    NOT NULL,
    -- Cold-survival update coefficient α in x3_{k+1} = x3_k + α*(1 - r_Tk) - r_Tk.[file:6]
    k_cold_alpha        REAL    NOT NULL,
    -- Weight factors in Lyapunov residual V_t = w_pfas * r_pfas^2 + w_cold * r_cold^2.[file:4][file:6]
    w_pfas              REAL    NOT NULL,
    w_cold              REAL    NOT NULL,
    -- Phoenix canal region, for join with other cyboquatic tables.[file:15]
    region_code         TEXT    NOT NULL,
    -- Governance bindings.
    evidence_hex        TEXT    NOT NULL,
    signing_did         TEXT    NOT NULL,
    created_utc         TEXT    NOT NULL
);

-- Initial conditions for recursive corridors: one row per segment.[file:6]
CREATE TABLE IF NOT EXISTS pfas_corridor_initial (
    segment_id        TEXT    PRIMARY KEY,
    m_mobile_mg       REAL    NOT NULL,
    m_sorbed_mg       REAL    NOT NULL,
    cold_survival     REAL    NOT NULL,
    degraded_fraction REAL    NOT NULL,
    -- Initial normalized risk coordinates.[file:6]
    r_pfas_init       REAL    NOT NULL,
    r_cold_init       REAL    NOT NULL,
    vt_residual_init  REAL    NOT NULL,
    evidence_hex      TEXT    NOT NULL,
    signing_did       TEXT    NOT NULL,
    created_utc       TEXT    NOT NULL
);

----------------------------------------------------------------------
-- 2. PFAS Lyapunov corridor recursive evaluation (non-actuating)
----------------------------------------------------------------------

-- This WITH RECURSIVE expression advances PFAS and cold-survival state across
-- N discrete steps, computing V_t at each step, as described in the PFAS corridor
-- design for domain (b).[file:6][file:4]
--
-- Usage pattern:
--   - Bind :segment_id and :max_steps at query time.
--   - Bind :temp_risk_k to a per-step temperature risk via a temp table or join.
--   - Insert results into pfas_corridor_state via INSERT ... SELECT.
--
-- Notes:
--   - This is a logical corridor evaluator only; no physical actuation.[file:4]

-- Example corridor evaluator as a view over a parameter table.
CREATE TABLE IF NOT EXISTS pfas_temp_risk_window (
    segment_id  TEXT    NOT NULL,
    step_k      INTEGER NOT NULL,
    -- r_Tk ∈ [0,1] temperature risk coordinate.[file:6]
    r_temp      REAL    NOT NULL,
    PRIMARY KEY (segment_id, step_k),
    CHECK (r_temp >= 0.0 AND r_temp <= 1.0)
);

CREATE VIEW IF NOT EXISTS v_pfas_corridor_recursive AS
WITH RECURSIVE corridor AS (
    -- Base step from initial conditions.[file:6]
    SELECT
        ic.segment_id              AS segment_id,
        0                          AS step_k,
        ic.m_mobile_mg             AS m_mobile_mg,
        ic.m_sorbed_mg             AS m_sorbed_mg,
        ic.cold_survival           AS cold_survival,
        ic.degraded_fraction       AS degraded_fraction,
        ic.r_pfas_init             AS r_pfas,
        ic.r_cold_init             AS r_cold,
        ic.vt_residual_init        AS vt_residual,
        ic.evidence_hex            AS evidence_hex,
        ic.signing_did             AS signing_did,
        ic.created_utc             AS created_utc
    FROM pfas_corridor_initial AS ic

    UNION ALL

    -- Recursive step using affine update and Lyapunov residual.[file:6]
    SELECT
        c.segment_id                                           AS segment_id,
        c.step_k + 1                                           AS step_k,
        -- Mobile PFAS update:
        -- m_mobile_{k+1} = m_mobile_k
        --                  - d_eff_k * m_mobile_k
        --                  + k_desorb * m_sorbed_k
        --                  - k_sorb * m_mobile_k.[file:6]
        (c.m_mobile_mg
            - ((coeff.k_deg_mobile_base * (1.0 - c.cold_survival)) * c.m_mobile_mg)
            + (coeff.k_desorb * c.m_sorbed_mg)
            - (coeff.k_sorb   * c.m_mobile_mg)
        )                                                       AS m_mobile_mg,
        -- Sorbed PFAS update:
        -- m_sorbed_{k+1} = m_sorbed_k
        --                  + k_sorb * m_mobile_k
        --                  - k_desorb * m_sorbed_k
        --                  - d_sed_k * m_sorbed_k.[file:6]
        (c.m_sorbed_mg
            + (coeff.k_sorb   * c.m_mobile_mg)
            - (coeff.k_desorb * c.m_sorbed_mg)
            - ((coeff.k_deg_sed_base * (1.0 - c.cold_survival)) * c.m_sorbed_mg)
        )                                                       AS m_sorbed_mg,
        -- Cold-survival update (clipped to [0,1]):[file:6]
        CASE
            WHEN c.cold_survival
                 + (coeff.k_cold_alpha * (1.0 - tr.r_temp))
                 - tr.r_temp > 1.0 THEN 1.0
            WHEN c.cold_survival
                 + (coeff.k_cold_alpha * (1.0 - tr.r_temp))
                 - tr.r_temp < 0.0 THEN 0.0
            ELSE c.cold_survival
                 + (coeff.k_cold_alpha * (1.0 - tr.r_temp))
                 - tr.r_temp
        END                                                      AS cold_survival,
        -- Degraded fraction update:[file:6]
        (c.degraded_fraction
            + (coeff.k_deg_mobile_base * (1.0 - c.cold_survival) * c.m_mobile_mg)
            + (coeff.k_deg_sed_base   * (1.0 - c.cold_survival) * c.m_sorbed_mg)
        )                                                       AS degraded_fraction,
        -- Normalized PFAS risk (example: linear mapping from mass to risk with
        -- implicit corridor normalization).[file:6][file:4]
        CASE
            WHEN (c.m_mobile_mg + c.m_sorbed_mg) <= 0.0 THEN 0.0
            ELSE MIN(1.0, (c.m_mobile_mg + c.m_sorbed_mg) / 100.0)
        END                                                      AS r_pfas,
        -- Normalized cold risk = cold_survival (already ∈ [0,1]).[file:6]
        CASE
            WHEN c.cold_survival < 0.0 THEN 0.0
            WHEN c.cold_survival > 1.0 THEN 1.0
            ELSE c.cold_survival
        END                                                      AS r_cold,
        -- Lyapunov residual V_t = w_pfas * r_pfas^2 + w_cold * r_cold^2.[file:4][file:6]
        (coeff.w_pfas * (
            CASE
                WHEN (c.m_mobile_mg + c.m_sorbed_mg) <= 0.0 THEN 0.0
                ELSE MIN(1.0, (c.m_mobile_mg + c.m_sorbed_mg) / 100.0)
            END
        ) * (
            CASE
                WHEN (c.m_mobile_mg + c.m_sorbed_mg) <= 0.0 THEN 0.0
                ELSE MIN(1.0, (c.m_mobile_mg + c.m_sorbed_mg) / 100.0)
            END
        )
        + coeff.w_cold * (
            CASE
                WHEN c.cold_survival < 0.0 THEN 0.0
                WHEN c.cold_survival > 1.0 THEN 1.0
                ELSE c.cold_survival
            END
        ) * (
            CASE
                WHEN c.cold_survival < 0.0 THEN 0.0
                WHEN c.cold_survival > 1.0 THEN 1.0
                ELSE c.cold_survival
            END
        )
        )                                                       AS vt_residual,
        c.evidence_hex                                         AS evidence_hex,
        c.signing_did                                          AS signing_did,
        c.created_utc                                          AS created_utc
    FROM corridor AS c
    JOIN pfas_corridor_coeffs AS coeff
      ON coeff.segment_id = c.segment_id
    JOIN pfas_temp_risk_window AS tr
      ON tr.segment_id = c.segment_id
     AND tr.step_k    = c.step_k + 1
    WHERE c.step_k < (
        SELECT MAX(step_k) FROM pfas_temp_risk_window
        WHERE segment_id = c.segment_id
    )
)
SELECT *
FROM corridor;

----------------------------------------------------------------------
-- 3. Daily progress summary for PFAS corridor (KER-linked)
----------------------------------------------------------------------

-- Aggregated daily PFAS corridor metrics per segment, with K,E,R and V_t window.[file:4][file:6][file:15]
CREATE TABLE IF NOT EXISTS pfas_corridor_dailyprogress (
    yyyymmdd          TEXT    NOT NULL,
    segment_id        TEXT    NOT NULL,
    domain            TEXT    NOT NULL, -- e.g. 'CYBOQUATIC', subdomain 'PFASFATECOLDSURVIVAL'[file:15]
    subtask_id        TEXT    NOT NULL, -- e.g. 'PHX-CANAL-PFAS-2026-07-29'[file:15]
    -- Window-level Lyapunov metrics.
    vt_max            REAL    NOT NULL,
    vt_min            REAL    NOT NULL,
    vt_avg            REAL    NOT NULL,
    -- K,E,R triad in [0,1].[file:4]
    k_knowledge       REAL    NOT NULL,
    e_ecoimpact       REAL    NOT NULL,
    r_risk            REAL    NOT NULL,
    -- Hex and governance bindings.[file:15][file:4]
    evidence_hex      TEXT    NOT NULL,
    prior_pointer_hex TEXT,
    signing_did       TEXT    NOT NULL,
    created_utc       TEXT    NOT NULL,
    PRIMARY KEY (yyyymmdd, segment_id),
    CHECK (k_knowledge >= 0.0 AND k_knowledge <= 1.0),
    CHECK (e_ecoimpact >= 0.0 AND e_ecoimpact <= 1.0),
    CHECK (r_risk      >= 0.0 AND r_risk      <= 1.0)
);

-- Non-actuating trigger to enforce KER corridor invariants on inserts:
-- - Require vt_max not to exceed a per-domain bound.
-- - Require K,E,R in gold band for research shards to be accepted.[file:4]
CREATE TRIGGER IF NOT EXISTS trg_pfas_corridor_dailyprogress_insert
BEFORE INSERT ON pfas_corridor_dailyprogress
FOR EACH ROW
BEGIN
    -- Enforce domain label.
    SELECT
        CASE
            WHEN NEW.domain <> 'CYBOQUATIC'
                 OR NEW.subtask_id IS NULL
            THEN RAISE(ABORT, 'pfas_corridor_dailyprogress: invalid domain or subtask_id')
        END;

    -- Enforce Lyapunov residual corridor (illustrative bound).[file:4]
    SELECT
        CASE
            WHEN NEW.vt_max > 1.0
            THEN RAISE(ABORT, 'pfas_corridor_dailyprogress: vt_max exceeds corridor bound')
        END;

    -- Enforce K,E,R research-band; production gating is handled elsewhere.[file:4]
    SELECT
        CASE
            WHEN NEW.k_knowledge < 0.80
              OR NEW.e_ecoimpact < 0.80
              OR NEW.r_risk     > 0.20
            THEN RAISE(ABORT, 'pfas_corridor_dailyprogress: KER triad outside research corridor')
        END;
END;

----------------------------------------------------------------------
-- 4. Seed example rows for 2026-07-29 (Phoenix PFAS corridor)
----------------------------------------------------------------------

-- These seeds link the PFAS corridor shard to the Phoenix Hex registry
-- via logical name and evidence_hex, consistent with PHXWORKLOADENERGYDV20260709
-- and PHXDRAINAGEDECAY20260708 patterns.[file:15]

INSERT OR IGNORE INTO pfas_corridor_coeffs (
    segment_id,
    k_deg_mobile_base,
    k_deg_sed_base,
    k_sorb,
    k_desorb,
    k_cold_alpha,
    w_pfas,
    w_cold,
    region_code,
    evidence_hex,
    signing_did,
    created_utc
) VALUES (
    'PHX-CANAL-SEG-PFAS-001',
    0.01,              -- illustrative normalized rate
    0.005,
    0.02,
    0.01,
    0.10,
    0.7,
    0.3,
    'PHX-CAZ-CEIM',
    '0x20260729PHXPFASColdCorridor',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '2026-07-29T00:00:00Z'
);

INSERT OR IGNORE INTO pfas_corridor_initial (
    segment_id,
    m_mobile_mg,
    m_sorbed_mg,
    cold_survival,
    degraded_fraction,
    r_pfas_init,
    r_cold_init,
    vt_residual_init,
    evidence_hex,
    signing_did,
    created_utc
) VALUES (
    'PHX-CANAL-SEG-PFAS-001',
    10.0,
    5.0,
    0.5,
    0.0,
    0.15,
    0.50,
    (0.7 * 0.15 * 0.15 + 0.3 * 0.50 * 0.50),
    '0x20260729PHXPFASColdCorridor',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '2026-07-29T00:00:00Z'
);

-- Example temperature risk window for 5 steps.
INSERT OR IGNORE INTO pfas_temp_risk_window (segment_id, step_k, r_temp) VALUES
('PHX-CANAL-SEG-PFAS-001', 1, 0.7),
('PHX-CANAL-SEG-PFAS-001', 2, 0.6),
('PHX-CANAL-SEG-PFAS-001', 3, 0.4),
('PHX-CANAL-SEG-PFAS-001', 4, 0.3),
('PHX-CANAL-SEG-PFAS-001', 5, 0.2);

-- Example daily progress row for 2026-07-29;
-- the vt_* and K,E,R values are placeholders to be overwritten by
-- actual corridor evaluation in CI.[file:4][file:15]
INSERT OR IGNORE INTO pfas_corridor_dailyprogress (
    yyyymmdd,
    segment_id,
    domain,
    subtask_id,
    vt_max,
    vt_min,
    vt_avg,
    k_knowledge,
    e_ecoimpact,
    r_risk,
    evidence_hex,
    prior_pointer_hex,
    signing_did,
    created_utc
) VALUES (
    '20260729',
    'PHX-CANAL-SEG-PFAS-001',
    'CYBOQUATIC',
    'PHX-CANAL-PFAS-2026-07-29',
    0.50,
    0.10,
    0.30,
    0.85,
    0.88,
    0.15,
    '0x20260729PHXPFASColdCorridor',
    '0x20260709PHX3345NWorkloadEnergyDeltaVt',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    '2026-07-29T00:00:00Z'
);
