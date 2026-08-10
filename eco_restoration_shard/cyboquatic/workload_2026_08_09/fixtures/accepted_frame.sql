-- File: eco_restoration_shard/cyboquatic/workload_2026_08_09/fixtures/accepted_frame.sql
-- Fixture: one valid, accepted frame with all new telemetry columns populated.
-- This file is runnable after the schema using SQLite's .read command.
-- It does not create a database or modify schema.

INSERT INTO cyboquatic_workload_frame (
    observed_utc,
    node_id,
    owner_did,
    energyreq_j,
    delta_vt,
    knowledge_factor,
    eco_impact_value,
    ker_k,
    ker_e,
    ker_r,
    fog_confidence,
    canal_node,
    accepted,
    flow_m3_s,
    lift_m,
    efficiency,
    runtime_s,
    voltage_drop_v,
    renewable_fraction,
    embodied_carbon_g_per_j,
    biodiversity_risk
) VALUES (
    '2026-08-10T12:00:00Z',
    'phoenix-canal-pump-01',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    1549.335191,
    0.150000,
    0.960000,
    0.820000,
    0.95,
    0.90,
    0.80,
    0.85,
    'phoenix-canal-north',
    1,
    0.035,
    4.2,
    0.78,
    900.0,
    2.1,
    0.82,
    0.000035,
    0.08
);
