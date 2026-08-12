-- File: sql/eco_restoration/lane_threshold_degradation_alerts.sql
CREATE TABLE IF NOT EXISTS lane_threshold_metric (
    observed_unix_s INTEGER NOT NULL,
    threshold_set_id TEXT NOT NULL,
    knowledge_factor REAL NOT NULL CHECK(knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK(eco_impact_value BETWEEN 0.0 AND 1.0),
    risk REAL NOT NULL CHECK(risk BETWEEN 0.0 AND 1.0),
    PRIMARY KEY(observed_unix_s, threshold_set_id)
) STRICT;

CREATE TABLE IF NOT EXISTS lane_threshold_degradation_alert (
    alert_id INTEGER PRIMARY KEY,
    threshold_set_id TEXT NOT NULL,
    observed_unix_s INTEGER NOT NULL,
    mean_knowledge_factor REAL NOT NULL,
    mean_eco_impact_value REAL NOT NULL,
    mean_risk REAL NOT NULL,
    status TEXT NOT NULL DEFAULT 'PENDING'
        CHECK(status IN ('PENDING','ACKNOWLEDGED','RESOLVED')),
    UNIQUE(threshold_set_id, observed_unix_s)
) STRICT;

CREATE INDEX IF NOT EXISTS lane_threshold_metric_set_time
ON lane_threshold_metric(threshold_set_id, observed_unix_s);

CREATE TRIGGER IF NOT EXISTS lane_threshold_metric_degradation_alert
AFTER INSERT ON lane_threshold_metric
WHEN (
    SELECT COUNT(*)
    FROM lane_threshold_metric
    WHERE threshold_set_id = NEW.threshold_set_id
      AND observed_unix_s BETWEEN NEW.observed_unix_s - 604800 AND NEW.observed_unix_s
) >= 7
AND (
    SELECT AVG(knowledge_factor)
    FROM lane_threshold_metric
    WHERE threshold_set_id = NEW.threshold_set_id
      AND observed_unix_s BETWEEN NEW.observed_unix_s - 604800 AND NEW.observed_unix_s
) < 0.55
OR (
    SELECT AVG(eco_impact_value)
    FROM lane_threshold_metric
    WHERE threshold_set_id = NEW.threshold_set_id
      AND observed_unix_s BETWEEN NEW.observed_unix_s - 604800 AND NEW.observed_unix_s
) < 0.55
OR (
    SELECT AVG(risk)
    FROM lane_threshold_metric
    WHERE threshold_set_id = NEW.threshold_set_id
      AND observed_unix_s BETWEEN NEW.observed_unix_s - 604800 AND NEW.observed_unix_s
) > 0.35
BEGIN
    INSERT OR IGNORE INTO lane_threshold_degradation_alert(
        threshold_set_id, observed_unix_s, mean_knowledge_factor,
        mean_eco_impact_value, mean_risk
    )
    SELECT NEW.threshold_set_id, NEW.observed_unix_s,
           AVG(knowledge_factor), AVG(eco_impact_value), AVG(risk)
    FROM lane_threshold_metric
    WHERE threshold_set_id = NEW.threshold_set_id
      AND observed_unix_s BETWEEN NEW.observed_unix_s - 604800 AND NEW.observed_unix_s;
END;
