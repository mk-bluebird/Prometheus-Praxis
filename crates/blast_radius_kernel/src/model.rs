// filename: model.rs
// destination: ecorestoration_shard/blast_radius_kernel/src/model.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LambdaSummary {
    pub segment_id: i64,
    pub region_code: String,
    pub contaminant_code: String,
    pub substrate_kind: String,
    pub k_eff_per_day: f64,
    pub v_mean_m_per_s: f64,
    pub eco_weight_applied: f64,
    pub lambda_eff_per_m: f64,
    pub lambda_eff_min_per_m: f64,
    pub lambda_eff_max_per_m: f64,
    pub telemetry_span_s: i64,
    pub t_snapshot_start_utc: String,
    pub t_snapshot_end_utc: String,
}

#[derive(Debug, Clone)]
pub struct LambdaQuery {
    pub segment_id: i64,
    pub region_code: String,
    pub contaminant_code: String,
    pub season_code: String,
    pub temp_celsius: f64,
}
