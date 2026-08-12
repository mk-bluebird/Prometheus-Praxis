-- File: sql/eco_restoration/hex_spatiotemporal_index.sql

CREATE TABLE IF NOT EXISTS hex_environment_event (
    event_id INTEGER PRIMARY KEY,
    hex_anchor INTEGER NOT NULL,
    observed_start_unix_s INTEGER NOT NULL,
    observed_end_unix_s INTEGER NOT NULL CHECK(observed_end_unix_s >= observed_start_unix_s),
    center_x_m REAL NOT NULL,
    center_y_m REAL NOT NULL,
    r_heat REAL NOT NULL CHECK(r_heat BETWEEN 0 AND 1),
    CHECK(hex_anchor >= 0)
) STRICT;

CREATE VIRTUAL TABLE IF NOT EXISTS hex_environment_rtree
USING rtree(
    event_id,
    min_x_m, max_x_m,
    min_y_m, max_y_m,
    min_hour, max_hour
);

CREATE INDEX IF NOT EXISTS hex_heat_recent_index
ON hex_environment_event(r_heat, observed_end_unix_s DESC, hex_anchor);

CREATE TRIGGER IF NOT EXISTS hex_environment_rtree_insert
AFTER INSERT ON hex_environment_event
FOR EACH ROW
BEGIN
    INSERT INTO hex_environment_rtree VALUES(
        NEW.event_id,
        NEW.center_x_m, NEW.center_x_m,
        NEW.center_y_m, NEW.center_y_m,
        NEW.observed_start_unix_s / 3600,
        NEW.observed_end_unix_s / 3600
    );
END;

CREATE TRIGGER IF NOT EXISTS hex_environment_rtree_update
AFTER UPDATE OF center_x_m, center_y_m, observed_start_unix_s, observed_end_unix_s
ON hex_environment_event
FOR EACH ROW
BEGIN
    UPDATE hex_environment_rtree SET
        min_x_m = NEW.center_x_m,
        max_x_m = NEW.center_x_m,
        min_y_m = NEW.center_y_m,
        max_y_m = NEW.center_y_m,
        min_hour = NEW.observed_start_unix_s / 3600,
        max_hour = NEW.observed_end_unix_s / 3600
    WHERE event_id = NEW.event_id;
END;

CREATE TRIGGER IF NOT EXISTS hex_environment_rtree_delete
AFTER DELETE ON hex_environment_event
FOR EACH ROW
BEGIN
    DELETE FROM hex_environment_rtree WHERE event_id = OLD.event_id;
END;

SELECT
    event.hex_anchor,
    event.observed_start_unix_s,
    event.observed_end_unix_s,
    event.r_heat
FROM hex_environment_rtree AS spatial_time
JOIN hex_environment_event AS event ON event.event_id = spatial_time.event_id
WHERE event.r_heat > 0.8
  AND spatial_time.max_hour >= (:now_unix_s - 86400) / 3600
  AND spatial_time.min_hour <= :now_unix_s / 3600
ORDER BY event.observed_end_unix_s DESC;
