-- filename db/db_econet_constellation_blastradius_mt6883.sql
-- destination eco_restoration_shard/db/db_econet_constellation_blastradius_mt6883.sql
-- repo-target github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. blast_radius_zones: node-scoped physical and digital limits
--    - Tied to existing node(nodeid, region, nodetype, ...)
--    - Hard caps for radius and propagation, non-actuating, research band
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blast_radius_zones (
    zone_id TEXT PRIMARY KEY,
    nodeid TEXT NOT NULL,
    max_physical_radius_meters REAL NOT NULL CHECK (max_physical_radius_meters <= 150.0),
    max_thermal_propagation_kelvin REAL NOT NULL CHECK (max_thermal_propagation_kelvin <= 0.30),
    max_acoustic_decibels REAL NOT NULL CHECK (max_acoustic_decibels <= 110.0),
    network_hop_containment INTEGER NOT NULL CHECK (network_hop_containment BETWEEN 1 AND 3),
    active_remedy_protocol TEXT NOT NULL CHECK (
        active_remedy_protocol IN (
            'ISOLATE_ACTUATOR',
            'DYNAMIC_THROTTLE',
            'FAIL_SAFE_SHUTDOWN'
        )
    ),
    region TEXT NOT NULL,
    createdutc TEXT NOT NULL,
    updatedutc TEXT NOT NULL,
    FOREIGN KEY (nodeid) REFERENCES node(nodeid) ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_blast_radius_zones_node
    ON blast_radius_zones (nodeid, region);

----------------------------------------------------------------------
-- 2. blast_radius_propagation: per-medium propagation invariants
--    - Linked to blast_radius_zones via zone_id
--    - Attenuation/safety floors normalized to 0..1 KER-friendly ranges
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS blast_radius_propagation (
    propagation_id INTEGER PRIMARY KEY AUTOINCREMENT,
    zone_id TEXT NOT NULL,
    medium TEXT NOT NULL CHECK (
        medium IN ('Acoustic', 'Thermal', 'Hydrodynamic', 'Electromagnetic')
    ),
    max_permitted_attenuation REAL NOT NULL CHECK (
        max_permitted_attenuation >= 0.0 AND max_permitted_attenuation <= 1.0
    ),
    environmental_safety_floor REAL NOT NULL CHECK (
        environmental_safety_floor >= 0.0 AND environmental_safety_floor <= 1.0
    ),
    monitoring_frequency_hz REAL NOT NULL CHECK (monitoring_frequency_hz >= 10.0),
    createdutc TEXT NOT NULL,
    updatedutc TEXT NOT NULL,
    FOREIGN KEY (zone_id) REFERENCES blast_radius_zones(zone_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_blast_radius_propagation_zone_medium
    ON blast_radius_propagation (zone_id, medium);

----------------------------------------------------------------------
-- 3. cyber_physical_routing_table: IP-table-like routing overlay
--    - Source/destination bound to node(nodeid)
--    - Non-actuating: used by research and orchestration layers
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS cyber_physical_routing_table (
    route_id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_nodeid TEXT NOT NULL,
    destination_nodeid TEXT NOT NULL,
    allocated_bandwidth_mbps INTEGER NOT NULL CHECK (allocated_bandwidth_mbps >= 1),
    allowed_protocol TEXT NOT NULL CHECK (
        allowed_protocol IN ('GRPC_SECURE', 'WEBS_DID', 'ALN_SHARD_SYNC')
    ),
    routing_status TEXT NOT NULL CHECK (
        routing_status IN ('ACTIVE_ROUTED', 'ISOLATED_BY_BLAST_LIMIT')
    ),
    region TEXT NOT NULL,
    createdutc TEXT NOT NULL,
    updatedutc TEXT NOT NULL,
    FOREIGN KEY (source_nodeid) REFERENCES node(nodeid) ON DELETE RESTRICT,
    FOREIGN KEY (destination_nodeid) REFERENCES node(nodeid) ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_cyber_physical_routing_src_dst
    ON cyber_physical_routing_table (source_nodeid, destination_nodeid, routing_status);

----------------------------------------------------------------------
-- 4. force_contribution_ledger: sovereign forced-contribution log
--    - Non-actuating ledger of forced-binding events
--    - RISK_chain trigger will log the containment action
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS force_contribution_ledger (
    force_id INTEGER PRIMARY KEY AUTOINCREMENT,
    violator_did TEXT NOT NULL,
    unauthorized_data_signature TEXT NOT NULL,
    bound_workload_type TEXT NOT NULL,    -- e.g. 'WAVE_MODEL', 'BIO_FILTER', 'CARBON_SIM'
    region TEXT NOT NULL,
    createdutc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_force_contribution_violator
    ON force_contribution_ledger (violator_did, region, createdutc);

----------------------------------------------------------------------
-- 5. RISK_chain: MT6883 risk chain audit log (append-only)
--    - Immutable ledger for neurorights-related MT6883 operations
--    - unauthorized_mutation_attempt forced to 0 at insert
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS RISK_chain (
    risk_id INTEGER PRIMARY KEY AUTOINCREMENT,
    node_did TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    unauthorized_mutation_attempt INTEGER NOT NULL CHECK (unauthorized_mutation_attempt = 0),
    risk_evidence_bundle TEXT NOT NULL,
    createdutc TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (node_did) REFERENCES node(nodeid) ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_RISK_chain_node_time
    ON RISK_chain (node_did, timestamp);

----------------------------------------------------------------------
-- 6. RISK_event: MT6883 risk event log (append-only)
--    - Per-risk_id events: invariant checks, operational actions
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS RISK_event (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    risk_id INTEGER NOT NULL,
    invariant_violation_detected INTEGER NOT NULL CHECK (invariant_violation_detected IN (0, 1)),
    operational_action_taken TEXT NOT NULL,
    createdutc TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (risk_id) REFERENCES RISK_chain(risk_id) ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_RISK_event_risk
    ON RISK_event (risk_id, invariant_violation_detected);

----------------------------------------------------------------------
-- 7. Strict invariant enforcement for RISK_chain / RISK_event
--    - Forbid UPDATE and DELETE to preserve immutability
----------------------------------------------------------------------

CREATE TRIGGER IF NOT EXISTS forbid_RISK_chain_update
BEFORE UPDATE ON RISK_chain
BEGIN
    SELECT RAISE(FAIL, 'CRITICAL SECURITY ERROR: UPDATE operations on RISK_chain are forbidden.');
END;

CREATE TRIGGER IF NOT EXISTS forbid_RISK_chain_delete
BEFORE DELETE ON RISK_chain
BEGIN
    SELECT RAISE(FAIL, 'CRITICAL SECURITY ERROR: DELETE operations on RISK_chain are forbidden.');
END;

CREATE TRIGGER IF NOT EXISTS forbid_RISK_event_update
BEFORE UPDATE ON RISK_event
BEGIN
    SELECT RAISE(FAIL, 'CRITICAL SECURITY ERROR: UPDATE operations on RISK_event are forbidden.');
END;

CREATE TRIGGER IF NOT EXISTS forbid_RISK_event_delete
BEFORE DELETE ON RISK_event
BEGIN
    SELECT RAISE(FAIL, 'CRITICAL SECURITY ERROR: DELETE operations on RISK_event are forbidden.');
END;

----------------------------------------------------------------------
-- 8. Trigger: auto_bind_violating_resources
--    - On new force_contribution_ledger row:
--      * Append an MT6883-safe RISK_chain record
--    - Does not alter actuation or routing; ledger only
----------------------------------------------------------------------

CREATE TRIGGER IF NOT EXISTS auto_bind_violating_resources
AFTER INSERT ON force_contribution_ledger
BEGIN
    INSERT INTO RISK_chain (node_did, unauthorized_mutation_attempt, risk_evidence_bundle)
    VALUES (
        NEW.violator_did,
        0,
        'CRITICAL ALERT: Unauthorized data access detected. Force-binding engaged for signature: '
            || NEW.unauthorized_data_signature
    );
END;

----------------------------------------------------------------------
-- 9. View: v_blast_radius_route_guard
--    - Derived guard surface for routing decisions
--    - Joins:
--        node          : physical node catalog
--        blast_radius_zones / blast_radius_propagation
--        blastradiuslink (if present in this DB)
--        cyboworkloadledger (if present) for KER corridor status
--    - Non-actuating: read-only view for Rust/Lua/Kotlin clients
----------------------------------------------------------------------

-- NOTE:
--  - blastradiuslink: evidence-anchored blast radius links
--  - cyboworkloadledger: KER / Lyapunov workload history
--  - Both are optional; LEFT JOIN keeps view resilient if absent,
--    while still providing value where present.

CREATE VIEW IF NOT EXISTS v_blast_radius_route_guard AS
SELECT
    n.nodeid,
    n.region,
    z.zone_id,
    z.max_physical_radius_meters,
    z.max_thermal_propagation_kelvin,
    z.max_acoustic_decibels,
    z.network_hop_containment,
    z.active_remedy_protocol,
    p.medium,
    p.max_permitted_attenuation,
    p.environmental_safety_floor,
    p.monitoring_frequency_hz,
    -- Aggregated impact envelope, if blastradiuslink present
    MIN(l.radiusmeters)    AS min_radius_m,
    MAX(l.radiusmeters)    AS max_radius_m,
    MIN(l.radiushours)     AS min_radius_h,
    MAX(l.radiushours)     AS max_radius_h,
    -- Simple corridor health flags derived from cyboworkloadledger, if present
    AVG(w.rscore)          AS mean_risk_R,
    AVG(w.kscore)          AS mean_knowledge_K,
    AVG(w.escore)          AS mean_energy_E
FROM
    node AS n
    JOIN blast_radius_zones AS z
        ON z.nodeid = n.nodeid
    LEFT JOIN blast_radius_propagation AS p
        ON p.zone_id = z.zone_id
    LEFT JOIN blastradiuslink AS l
        ON l.sourcekind = 'NODE'
       AND l.sourceid   = n.nodeid
    LEFT JOIN cyboworkloadledger AS w
        ON w.nodeid = n.nodeid
GROUP BY
    n.nodeid,
    n.region,
    z.zone_id,
    z.max_physical_radius_meters,
    z.max_thermal_propagation_kelvin,
    z.max_acoustic_decibels,
    z.network_hop_containment,
    z.active_remedy_protocol,
    p.medium,
    p.max_permitted_attenuation,
    p.environmental_safety_floor,
    p.monitoring_frequency_hz;

----------------------------------------------------------------------
-- 10. View: v_cyber_physical_routing_effective
--     - Joins routing table with v_blast_radius_route_guard
--     - Adds a derived "blast_safe" boolean for tooling
--     - Non-actuating: ENGINE repos make their own decisions
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_cyber_physical_routing_effective AS
SELECT
    r.route_id,
    r.source_nodeid,
    r.destination_nodeid,
    r.allocated_bandwidth_mbps,
    r.allowed_protocol,
    r.routing_status,
    r.region,
    r.createdutc,
    r.updatedutc,
    g.max_physical_radius_meters,
    g.max_thermal_propagation_kelvin,
    g.max_acoustic_decibels,
    g.network_hop_containment,
    g.medium,
    g.max_permitted_attenuation,
    g.environmental_safety_floor,
    g.min_radius_m,
    g.max_radius_m,
    g.min_radius_h,
    g.max_radius_h,
    g.mean_risk_R,
    g.mean_knowledge_K,
    g.mean_energy_E,
    CASE
        WHEN g.max_radius_m IS NOT NULL
         AND g.max_radius_m <= g.max_physical_radius_meters
         AND (g.mean_risk_R IS NULL OR g.mean_risk_R <= 0.13)
        THEN 1
        ELSE 0
    END AS blast_safe
FROM
    cyber_physical_routing_table AS r
    LEFT JOIN v_blast_radius_route_guard AS g
        ON g.nodeid = r.source_nodeid
       AND g.region = r.region;
