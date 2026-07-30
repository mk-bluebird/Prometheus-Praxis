-- filename: ecorestorationshard/cyboquaticprogress/pfas_delay/pfas_delay_guard_20260729.sql
-- destination: ecorestorationshard/cyboquaticprogress/pfas_delay/pfas_delay_guard_20260729.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Non-actuating SQLite shard enforcing PFAS delay guard:
--   tau_current <= tau_max, and tau_max derived from Lyapunov/KER
--   parameters for each segment, consistent with PFASFateDelayGuard2026v1.[4][6]

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS pfas_delay_params (
    segment_id   TEXT    PRIMARY KEY,
    tau_current  INTEGER NOT NULL,
    tau_max      INTEGER NOT NULL,
    w_pfas       REAL    NOT NULL,
    alpha_pfas   REAL    NOT NULL,
    beta_pfas    REAL    NOT NULL,
    L_pfas       REAL    NOT NULL,
    epsilon      REAL    NOT NULL,
    evidence_hex TEXT    NOT NULL,
    created_utc  TEXT    NOT NULL,
    CHECK (tau_current >= 0),
    CHECK (tau_max     >= 0),
    CHECK (w_pfas      >  0.0),
    CHECK (alpha_pfas  >  0.0),
    CHECK (beta_pfas   >  0.0),
    CHECK (L_pfas      >  0.0),
    CHECK (epsilon     >  0.0)
);

-- Trigger: derive tau_max from epsilon, w_pfas, alpha_pfas, beta_pfas, L_pfas
-- and enforce tau_current <= tau_max.[4][6]
CREATE TRIGGER IF NOT EXISTS trg_pfas_delay_params_insert
BEFORE INSERT ON pfas_delay_params
FOR EACH ROW
BEGIN
    -- Compute derived tau_max_bound = epsilon / (w_pfas * (2*alpha_pfas + 4*beta_pfas) * L_pfas).
    -- SQLite does not have variables, so we recompute inline in checks.
    SELECT
        CASE
            WHEN NEW.w_pfas <= 0.0
              OR NEW.alpha_pfas <= 0.0
              OR NEW.beta_pfas  <= 0.0
              OR NEW.L_pfas     <= 0.0
            THEN RAISE(ABORT, 'pfas_delay_params: invalid Lyapunov parameters')
        END;

    SELECT
        CASE
            WHEN NEW.tau_max >
                 NEW.epsilon / (NEW.w_pfas * (2.0*NEW.alpha_pfas + 4.0*NEW.beta_pfas) * NEW.L_pfas)
            THEN RAISE(ABORT, 'pfas_delay_params: tau_max exceeds admissible delay bound')
        END;

    SELECT
        CASE
            WHEN NEW.tau_current > NEW.tau_max
            THEN RAISE(ABORT, 'pfas_delay_params: tau_current exceeds tau_max')
        END;
END;

CREATE TRIGGER IF NOT EXISTS trg_pfas_delay_params_update
BEFORE UPDATE ON pfas_delay_params
FOR EACH ROW
BEGIN
    SELECT
        CASE
            WHEN NEW.w_pfas <= 0.0
              OR NEW.alpha_pfas <= 0.0
              OR NEW.beta_pfas  <= 0.0
              OR NEW.L_pfas     <= 0.0
            THEN RAISE(ABORT, 'pfas_delay_params: invalid Lyapunov parameters (update)')
        END;

    SELECT
        CASE
            WHEN NEW.tau_max >
                 NEW.epsilon / (NEW.w_pfas * (2.0*NEW.alpha_pfas + 4.0*NEW.beta_pfas) * NEW.L_pfas)
            THEN RAISE(ABORT, 'pfas_delay_params: tau_max exceeds admissible delay bound (update)')
        END;

    SELECT
        CASE
            WHEN NEW.tau_current > NEW.tau_max
            THEN RAISE(ABORT, 'pfas_delay_params: tau_current exceeds tau_max (update)')
        END;
END;
