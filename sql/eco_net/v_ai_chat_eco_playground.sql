-- File: sql/eco_net/v_ai_chat_eco_playground.sql

PRAGMA foreign_keys = ON;

-- View: v_ai_chat_eco_playground
-- Human-readable eco-restoration playground for AI-chat agents.
-- Surfaces hex, canal, PFAS, and KER summaries in a single row per (hex_id, canal_node).[59][78]

CREATE VIEW IF NOT EXISTS v_ai_chat_eco_playground AS
SELECT
    -- Identity
    h.hex_id                                AS hex_id,
    h.domain                                AS hex_domain,
    h.subdomain                             AS hex_subdomain,
    h.owner_did                             AS hex_owner_did,
    cn.node_code                            AS canal_node_code,
    cn.description                          AS canal_node_description,
    cn.canal_plane                          AS canal_plane,
    cn.ker_band                             AS canal_ker_band,
    cn.fog_band                             AS canal_fog_band,

    -- Hex risk coordinates and Lyapunov residual
    h.r_hydraulics                          AS hex_r_hydraulics,
    h.r_energy                              AS hex_r_energy,
    h.r_topology                            AS hex_r_topology,
    h.r_biodiversity                        AS hex_r_biodiversity,
    (h.w_h * h.r_hydraulics  * h.r_hydraulics +
     h.w_e * h.r_energy      * h.r_energy +
     h.w_t * h.r_topology    * h.r_topology +
     h.w_b * h.r_biodiversity* h.r_biodiversity) AS hex_Vt,

    -- Canal workload telemetry aggregates (if available)
    v.avg_deltaVt                           AS canal_avg_deltaVt,
    v.avg_energy_input_J                    AS canal_avg_energy_input_J,
    v.avg_pfas_ugL                          AS canal_avg_pfas_ugL,
    v.min_ker_score                         AS canal_min_ker_score,

    -- Latest PFAS corridor state per canal node (if available)
    ps.mass_kg                              AS pfas_mass_kg,
    ps.sorbed_fraction                      AS pfas_sorbed_fraction,
    ps.cold_survival_factor                 AS pfas_cold_survival_factor,

    -- Friendly corridor health flags
    CASE
        WHEN v.min_ker_score IS NOT NULL AND v.min_ker_score > 0.0 THEN 'SAFE'
        WHEN v.min_ker_score IS NOT NULL AND v.min_ker_score <= 0.0 THEN 'RISK'
        ELSE 'UNKNOWN'
    END                                     AS ker_corridor_health,

    CASE
        WHEN ps.mass_kg IS NOT NULL AND ps.cold_survival_factor >= 1.0 THEN 'PFAS_COLD_SURVIVAL_MONITOR'
        WHEN ps.mass_kg IS NOT NULL THEN 'PFAS_CORRIDOR_ACTIVE'
        ELSE 'PFAS_UNKNOWN'
    END                                     AS pfas_corridor_health

FROM phoenix_hex_registry h
LEFT JOIN canal_node cn
    ON cn.canal_plane = 'HYDRAULICS'
LEFT JOIN v_cyboquatic_workload_ker_summary v
    ON v.node_code = cn.node_code
LEFT JOIN pfas_corridor_state ps
    ON ps.node_id = cn.node_id;
