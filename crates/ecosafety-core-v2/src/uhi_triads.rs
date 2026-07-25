// filename: crates/ecosafety-core-v2/src/uhi_triads.rs
// destination: https://github.com/mk-bluebird/Prometheus-Praxis/crates/ecosafety-core-v2/src/uhi_triads.rs
// edition: 2024
// rust-version = "1.85"
// license: MIT OR Apache-2.0

#![forbid(unsafe_code)]

// Phoenix Urban Heat Island (UHI) triad builder.
// Non-actuating: maps physical UHI observables (temperature, vegetation, air quality)
// into normalized risk coordinates r_T, r_C, r_A and a composite thermal risk scalar.

use crate::{EcosafetyError, RiskVector, Scalar};

use serde::{Deserialize, Serialize};

/// Raw UHI observables for a Phoenix hex or census tract.
/// These fields are chosen to align with published Phoenix heat models:
/// - Surface temperature range over hot season.[web:38][web:41]
/// - Vegetation indices (e.g., NDVI, canopy fraction).[web:38][web:44][web:46]
/// - Air-quality metrics (e.g., PM2.5, ozone or a composite AQI).[web:44]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UhiObservables {
    /// Daily or seasonal mean surface temperature [°C] or [°F converted].
    pub surface_temp_c: Scalar,
    /// Normalized Difference Vegetation Index (NDVI) in [-1,1] but typically [0,1] in Phoenix.[web:44][web:38]
    pub ndvi: Scalar,
    /// Tree canopy fraction (0..1) for the hex (optional, can refine vegetation risk).[web:39][web:46]
    pub canopy_fraction: Option<Scalar>,
    /// Air-quality index (AQI-like scalar) where higher means worse (e.g., PM2.5 / ozone composite).
    pub air_quality_index: Scalar,
}

/// Normalization bands for UHI risk coordinates.
/// These ranges are policy/analysis parameters, not hard-coded physics;
/// they can be tuned per timeframe and scenario.[web:35][web:40][web:39]
#[derive(Debug, Clone)]
pub struct UhiBands {
    /// Temperature band [T_min_safe, T_max_extreme] in °C for normalization.
    pub t_min_safe_c: Scalar,
    pub t_max_extreme_c: Scalar,
    /// NDVI band [NDVI_min, NDVI_optimal].
    pub ndvi_min: Scalar,
    pub ndvi_optimal: Scalar,
    /// Canopy fraction band [C_min, C_target] (0..1).
    pub canopy_min: Scalar,
    pub canopy_target: Scalar,
    /// AQI band [AQI_good, AQI_hazardous] for normalization.
    pub aqi_good: Scalar,
    pub aqi_hazardous: Scalar,
    /// Weights for collapsing r_T, r_C, r_A into a single r_thermal.
    pub w_temp: Scalar,
    pub w_veg: Scalar,
    pub w_air: Scalar,
}

impl UhiBands {
    /// Basic sanity check for bands and weights (non-negative, ordered).
    pub fn validate(&self) -> Result<(), EcosafetyError> {
        if self.t_max_extreme_c <= self.t_min_safe_c {
            return Err(EcosafetyError::StabilityStep(
                "t_max_extreme_c must be > t_min_safe_c".to_string(),
            ));
        }
        if self.ndvi_optimal <= self.ndvi_min {
            return Err(EcosafetyError::StabilityStep(
                "ndvi_optimal must be > ndvi_min".to_string(),
            ));
        }
        if self.canopy_target <= self.canopy_min {
            return Err(EcosafetyError::StabilityStep(
                "canopy_target must be > canopy_min".to_string(),
            ));
        }
        if self.aqi_hazardous <= self.aqi_good {
            return Err(EcosafetyError::StabilityStep(
                "aqi_hazardous must be > aqi_good".to_string(),
            ));
        }
        if self.w_temp < 0.0 || self.w_veg < 0.0 || self.w_air < 0.0 {
            return Err(EcosafetyError::NegativeWeight(
                "UHI weights must be non-negative".to_string(),
            ));
        }
        Ok(())
    }
}

/// Normalized UHI triad and composite thermal risk.
/// All r_.* fields are in [0,1], where 0 is best (cool/clean/green) and 1 is worst.[web:35][web:40][web:44]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UhiTriadRisk {
    /// Temperature risk coordinate r_T.
    pub r_t: Scalar,
    /// Vegetation risk coordinate r_C (low vegetation → high risk).
    pub r_c: Scalar,
    /// Air-quality risk coordinate r_A.
    pub r_a: Scalar,
    /// Composite thermal risk scalar r_thermal in [0,1].
    pub r_thermal: Scalar,
}

/// Helper: clamp a scalar to [0,1].
fn clamp01(x: Scalar) -> Scalar {
    if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

/// Compute r_T from surface temperature, using a linear ramp between
/// t_min_safe and t_max_extreme.[web:35][web:39]
fn compute_r_t(surface_temp_c: Scalar, bands: &UhiBands) -> Scalar {
    let t = surface_temp_c;
    let num = t - bands.t_min_safe_c;
    let den = bands.t_max_extreme_c - bands.t_min_safe_c;
    let raw = if den > 0.0 { num / den } else { 1.0 };
    clamp01(raw)
}

/// Compute vegetation risk r_C from NDVI and canopy fraction.
/// High NDVI and canopy → low risk; low NDVI and sparse canopy → high risk.[web:38][web:44][web:46]
fn compute_r_c(ndvi: Scalar, canopy_fraction: Option<Scalar>, bands: &UhiBands) -> Scalar {
    // NDVI component: map [ndvi_min, ndvi_optimal] to [1,0].
    let n = ndvi;
    let num_ndvi = bands.ndvi_optimal - n;
    let den_ndvi = bands.ndvi_optimal - bands.ndvi_min;
    let raw_ndvi = if den_ndvi > 0.0 {
        num_ndvi / den_ndvi
    } else {
        1.0
    };
    let ndvi_risk = clamp01(raw_ndvi);

    // Canopy component: map [canopy_min, canopy_target] to [1,0].
    let canopy = canopy_fraction.unwrap_or(bands.canopy_min);
    let num_canopy = bands.canopy_target - canopy;
    let den_canopy = bands.canopy_target - bands.canopy_min;
    let raw_canopy = if den_canopy > 0.0 {
        num_canopy / den_canopy
    } else {
        1.0
    };
    let canopy_risk = clamp01(raw_canopy);

    // Simple average of NDVI and canopy risk contributions.
    clamp01((ndvi_risk + canopy_risk) * 0.5)
}

/// Compute air-quality risk r_A from an AQI-like index.
/// Map [aqi_good, aqi_hazardous] to [0,1].[web:44]
fn compute_r_a(aqi: Scalar, bands: &UhiBands) -> Scalar {
    let num = aqi - bands.aqi_good;
    let den = bands.aqi_hazardous - bands.aqi_good;
    let raw = if den > 0.0 { num / den } else { 1.0 };
    clamp01(raw)
}

/// Build the UHI triad risk and composite thermal scalar for a single location.[web:35][web:40][web:44]
pub fn build_uhi_triad(observed: &UhiObservables, bands: &UhiBands) -> Result<UhiTriadRisk, EcosafetyError> {
    bands.validate()?;

    let r_t = compute_r_t(observed.surface_temp_c, bands);
    let r_c = compute_r_c(observed.ndvi, observed.canopy_fraction, bands);
    let r_a = compute_r_a(observed.air_quality_index, bands);

    // Weighted sum with normalization; weights are advisory and tuned per policy.[web:35][web:40]
    let w_sum = bands.w_temp + bands.w_veg + bands.w_air;
    let r_thermal = if w_sum > 0.0 {
        clamp01(
            (bands.w_temp * r_t + bands.w_veg * r_c + bands.w_air * r_a) / w_sum,
        )
    } else {
        // If weights are all zero, fall back to simple average.
        clamp01((r_t + r_c + r_a) / 3.0)
    };

    Ok(UhiTriadRisk {
        r_t,
        r_c,
        r_a,
        r_thermal,
    })
}

/// Convenience function: inject UHI thermal scalar into a RiskVector, leaving
/// other planes unchanged.
/// This allows drainage/AI crates to compute their own r_hyd, r_energy, r_ai, r_biodiv
/// while sharing a common thermal builder.[file:14]
pub fn apply_uhi_to_riskvector(
    mut rv: RiskVector,
    uhi_risk: &UhiTriadRisk,
) -> RiskVector {
    rv.r_thermal = uhi_risk.r_thermal;
    rv
}
