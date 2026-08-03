-- File: sql/views/v_synapse_safe_for_eco.sql
-- Destination: mk-bluebird/Prometheus-Praxis/sql/views/v_synapse_safe_for_eco.sql

CREATE VIEW IF NOT EXISTS v_synapse_safe_for_eco AS
SELECT
    s.synapse_id,
    s.producer_lang,
    s.producer_relpath,
    s.consumer_lang,
    s.consumer_relpath,
    s.synapse_class,
    s.transport_kind,
    s.lane_default,
    s.primary_plane,
    s.non_actuating,
    s.allows_readonly,
    s.allows_actuation,
    s.neuro_flag,
    s.ker_k,
    s.ker_e,
    s.ker_r,
    s.ker_s
FROM synapse_endpoint AS s
WHERE
    -- Safe analytics bridges: non-actuating, readonly, no actuation allowed,
    -- and basic KER thresholds met.
    s.synapse_class = 'ANALYTIC_BRIDGE'
    AND s.non_actuating = 1
    AND s.allows_readonly = 1
    AND s.allows_actuation = 0
    AND s.ker_s > 0.0
ORDER BY
    s.primary_plane,
    s.producer_relpath,
    s.consumer_relpath;
