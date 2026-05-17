-- filename dbresidual_monitor.sql
-- destination Eco-Fort/db/dbresidual_monitor.sql
-- repo-target github.com/mk-bluebird/Eco-Fort

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS residual_monitor (
  kernel_id      TEXT PRIMARY KEY,
  k_metric       REAL NOT NULL,
  e_metric       REAL NOT NULL,
  r_metric       REAL NOT NULL,
  vt_residual    REAL NOT NULL,
  ker_ok         INTEGER NOT NULL CHECK (ker_ok IN (0,1)),
  reason_code    TEXT NOT NULL,   -- e.g. 'RESIDUAL_DRIFT', 'PLANEWEIGHT_MISMATCH'
  updated_utc    TEXT NOT NULL
);

CREATE VIEW IF NOT EXISTS v_residual_guard_pricing AS
SELECT
  kernel_id,
  k_metric,
  e_metric,
  r_metric,
  vt_residual,
  ker_ok,
  reason_code
FROM residual_monitor
WHERE ker_ok = 0;
