-- filename: ecorestorationshard/cyboquatic_progress/20260728/sql/cyboquatic_ker_invariants.sql
-- purpose: Strict invariants on KER, FOG, and Canal node parameters for domain (d)

PRAGMA foreign_keys = ON;

-- KER invariants: K,E,R in [0,1], non-regressive residual, corridor-safe energy
CREATE TRIGGER IF NOT EXISTS trg_dailyprogress_ker_invariants
BEFORE INSERT ON dailyprogress
FOR EACH ROW
BEGIN
  -- KER bounds
  SELECT
    CASE
      WHEN NEW.k_metric < 0.0 OR NEW.k_metric > 1.0 THEN
        RAISE(ABORT, 'k_metric out of [0,1]')
      WHEN NEW.e_metric < 0.0 OR NEW.e_metric > 1.0 THEN
        RAISE(ABORT, 'e_metric out of [0,1]')
      WHEN NEW.r_metric < 0.0 OR NEW.r_metric > 1.0 THEN
        RAISE(ABORT, 'r_metric out of [0,1]')
    END;

  -- Non-regression: delta_vt <= 0
  SELECT
    CASE
      WHEN NEW.delta_vt > 0.0 THEN
        RAISE(ABORT, 'delta_vt violates non-regression corridor')
    END;

  -- Energy corridor: r_energy <= 1.0
  SELECT
    CASE
      WHEN NEW.r_energy > 1.0 THEN
        RAISE(ABORT, 'r_energy exceeds corridor edge')
    END;

  -- Canal velocity and sensor health normalized to [0,1]
  SELECT
    CASE
      WHEN NEW.canal_velocity_ms < 0.0 OR NEW.canal_velocity_ms > 1.0 THEN
        RAISE(ABORT, 'canal_velocity_ms must be normalized [0,1]')
      WHEN NEW.sensor_health_risk < 0.0 OR NEW.sensor_health_risk > 1.0 THEN
        RAISE(ABORT, 'sensor_health_risk must be normalized [0,1]')
    END;
END;
