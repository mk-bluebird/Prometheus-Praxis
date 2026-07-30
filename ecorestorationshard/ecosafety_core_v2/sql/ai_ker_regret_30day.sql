-- filename: ecorestorationshard/ecosafety_core_v2/sql/ai_ker_regret_30day.sql
-- destination: ecorestorationshard/ecosafety_core_v2/sql/ai_ker_regret_30day.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Non-actuating SQLite shard to:
--     - Record per-day KER snapshots per AI agent.
--     - Compute 30-day regret aggregates (sum, average) per agent.
--   Consistent with AIKERRegret30Day2026v1.[4]

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ai_ker_daily (
    agent_id      TEXT    NOT NULL,
    yyyymmdd      TEXT    NOT NULL,
    k_actual      REAL    NOT NULL,
    e_actual      REAL    NOT NULL,
    r_actual      REAL    NOT NULL,
    k_target      REAL    NOT NULL,
    e_target      REAL    NOT NULL,
    r_target      REAL    NOT NULL,
    regret_daily  REAL    NOT NULL,
    evidence_hex  TEXT    NOT NULL,
    created_utc   TEXT    NOT NULL,
    PRIMARY KEY (agent_id, yyyymmdd),
    CHECK (k_actual BETWEEN 0.0 AND 1.0),
    CHECK (e_actual BETWEEN 0.0 AND 1.0),
    CHECK (r_actual BETWEEN 0.0 AND 1.0),
    CHECK (k_target BETWEEN 0.0 AND 1.0),
    CHECK (e_target BETWEEN 0.0 AND 1.0),
    CHECK (r_target BETWEEN 0.0 AND 1.0),
    CHECK (regret_daily >= 0.0)
);

CREATE TABLE IF NOT EXISTS ai_ker_regret_30day (
    agent_id      TEXT    NOT NULL,
    window_start  TEXT    NOT NULL,
    window_end    TEXT    NOT NULL,
    regret_sum    REAL    NOT NULL,
    regret_avg    REAL    NOT NULL,
    k_target      REAL    NOT NULL,
    e_target      REAL    NOT NULL,
    r_target      REAL    NOT NULL,
    evidence_hex  TEXT    NOT NULL,
    created_utc   TEXT    NOT NULL,
    PRIMARY KEY (agent_id, window_start, window_end),
    CHECK (regret_sum >= 0.0),
    CHECK (regret_avg >= 0.0),
    CHECK (k_target BETWEEN 0.0 AND 1.0),
    CHECK (e_target BETWEEN 0.0 AND 1.0),
    CHECK (r_target BETWEEN 0.0 AND 1.0)
);

-- Trigger: enforce daily regret consistency with KER targets.
CREATE TRIGGER IF NOT EXISTS trg_ai_ker_daily_insert
BEFORE INSERT ON ai_ker_daily
FOR EACH ROW
BEGIN
    -- Ensure regret_daily is at least the sum of component regrets.[4]
    SELECT
        CASE
            WHEN NEW.regret_daily <
                 (CASE WHEN NEW.k_target > NEW.k_actual
                       THEN NEW.k_target - NEW.k_actual
                       ELSE 0.0 END)
               + (CASE WHEN NEW.e_target > NEW.e_actual
                       THEN NEW.e_target - NEW.e_actual
                       ELSE 0.0 END)
               + (CASE WHEN NEW.r_actual > NEW.r_target
                       THEN NEW.r_actual - NEW.r_target
                       ELSE 0.0 END)
            THEN RAISE(ABORT, 'ai_ker_daily: regret_daily too small vs KER diffs')
        END;
END;

-- Trigger: compute 30-day regret aggregates from ai_ker_daily rows.
-- This assumes exactly 30 days in the window; CI or tooling should
-- ensure window coverage.
CREATE TRIGGER IF NOT EXISTS trg_ai_ker_regret_30day_insert
BEFORE INSERT ON ai_ker_regret_30day
FOR EACH ROW
BEGIN
    -- Compute regret_sum over [window_start, window_end].
    SELECT
        CASE
            WHEN NEW.regret_sum <
                 (SELECT SUM(regret_daily)
                  FROM ai_ker_daily
                  WHERE agent_id = NEW.agent_id
                    AND yyyymmdd BETWEEN NEW.window_start AND NEW.window_end)
            THEN RAISE(ABORT, 'ai_ker_regret_30day: regret_sum too small vs daily rows')
        END;

    -- Enforce regret_avg = regret_sum / 30.0 (30-day horizon).
    SELECT
        CASE
            WHEN ABS(NEW.regret_avg - NEW.regret_sum / 30.0) > 1e-6
            THEN RAISE(ABORT, 'ai_ker_regret_30day: regret_avg must equal regret_sum/30')
        END;
END;
