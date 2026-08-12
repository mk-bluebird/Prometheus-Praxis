-- File: sql/ppx_ai_workload/heat_workload_admission.sql
CREATE TABLE IF NOT EXISTS ppx_workload_admission (
    workload_id TEXT PRIMARY KEY,
    hex_anchor INTEGER NOT NULL,
    power_w REAL NOT NULL CHECK(power_w >= 0.0),
    action TEXT NOT NULL CHECK(action IN ('PROCEED', 'DERATE', 'HALT')),
    decision_json TEXT NOT NULL CHECK(json_valid(decision_json))
) STRICT;

CREATE TRIGGER IF NOT EXISTS ppx_heat_power_admission_guard
BEFORE INSERT ON ppx_workload_admission
FOR EACH ROW
WHEN NEW.power_w > 500.0
 AND NEW.action = 'PROCEED'
 AND EXISTS (
    SELECT 1 FROM ppx_heat_risk
    WHERE hex_anchor = NEW.hex_anchor
      AND r_heat > 0.8
 )
BEGIN
    SELECT RAISE(ABORT, 'high-power workload blocked in high heat-risk hex');
END;
