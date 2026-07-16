-- filename: eco_restoration_shard/db/fog_router_log.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS fog_router_log (
    log_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id         TEXT NOT NULL,
    region          TEXT NOT NULL,
    t_seen          TEXT NOT NULL,
    sensor_id       TEXT NOT NULL,

    bod_mg_per_l    REAL,
    tss_mg_per_l    REAL,
    cec_proxy       REAL,

    flag_turbid_spike    INTEGER NOT NULL CHECK (flag_turbid_spike IN (0,1)),
    flag_unmodeled_media INTEGER NOT NULL CHECK (flag_unmodeled_media IN (0,1)),
    flag_cold_shock      INTEGER NOT NULL CHECK (flag_cold_shock IN (0,1)),

    corridor_id     TEXT NOT NULL,
    evidence_hex    TEXT NOT NULL,
    signing_did     TEXT NOT NULL,
    created_utc     TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_fog_router_node_time
    ON fog_router_log (node_id, t_seen);
