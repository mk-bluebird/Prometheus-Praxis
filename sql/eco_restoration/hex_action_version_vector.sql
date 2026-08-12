-- File: sql/eco_restoration/hex_action_version_vector.sql

CREATE TABLE IF NOT EXISTS hex_action_change (
    hex_anchor INTEGER NOT NULL,
    actor_did TEXT NOT NULL,
    counter INTEGER NOT NULL CHECK(counter > 0),
    action TEXT NOT NULL,
    observed_unix_s INTEGER NOT NULL,
    PRIMARY KEY(hex_anchor, actor_did, counter),
    CHECK(action IN (
        'tree_planting','canal_cleaning','native_seedling','mulch_application',
        'infiltration_basin','habitat_monitoring','soil_amendment'
    ))
) STRICT;

CREATE TABLE IF NOT EXISTS hex_action_version (
    hex_anchor INTEGER NOT NULL,
    actor_did TEXT NOT NULL,
    counter INTEGER NOT NULL CHECK(counter >= 0),
    PRIMARY KEY(hex_anchor, actor_did)
) STRICT;

CREATE TABLE IF NOT EXISTS hex_action_resolution (
    hex_anchor INTEGER PRIMARY KEY,
    action TEXT,
    state TEXT NOT NULL CHECK(state IN ('RESOLVED','CONFLICT')),
    resolved_unix_s INTEGER NOT NULL
) STRICT;

CREATE TRIGGER IF NOT EXISTS hex_action_change_counter_guard
BEFORE INSERT ON hex_action_change
FOR EACH ROW
WHEN NEW.counter <> COALESCE((
    SELECT counter + 1
    FROM hex_action_version
    WHERE hex_anchor = NEW.hex_anchor AND actor_did = NEW.actor_did
), 1)
BEGIN
    SELECT RAISE(ABORT, 'version vector counter is not consecutive');
END;

CREATE TRIGGER IF NOT EXISTS hex_action_change_version_update
AFTER INSERT ON hex_action_change
FOR EACH ROW
BEGIN
    INSERT INTO hex_action_version VALUES(NEW.hex_anchor, NEW.actor_did, NEW.counter)
    ON CONFLICT(hex_anchor, actor_did) DO UPDATE SET counter = excluded.counter;
END;

CREATE VIEW IF NOT EXISTS hex_action_concurrent_frontier AS
SELECT change.hex_anchor, change.actor_did, change.counter, change.action
FROM hex_action_change AS change
JOIN hex_action_version AS version
  ON version.hex_anchor = change.hex_anchor
 AND version.actor_did = change.actor_did
 AND version.counter = change.counter;
