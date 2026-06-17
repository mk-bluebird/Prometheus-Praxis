-- filename db_node_ker_trust_view.sql
-- destination eco_restoration_shard/sql/views/db_node_ker_trust_view.sql
PRAGMA foreign_keys = ON;

CREATE VIEW IF NOT EXISTS v_node_ker_trust AS
SELECT
    p.nodeid,
    p.region,
    p.medium,
    p.twindowend AS ts,
    p.braw,
    p.rraw,
    p.dt,
    p.ki,
    p.ti,
    p.badj,
    p.safetyphase,
    p.securityresponsecap
FROM planning_safety_security AS p;
