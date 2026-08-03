-- File: sql/views/v_synapse_neuro_corridor.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_synapse_neuro_corridor.sql

CREATE VIEW IF NOT EXISTS v_synapse_neuro_corridor AS
SELECT
    s.synapse_id,
    s.producer_lang,
    s.producer_relpath,
    s.consumer_lang,
    s.consumer_relpath,
    s.synapse_class,
    s.transport_kind,
    s.lane_default,
    s.non_actuating,
    s.allows_actuation,
    s.neuro_flag,
    s.ker_k,
    s.ker_e,
    s.ker_r,
    s.ker_s
FROM synapse_endpoint AS s
WHERE
    s.neuro_flag = 1
ORDER BY
    s.lane_default,
    s.synapse_class,
    s.producer_relpath;
