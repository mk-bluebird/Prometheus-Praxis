-- File: sql/cyboquatic_workload_20260807/cyboquatic_workload.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cyboquatic_workload_frame (
    frame_id INTEGER PRIMARY KEY,
    observed_utc TEXT NOT NULL CHECK(length(observed_utc) = 20),
    node_id TEXT NOT NULL CHECK(length(node_id) BETWEEN 1 AND 96),
    owner_did TEXT NOT NULL CHECK(owner_did = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'),
    canal_node TEXT NOT NULL CHECK(length(canal_node) BETWEEN 1 AND 96),
    fog_state TEXT NOT NULL CHECK(fog_state IN ('CLEAR', 'UNCERTAIN', 'BLOCKED')),
    energyreq_j REAL NOT NULL CHECK(energyreq_j >= 0.0),
    delta_vt REAL NOT NULL,
    renewable_fraction REAL NOT NULL CHECK(renewable_fraction BETWEEN 0.0 AND 1.0),
    recovered_energy_j REAL NOT NULL CHECK(recovered_energy_j >= 0.0),
    water_quality_gain REAL NOT NULL CHECK(water_quality_gain BETWEEN 0.0 AND 1.0),
    k_factor REAL NOT NULL CHECK(k_factor BETWEEN 0.0 AND 1.0),
    e_eco_impact REAL NOT NULL CHECK(e_eco_impact BETWEEN 0.0 AND 1.0),
    r_residual REAL NOT NULL CHECK(r_residual BETWEEN 0.0 AND 1.0),
    decision TEXT NOT NULL CHECK(decision IN ('ACCEPT', 'REJECT')),
    CHECK(recovered_energy_j <= energyreq_j),
    CHECK((decision = 'ACCEPT' AND fog_state = 'CLEAR' AND delta_vt <= 0.0
           AND renewable_fraction >= 0.70 AND water_quality_gain >= 0.20
           AND energyreq_j - recovered_energy_j <= 50000.0)
       OR decision = 'REJECT')
) STRICT;

CREATE INDEX IF NOT EXISTS idx_cyboquatic_workload_node_time
ON cyboquatic_workload_frame(node_id, observed_utc DESC);

CREATE INDEX IF NOT EXISTS idx_cyboquatic_workload_canal_decision
ON cyboquatic_workload_frame(canal_node, decision, observed_utc DESC);

CREATE VIEW IF NOT EXISTS v_cyboquatic_workload_corridor AS
SELECT observed_utc, node_id, canal_node, fog_state, energyreq_j, delta_vt,
       k_factor, e_eco_impact, r_residual, decision
FROM cyboquatic_workload_frame
WHERE decision = 'ACCEPT'
  AND k_factor >= 0.60
  AND e_eco_impact >= 0.60
  AND r_residual <= 0.40;
