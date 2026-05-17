// filename: ecorestorationshard/ecowealth/src/model.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoUnit {
    pub steward_id: String,
    pub region_code: String,
    pub window_start_utc: String,
    pub window_end_utc: String,

    // Aggregated KER over window (typically long window)
    pub k_window: f64,
    pub e_window: f64,
    pub r_window: f64,
    pub vt_trend: f64,         // V_t(end) - V_t(start)

    // Base EcoWealth kernel parameters (from EcoWealthKernel2026v1)
    pub alpha: f64,
    pub beta:  f64,
    pub gamma: f64,

    // Regional scaling from CEIM benefit kernels
    pub s_region: f64,         // kg CO2e avoided per unit, etc.

    // Topology / representation bonus or penalty
    pub b_s: f64,              // B_s in EcoUnit = S_region * W_s + B_s

    // Trust discounts
    pub trust_discount_data:  f64, // e.g. from rcalib, rsigma
    pub trust_discount_topology: f64, // from rtopology
    pub trust_discount_total: f64,

    // Raw and final values
    pub w_s_raw: f64,          // E^alpha * K^beta * (1 - R)^gamma
    pub eco_unit_raw: f64,     // S_region * w_s_raw + B_s
    pub eco_unit_final: f64,   // eco_unit_raw * (1 - trust_discount_total)

    // Provenance
    pub kernel_id: String,     // EcoWealthKernel2026v1 id
    pub plane_contract_id: String, // EcosafetyPlaneWeights2026v1
    pub corridor_set_id: String,
}
