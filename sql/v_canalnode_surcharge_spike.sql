-- filename: sql/v_canalnode_surcharge_spike.sql

CREATE VIEW IF NOT EXISTS v_canalnode_surcharge_spike AS
SELECT
    c.node_id,
    c.window_start_utc,
    c.window_end_utc,
    c.posterior_spike_prob,
    CASE
        WHEN c.posterior_spike_prob >= 0.9 THEN 1
        ELSE 0
    END AS surcharge_spike_flag
FROM canalnode_tox_changepoint AS c;
