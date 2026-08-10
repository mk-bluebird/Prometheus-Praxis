-- File: sql/cyboquatic/2026_08_08_workload_telemetry.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_workload_frame (
    frame_id INTEGER PRIMARY KEY,
    observed_at TEXT NOT NULL,
    node_id TEXT NOT NULL,
    canal_node TEXT NOT NULL,
    fog_media TEXT NOT NULL CHECK (fog_media IN ('canal-water','sediment','bioswale','recycled-polymer')),
    energyreq_j REAL NOT NULL CHECK (energyreq_j >= 0.0),
    delta_vt REAL NOT NULL CHECK (delta_vt >= 0.0 AND delta_vt <= 1.0),
    ker_k REAL NOT NULL CHECK (ker_k >= 0.0 AND ker_k <= 1.0),
    ker_e REAL NOT NULL CHECK (ker_e >= 0.0 AND ker_e <= 1.0),
    ker_r REAL NOT NULL CHECK (ker_r >= 0.0 AND ker_r <= 1.0),
    renewable_fraction REAL NOT NULL CHECK (renewable_fraction >= 0.0 AND renewable_fraction <= 1.0),
    knowledge_factor REAL NOT NULL CHECK (knowledge_factor >= 0.0 AND knowledge_factor <= 1.0),
    eco_impact_value REAL NOT NULL CHECK (eco_impact_value >= 0.0 AND eco_impact_value <= 1.0),
    CHECK (ABS(ker_k - (1.0 - delta_vt)) <= 0.000001),
    CHECK (ker_k * ker_e > ker_r),
    CHECK (renewable_fraction >= 0.70),
    CHECK (delta_vt <= 0.25)
);

CREATE INDEX IF NOT EXISTS idx_workload_node_time
ON cyboquatic_workload_frame(node_id, observed_at);

CREATE INDEX IF NOT EXISTS idx_workload_canal_fog
ON cyboquatic_workload_frame(canal_node, fog_media);
