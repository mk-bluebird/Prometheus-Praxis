-- filename: db/dbcyboquaticdailyprogress.sql
-- license: MIT OR Apache-2.0
-- role: Canonical cyboquatic daily progress index across domains (a–g).
-- note: Non-actuating governance DB; stores diagnostics only.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Daily progress table
-- Consolidates per-day domain shards (a–g) into a single index.
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyboquatic_daily_progress (
    progress_id            TEXT PRIMARY KEY,           -- hex or UUID
    date_utc               TEXT NOT NULL,              -- YYYY-MM-DD
    domain_code            TEXT NOT NULL,              -- 'a'..'g' per cyboquatic daily rotation
    subtask_id             TEXT NOT NULL,              -- e.g. '20260722-e-drainagedecay'
    node_id                TEXT NOT NULL,              -- canal/machinery node
    window_start_utc       TEXT NOT NULL,              -- ISO8601
    window_end_utc         TEXT NOT NULL,              -- ISO8601

    -- KER triad
    k_knowledge            REAL NOT NULL CHECK (k_knowledge  >= 0.0 AND k_knowledge  <= 1.0),
    e_ecoimpact            REAL NOT NULL CHECK (e_ecoimpact  >= 0.0 AND e_ecoimpact  <= 1.0),
    r_risk                 REAL NOT NULL CHECK (r_risk       >= 0.0 AND r_risk       <= 1.0),
    ker_score              REAL NOT NULL,              -- k * e - r, enforced by trigger

    -- Lyapunov residual
    vt_current             REAL NOT NULL,              -- current Lyapunov value
    vt_next                REAL NOT NULL,              -- next Lyapunov value
    vt_delta               REAL NOT NULL,              -- vt_next - vt_current

    -- Lane and governance
    lane                   TEXT NOT NULL,              -- RESEARCH/PILOT/PRODUCTION
    safetopromote_ok       INTEGER NOT NULL CHECK (safetopromote_ok IN (0, 1)),

    -- Phoenix evidence and DID binding
    evidence_hex           TEXT NOT NULL,              -- hex string bound to Phoenix registry
    signing_did            TEXT NOT NULL,              -- e.g. bostrom DID
    created_at_utc         TEXT NOT NULL,
    last_updated_utc       TEXT NOT NULL
);

----------------------------------------------------------------------
-- 2. Indices
----------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_daily_progress_date_domain
    ON cyboquatic_daily_progress (date_utc, domain_code);

CREATE INDEX IF NOT EXISTS idx_daily_progress_node_window
    ON cyboquatic_daily_progress (node_id, window_start_utc, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_daily_progress_ker
    ON cyboquatic_daily_progress (k_knowledge, e_ecoimpact, r_risk, ker_score);

CREATE INDEX IF NOT EXISTS idx_daily_progress_lane
    ON cyboquatic_daily_progress (lane, safetopromote_ok);

CREATE INDEX IF NOT EXISTS idx_daily_progress_evidence
    ON cyboquatic_daily_progress (evidence_hex, signing_did);

----------------------------------------------------------------------
-- 3. Trigger: enforce KER score and Lyapunov non-increase where required
----------------------------------------------------------------------

DROP TRIGGER IF EXISTS trg_daily_progress_ker_lyapunov;

CREATE TRIGGER trg_daily_progress_ker_lyapunov
BEFORE INSERT ON cyboquatic_daily_progress
BEGIN
    -- KER score consistency: ker_score ~= k * e - r
    SELECT
        CASE
            WHEN ABS(NEW.k_knowledge * NEW.e_ecoimpact - NEW.r_risk - NEW.ker_score) > 0.000001
            THEN RAISE(ABORT, 'ker_score inconsistent with KER triad for daily progress')
        END;

    -- Lyapunov residual must not be increasing beyond 0 (non-regressive corridor).
    SELECT
        CASE
            WHEN NEW.vt_delta > 0.0
            THEN RAISE(ABORT, 'Lyapunov residual must be non-increasing (vt_delta <= 0)')
        END;
END;
