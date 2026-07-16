// filename: crates/ai_node_shard/src/daily_progress.rs

use crate::{AiDatacenterNode2026v1, AINodeError};
use rusqlite::{params, Connection};
use time::{format_description::well_known::Rfc3339, OffsetDateTime};

/// Initialize the daily_progress_ai_node schema in a SQLite database.[file:91]
pub fn init_daily_progress_ai_node_schema(conn: &Connection) -> Result<(), AINodeError> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS daily_progress_ai_node (
            nodeid                    TEXT    NOT NULL,
            region                    TEXT    NOT NULL,
            lane                      TEXT    NOT NULL,
            steward_uuid              TEXT    NOT NULL,
            steward_signinghex        TEXT    NOT NULL,
            twindow_start             TEXT    NOT NULL,
            twindow_end               TEXT    NOT NULL,

            core_energy_kwh_per_workload REAL NOT NULL CHECK(core_energy_kwh_per_workload >= 0.0),
            joules_per_inference          REAL NOT NULL CHECK(joules_per_inference >= 0.0),
            pue                           REAL NOT NULL CHECK(pue >= 0.0),
            cue_kg_co2_per_kwh            REAL NOT NULL CHECK(cue_kg_co2_per_kwh >= 0.0),
            eco_per_joule                 REAL NOT NULL,
            throughput_tokens_per_s       REAL NOT NULL,
            throughput_inferences_per_s   REAL NOT NULL,
            utilization_pct               REAL NOT NULL CHECK(utilization_pct >= 0.0 AND utilization_pct <= 100.0),
            ere                           REAL NOT NULL,
            eco_task_ratio_pct            REAL NOT NULL,
            wue_l_per_kwh                 REAL NOT NULL CHECK(wue_l_per_kwh >= 0.0),
            embodied_kg_co2eq             REAL NOT NULL CHECK(embodied_kg_co2eq >= 0.0),
            topology_violation_count      INTEGER NOT NULL CHECK(topology_violation_count >= 0),

            r_pue             REAL NOT NULL CHECK(r_pue             >= 0.0 AND r_pue             <= 1.0),
            r_cue             REAL NOT NULL CHECK(r_cue             >= 0.0 AND r_cue             <= 1.0),
            r_eco_per_joule   REAL NOT NULL CHECK(r_eco_per_joule   >= 0.0 AND r_eco_per_joule   <= 1.0),
            r_eco_task_ratio  REAL NOT NULL CHECK(r_eco_task_ratio  >= 0.0 AND r_eco_task_ratio  <= 1.0),
            r_wue             REAL NOT NULL CHECK(r_wue             >= 0.0 AND r_wue             <= 1.0),
            r_embodied        REAL NOT NULL CHECK(r_embodied        >= 0.0 AND r_embodied        <= 1.0),
            r_topology        REAL NOT NULL CHECK(r_topology        >= 0.0 AND r_topology        <= 1.0),
            r_energy          REAL NOT NULL CHECK(r_energy          >= 0.0 AND r_energy          <= 1.0),
            r_joule_inf       REAL NOT NULL CHECK(r_joule_inf       >= 0.0 AND r_joule_inf       <= 1.0),
            r_heat_reuse      REAL NOT NULL CHECK(r_heat_reuse      >= 0.0 AND r_heat_reuse      <= 1.0),
            r_utilization     REAL NOT NULL CHECK(r_utilization     >= 0.0 AND r_utilization     <= 1.0),
            r_bandwidth       REAL NOT NULL CHECK(r_bandwidth       >= 0.0 AND r_bandwidth       <= 1.0),

            vt                REAL NOT NULL CHECK(vt >= 0.0),
            k                 REAL NOT NULL CHECK(k  >= 0.0 AND k  <= 1.0),
            e                 REAL NOT NULL CHECK(e  >= 0.0 AND e  <= 1.0),
            r                 REAL NOT NULL CHECK(r  >= 0.0 AND r  <= 1.0),

            strength_index_s  REAL NOT NULL CHECK(strength_index_s >= 0.0 AND strength_index_s <= 1.0),

            evidencehex       TEXT    NOT NULL,
            prior_evidencehex TEXT,
            phoenix_anchor_id TEXT,
            created_utc       TEXT    NOT NULL,

            PRIMARY KEY (nodeid, twindow_start)
        );

        CREATE INDEX IF NOT EXISTS idx_daily_ai_node_lane_time
            ON daily_progress_ai_node (lane, twindow_start);

        CREATE INDEX IF NOT EXISTS idx_daily_ai_node_steward_time
            ON daily_progress_ai_node (steward_uuid, twindow_start);

        CREATE INDEX IF NOT EXISTS idx_daily_ai_node_evidence
            ON daily_progress_ai_node (evidencehex);
        "#,
    )?;
    Ok(())
}

/// Compute normalized risk coordinates r_j and Lyapunov residual vt
/// for an AiDatacenterNode2026v1, matching AiDatacenterNode2026v1Ker.[file:91]
pub fn compute_risk_and_vt(
    node: &AiDatacenterNode2026v1,
    vt_prev: f64,
) -> (AiDatacenterNode2026v1, f64, f64, f64, f64) {
    // Helper closures for clamped linear maps.
    fn clamp01(x: f64) -> f64 {
        if x < 0.0 {
            0.0
        } else if x > 1.0 {
            1.0
        } else {
            x
        }
    }

    fn r_linear(x: f64, ideal: f64, ceiling: f64) -> f64 {
        if x <= ideal {
            0.0
        } else {
            let num = x - ideal;
            let den = ceiling - ideal;
            if den <= 0.0 {
                1.0
            } else {
                clamp01(num / den)
            }
        }
    }

    // Primary risk coordinates (mirroring ALN particle).[file:91]
    let r_pue = r_linear(node.pue, 1.05, 1.40);
    let r_cue = r_linear(node.cue_kg_co2_per_kwh, 0.05, 0.40);
    let r_wue = r_linear(node.wue_l_per_kwh, 0.20, 1.50);
    let r_embodied = clamp01(node.embodied_kg_co2eq / 100.0);

    // Eco-per-joule: lower than 1.0 is worse, invert.[file:91]
    let r_eco_per_joule = if node.eco_per_joule >= 1.0 {
        0.0
    } else {
        clamp01((1.0 - node.eco_per_joule) / 1.0)
    };

    // Eco-task ratio: lower than 50% is worse.[file:91]
    let r_eco_task_ratio = if node.eco_task_ratio_pct >= 50.0 {
        0.0
    } else {
        clamp01((50.0 - node.eco_task_ratio_pct) / (50.0 - 20.0))
    };

    // Energy and joules per inference.[file:91]
    let r_energy =
        r_linear(node.core_energy_kwh_per_workload, 0.10, 0.50);
    let r_joule_inf =
        r_linear(node.joules_per_inference, 1.0, 10.0);

    // Heat reuse: low ERE is worse.[file:91]
    let r_heat_reuse = if node.ere >= 0.5 {
        0.0
    } else {
        clamp01((0.5 - node.ere) / 0.5)
    };

    // Utilization: under-utilization is a soft risk.[file:91]
    let r_utilization = if node.utilization_pct >= 50.0 {
        0.0
    } else {
        clamp01((50.0 - node.utilization_pct) / 50.0)
    };

    // Bandwidth: placeholder (guidance only for now).[file:91]
    let r_bandwidth = 0.0;

    // Topology: violation count normalized to [0,1].[file:91]
    let r_topology = clamp01(node.topology_violation_count as f64 / 5.0);

    // Lyapunov residual vt with weighted quadratic sum.[file:91][file:92]
    let vt = 0.20 * r_cue.powi(2)
        + 0.20 * r_eco_per_joule.powi(2)
        + 0.15 * r_eco_task_ratio.powi(2)
        + 0.15 * r_wue.powi(2)
        + 0.10 * r_pue.powi(2)
        + 0.05 * r_energy.powi(2)
        + 0.05 * r_joule_inf.powi(2)
        + 0.05 * r_embodied.powi(2)
        + 0.03 * r_heat_reuse.powi(2)
        + 0.02 * r_topology.powi(2);

    let vt_prev_clamped = if vt_prev < 0.0 { 0.0 } else { vt_prev };
    let delta_vt = vt - vt_prev_clamped;

    // KER derivation (mirroring AiDatacenterNode2026v1Ker).[file:91][file:92]
    let k_raw = 0.95 - 0.30 * vt - 0.10 * delta_vt.max(0.0);
    let e_raw = 0.93 - 0.25 * vt;
    let r_raw = 0.12 + 0.40 * vt + 0.20 * delta_vt.max(0.0);

    let clamp_ker = |x: f64| {
        if x < 0.0 {
            0.0
        } else if x > 1.0 {
            1.0
        } else {
            x
        }
    };

    let k = clamp_ker(k_raw);
    let e = clamp_ker(e_raw);
    let r = clamp_ker(r_raw);

    // Strength index S: concave eco-strength index.[file:91][file:92]
    let b_raw = node.eco_per_joule * (node.eco_task_ratio_pct / 100.0);
    let b_norm = if b_raw >= 1.0 {
        1.0
    } else if b_raw <= 0.0 {
        0.0
    } else {
        b_raw
    };

    let g_r = if r >= 0.20 {
        0.0
    } else {
        let v = 1.0 - r / 0.20;
        if v < 0.0 { 0.0 } else { v }
    };

    let strength_index_s = g_r * b_norm.sqrt();

    let mut updated = node.clone();
    updated.r_pue = r_pue;
    updated.r_cue = r_cue;
    updated.r_eco_per_joule = r_eco_per_joule;
    updated.r_eco_task_ratio = r_eco_task_ratio;
    updated.r_wue = r_wue;
    updated.r_embodied = r_embodied;
    updated.r_topology = r_topology;
    updated.r_energy = r_energy;
    updated.r_joule_inf = r_joule_inf;
    updated.r_heat_reuse = r_heat_reuse;
    updated.r_utilization = r_utilization;
    updated.r_bandwidth = r_bandwidth;
    updated.vt = vt;
    updated.k = k;
    updated.e = e;
    updated.r = r;
    updated.strength_index_s = strength_index_s;

    (updated, vt, k, e, r)
}

/// Insert a daily AiDatacenterNode2026v1 record into daily_progress_ai_node,
/// computing r_j, vt, KER, and strength index, and attaching evidencehex.[file:91]
pub fn insert_daily_progress_ai_node(
    conn: &Connection,
    mut node: AiDatacenterNode2026v1,
    vt_prev: f64,
    prior_evidencehex: Option<String>,
    phoenix_anchor_id: Option<String>,
    evidencehex: String,
) -> Result<(), AINodeError> {
    // Compute risk coordinates and KER.
    let (node_with_risk, vt, k, e, r) = compute_risk_and_vt(&node, vt_prev);

    // Update node fields.
    node.r_pue = node_with_risk.r_pue;
    node.r_cue = node_with_risk.r_cue;
    node.r_eco_per_joule = node_with_risk.r_eco_per_joule;
    node.r_eco_task_ratio = node_with_risk.r_eco_task_ratio;
    node.r_wue = node_with_risk.r_wue;
    node.r_embodied = node_with_risk.r_embodied;
    node.r_topology = node_with_risk.r_topology;
    node.r_energy = node_with_risk.r_energy;
    node.r_joule_inf = node_with_risk.r_joule_inf;
    node.r_heat_reuse = node_with_risk.r_heat_reuse;
    node.r_utilization = node_with_risk.r_utilization;
    node.r_bandwidth = node_with_risk.r_bandwidth;
    node.vt = vt;
    node.k = k;
    node.e = e;
    node.r = r;
    node.strength_index_s = node_with_risk.strength_index_s;
    node.evidencehex = evidencehex.clone();

    // Basic sanity checks using existing validator.[file:91]
    node.validate()?;

    let now = OffsetDateTime::now_utc();
    let created_utc = now.format(&Rfc3339)?;

    conn.execute(
        r#"
        INSERT INTO daily_progress_ai_node (
            nodeid,
            region,
            lane,
            steward_uuid,
            steward_signinghex,
            twindow_start,
            twindow_end,

            core_energy_kwh_per_workload,
            joules_per_inference,
            pue,
            cue_kg_co2_per_kwh,
            eco_per_joule,
            throughput_tokens_per_s,
            throughput_inferences_per_s,
            utilization_pct,
            ere,
            eco_task_ratio_pct,
            wue_l_per_kwh,
            embodied_kg_co2eq,
            topology_violation_count,

            r_pue,
            r_cue,
            r_eco_per_joule,
            r_eco_task_ratio,
            r_wue,
            r_embodied,
            r_topology,
            r_energy,
            r_joule_inf,
            r_heat_reuse,
            r_utilization,
            r_bandwidth,

            vt,
            k,
            e,
            r,
            strength_index_s,

            evidencehex,
            prior_evidencehex,
            phoenix_anchor_id,
            created_utc
        ) VALUES (
            ?1, ?2, ?3, ?4, ?5, ?6, ?7,
            ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20,
            ?21,
            ?22, ?23, ?24, ?25, ?26, ?27, ?28, ?29, ?30, ?31, ?32, ?33,
            ?34, ?35, ?36, ?37, ?38,
            ?39, ?40, ?41, ?42
        );
        "#,
        params![
            node.nodeid,
            node.region,
            node.lane,
            node.steward.steward_uuid,
            node.signinghex,
            node.twindow_start,
            node.twindow_end,
            node.core_energy_kwh_per_workload,
            node.joules_per_inference,
            node.pue,
            node.cue_kg_co2_per_kwh,
            node.eco_per_joule,
            node.throughput_tokens_per_s,
            node.throughput_inferences_per_s,
            node.utilization_pct,
            node.ere,
            node.eco_task_ratio_pct,
            node.wue_l_per_kwh,
            node.embodied_kg_co2eq,
            node.topology_violation_count,
            node.r_pue,
            node.r_cue,
            node.r_eco_per_joule,
            node.r_eco_task_ratio,
            node.r_wue,
            node.r_embodied,
            node.r_topology,
            node.r_energy,
            node.r_joule_inf,
            node.r_heat_reuse,
            node.r_utilization,
            node.r_bandwidth,
            node.vt,
            node.k,
            node.e,
            node.r,
            node.strength_index_s,
            node.evidencehex,
            prior_evidencehex,
            phoenix_anchor_id,
            created_utc,
        ],
    )?;

    Ok(())
}
