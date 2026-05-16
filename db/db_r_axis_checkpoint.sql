-- filename: db_r_axis_checkpoint.sql
-- destination: ecorestorationshard/db/db_r_axis_checkpoint.sql

PRAGMA foreign_keys = ON;

-- Per-host r_axis history snapshot table.
CREATE TABLE IF NOT EXISTS eco_r_axis_history (
    checkpoint_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    host_address       TEXT    NOT NULL,
    block_height       INTEGER NOT NULL,
    r_axis_value       REAL    NOT NULL,
    bls_agg_signature  BLOB    NOT NULL,
    validator_set_hash BLOB    NOT NULL,
    created_utc        TEXT    NOT NULL,
    UNIQUE (host_address, block_height)
);

-- View for last known good r_axis per host.
CREATE VIEW IF NOT EXISTS v_last_known_good_r_axis AS
SELECT
    h.host_address,
    h.block_height,
    h.r_axis_value,
    h.bls_agg_signature,
    h.validator_set_hash
FROM eco_r_axis_history AS h
JOIN (
    SELECT host_address, MAX(block_height) AS max_height
    FROM eco_r_axis_history
    GROUP BY host_address
) AS g
ON h.host_address = g.host_address AND h.block_height = g.max_height;
