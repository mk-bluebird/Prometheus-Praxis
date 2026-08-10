-- File: sql/cyboquatic/workload_telemetry_2026_08_09.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_workload_frame (
    frame_id INTEGER PRIMARY KEY,
    observed_utc TEXT NOT NULL,
    node_id TEXT NOT NULL,
    owner_did TEXT NOT NULL CHECK (owner_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0),
    delta_vt REAL NOT NULL CHECK (delta_vt BETWEEN 0.0 AND 1.0),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor BETWEEN 0.0 AND 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value BETWEEN 0.0 AND 1.0),
    ker_k REAL NOT NULL CHECK (ker_k BETWEEN 0.0 AND 1.0),
    ker_e REAL NOT NULL CHECK (ker_e BETWEEN 0.0 AND 1.0),
    ker_r REAL NOT NULL CHECK (ker_r BETWEEN 0.0 AND 1.0),
    fog_confidence REAL NOT NULL CHECK (fog_confidence BETWEEN 0.0 AND 1.0),
    canal_node TEXT NOT NULL,
    accepted INTEGER NOT NULL CHECK (accepted IN (0, 1)),
    CHECK (accepted = 0 OR (delta_vt <= 0.35 AND eco_impact_value >= 0.60 AND ker_k * ker_e > ker_r))
);

CREATE INDEX IF NOT EXISTS idx_cyboquatic_workload_node_time
ON cyboquatic_workload_frame(node_id, observed_utc DESC);

CREATE TRIGGER IF NOT EXISTS reject_unverified_canal_frame
BEFORE INSERT ON cyboquatic_workload_frame
WHEN NEW.fog_confidence < 0.75
BEGIN
    SELECT RAISE(ABORT, 'FOG confidence below operational corridor');
END;
