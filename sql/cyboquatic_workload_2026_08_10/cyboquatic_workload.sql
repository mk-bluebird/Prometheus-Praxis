-- File: sql/cyboquatic_workload_2026_08_10/cyboquatic_workload.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_canal_node (
    node_id TEXT PRIMARY KEY,
    canal_node_parameter REAL NOT NULL CHECK (canal_node_parameter >= 0.0),
    fog_media_class TEXT NOT NULL CHECK (fog_media_class IN ('WATER', 'SEDIMENT', 'BIOFILM')),
    owner_did TEXT NOT NULL CHECK (owner_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7')
) STRICT;

CREATE TABLE IF NOT EXISTS cyboquatic_workload_frame (
    frame_id INTEGER PRIMARY KEY,
    node_id TEXT NOT NULL REFERENCES cyboquatic_canal_node(node_id),
    observed_utc TEXT NOT NULL,
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0),
    delta_vt REAL NOT NULL CHECK (delta_vt >= 0.0 AND delta_vt <= 1.0),
    k_knowledge REAL NOT NULL CHECK (k_knowledge BETWEEN 0.0 AND 1.0),
    e_eco_impact REAL NOT NULL CHECK (e_eco_impact BETWEEN 0.0 AND 1.0),
    r_risk REAL NOT NULL CHECK (r_risk BETWEEN 0.0 AND 1.0),
    fog_media_class TEXT NOT NULL CHECK (fog_media_class IN ('WATER', 'SEDIMENT', 'BIOFILM')),
    canal_node_parameter REAL NOT NULL CHECK (canal_node_parameter >= 0.0),
    CHECK (r_risk >= delta_vt),
    CHECK (e_eco_impact >= 0.55 OR r_risk >= 0.45)
) STRICT;

CREATE INDEX IF NOT EXISTS idx_cyboquatic_frame_node_time
ON cyboquatic_workload_frame(node_id, observed_utc DESC);

CREATE TRIGGER IF NOT EXISTS cyboquatic_workload_corridor_guard
BEFORE INSERT ON cyboquatic_workload_frame
FOR EACH ROW WHEN NEW.delta_vt > 0.20 OR NEW.k_knowledge < 0.60 OR NEW.e_eco_impact < 0.55
BEGIN
    SELECT RAISE(ABORT, 'Cyboquatic workload frame violates low-impact KER corridor');
END;
