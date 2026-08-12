-- File: sql/ppx_ai_workload/lane_admission_insert_trigger.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ppx_lane_threshold (
    lane TEXT PRIMARY KEY CHECK(lane IN ('RESEARCH', 'PILOT', 'PRODUCTION')),
    k_min REAL NOT NULL CHECK(k_min BETWEEN 0.0 AND 1.0),
    e_min REAL NOT NULL CHECK(e_min BETWEEN 0.0 AND 1.0),
    r_max REAL NOT NULL CHECK(r_max BETWEEN 0.0 AND 1.0)
) STRICT;

CREATE TABLE IF NOT EXISTS ppx_lane_admission_base (
    workload_id TEXT PRIMARY KEY,
    observed_utc TEXT NOT NULL,
    lane TEXT NOT NULL REFERENCES ppx_lane_threshold(lane),
    k_knowledge REAL NOT NULL CHECK(k_knowledge BETWEEN 0.0 AND 1.0),
    e_eco_impact REAL NOT NULL CHECK(e_eco_impact BETWEEN 0.0 AND 1.0),
    r_risk REAL NOT NULL CHECK(r_risk BETWEEN 0.0 AND 1.0),
    action TEXT NOT NULL CHECK(action IN ('PROCEED', 'DERATE', 'HALT')),
    reason_code TEXT NOT NULL
) STRICT;

CREATE TABLE IF NOT EXISTS ppx_lane_admission_audit (
    audit_id INTEGER PRIMARY KEY,
    workload_id TEXT NOT NULL,
    observed_utc TEXT NOT NULL,
    lane TEXT NOT NULL,
    k_knowledge REAL NOT NULL,
    e_eco_impact REAL NOT NULL,
    r_risk REAL NOT NULL,
    action TEXT NOT NULL,
    rejection_reason TEXT NOT NULL,
    audited_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
) STRICT;

CREATE VIEW IF NOT EXISTS v_ppx_lane_admission AS
SELECT
    workload_id,
    observed_utc,
    lane,
    k_knowledge,
    e_eco_impact,
    r_risk,
    action,
    reason_code
FROM ppx_lane_admission_base;

CREATE TRIGGER IF NOT EXISTS ppx_lane_admission_filter_insert
INSTEAD OF INSERT ON v_ppx_lane_admission
FOR EACH ROW
BEGIN
    INSERT INTO ppx_lane_admission_audit(
        workload_id, observed_utc, lane, k_knowledge, e_eco_impact, r_risk, action, rejection_reason
    )
    SELECT
        NEW.workload_id,
        NEW.observed_utc,
        NEW.lane,
        NEW.k_knowledge,
        NEW.e_eco_impact,
        NEW.r_risk,
        NEW.action,
        CASE
            WHEN NOT EXISTS (
                SELECT 1 FROM ppx_lane_threshold WHERE lane = NEW.lane
            ) THEN 'unknown_lane'
            WHEN NEW.k_knowledge < (
                SELECT k_min FROM ppx_lane_threshold WHERE lane = NEW.lane
            ) THEN 'knowledge_below_lane_min'
            WHEN NEW.e_eco_impact < (
                SELECT e_min FROM ppx_lane_threshold WHERE lane = NEW.lane
            ) THEN 'eco_impact_below_lane_min'
            WHEN NEW.r_risk > (
                SELECT r_max FROM ppx_lane_threshold WHERE lane = NEW.lane
            ) THEN 'risk_above_lane_max'
            ELSE 'invalid_admission_record'
        END
    WHERE NOT EXISTS (
        SELECT 1
        FROM ppx_lane_threshold AS threshold
        WHERE threshold.lane = NEW.lane
          AND NEW.k_knowledge >= threshold.k_min
          AND NEW.e_eco_impact >= threshold.e_min
          AND NEW.r_risk <= threshold.r_max
    );

    INSERT INTO ppx_lane_admission_base(
        workload_id, observed_utc, lane, k_knowledge, e_eco_impact, r_risk, action, reason_code
    )
    SELECT
        NEW.workload_id,
        NEW.observed_utc,
        NEW.lane,
        NEW.k_knowledge,
        NEW.e_eco_impact,
        NEW.r_risk,
        NEW.action,
        NEW.reason_code
    WHERE EXISTS (
        SELECT 1
        FROM ppx_lane_threshold AS threshold
        WHERE threshold.lane = NEW.lane
          AND NEW.k_knowledge >= threshold.k_min
          AND NEW.e_eco_impact >= threshold.e_min
          AND NEW.r_risk <= threshold.r_max
    );
END;
