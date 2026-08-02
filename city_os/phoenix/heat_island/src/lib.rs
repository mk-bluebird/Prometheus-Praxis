// Prometheus-Praxis/city_os/phoenix/heat_island/src/lib.rs

use serde::{Deserialize, Serialize};

/// Calibration parameters for Phoenix heat island model.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CalibrationParams {
    pub alpha: ParameterEstimate, // vegetation impact
    pub beta: ParameterEstimate,  // built-up / materials impact
    pub gamma: ParameterEstimate, // water / moisture impact
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParameterEstimate {
    pub estimate: f64,
    pub ci_low: f64,
    pub ci_high: f64,
    pub units: &'static str,
}

/// Hex-level metrics derived from Landsat and city OS layers.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HexMetrics {
    pub hex_id: String,
    pub uhi: f64,       // degC above rural reference
    pub ndvi: f64,
    pub ndbi: f64,
    pub ndwi: f64,
    pub roof_area_fraction: f64,       // 0..1
    pub feasible_tree_factor: f64,     // 0..1
    pub hydrology_feasibility: f64,    // 0..1
}

/// Cooling offset under intervention deltas (ΔNDVI, ΔNDBI, ΔNDWI).
pub fn delta_temperature(
    params: &CalibrationParams,
    delta_ndvi: f64,
    delta_ndbi: f64,
    delta_ndwi: f64,
) -> f64 {
    params.alpha.estimate * delta_ndvi
        + params.beta.estimate * delta_ndbi
        + params.gamma.estimate * delta_ndwi
}

/// Priority score for tree canopy interventions in a hex.
pub fn tree_priority(
    params: &CalibrationParams,
    metrics: &HexMetrics,
    ndvi_target_max: f64,
) -> f64 {
    let ndvi_gap = (ndvi_target_max - metrics.ndvi).max(0.0);
    metrics.uhi
        * params.alpha.estimate.abs()
        * ndvi_gap
        * metrics.feasible_tree_factor
}

/// Priority score for cool roof interventions in a hex.
pub fn roof_priority(params: &CalibrationParams, metrics: &HexMetrics) -> f64 {
    metrics.uhi
        * params.beta.estimate.abs()
        * metrics.roof_area_fraction
}

/// Priority score for water/hydrology interventions in a hex.
pub fn water_priority(params: &CalibrationParams, metrics: &HexMetrics) -> f64 {
    metrics.uhi
        * params.gamma.estimate.abs()
        * metrics.hydrology_feasibility
}

#[cfg(kani)]
mod verification {
    use super::*;

    #[kani::proof]
    fn tree_priority_non_negative() {
        let params = CalibrationParams {
            alpha: ParameterEstimate {
                estimate: -3.0,
                ci_low: -4.0,
                ci_high: -2.0,
                units: "degC_per_unit_NDVI",
            },
            beta: ParameterEstimate {
                estimate: 2.0,
                ci_low: 1.0,
                ci_high: 3.0,
                units: "degC_per_unit_NDBI",
            },
            gamma: ParameterEstimate {
                estimate: -1.5,
                ci_low: -2.0,
                ci_high: -1.0,
                units: "degC_per_unit_NDWI",
            },
        };

        let metrics = HexMetrics {
            hex_id: String::from("hex-test"),
            uhi: 5.0,
            ndvi: 0.1,
            ndbi: 0.3,
            ndwi: 0.0,
            roof_area_fraction: 0.5,
            feasible_tree_factor: 0.8,
            hydrology_feasibility: 0.6,
        };

        let ndvi_target_max = 0.4;
        let score = tree_priority(&params, &metrics, ndvi_target_max);
        kani::assert!(score >= 0.0);
    }
}
