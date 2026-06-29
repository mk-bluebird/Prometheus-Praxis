// SPDX-License-Identifier: MIT OR Apache-2.0
//
// File: crates/titan-metrics-core/src/eco_solar_smartflower.rs
// Role: EcoSmartflowerLyapunovSample2026v1 CSV ingestion and basic eco/Lyapunov helpers.
//
// Edition and rust-version are controlled at the workspace level (edition = 2024, rust-version = "1.85").
// Kani verifier version is unified at workspace level (kani-verifier = "0.56.0") per Prometheus-Praxis rules.

#![deny(unsafe_code)]
#![allow(clippy::needless_return)]

use std::fs::File;
use std::io::BufReader;
use std::path::Path;

use serde::{Deserialize, Serialize};

use thiserror::Error;

/// Single Smartflower sample row for 2026 Lyapunov / eco-per-joule analysis.
///
/// This is intentionally minimal, and stable, so KER/Titan tools can derive
/// higher-level metrics without touching raw CSV directly.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoSmartflowerLyapunovSample2026v1 {
    /// UNIX timestamp (UTC) in seconds for this sample.
    pub sample_timestamp_utc: i64,

    /// Stable Smartflower asset identifier (e.g., "PHX-SMARTFLOWER-001").
    pub smartflower_id: String,

    /// Region or corridor identifier (e.g., "PHX-NORTH-GATEWAY").
    pub region: String,

    /// Instantaneous AC output from the Smartflower, in kilowatts.
    pub pv_output_kw: f64,

    /// Measured or estimated solar irradiance, W/m^2.
    pub irradiance_w_per_m2: f64,

    /// Panel or module temperature in degrees Celsius.
    pub panel_temp_c: f64,

    /// Wind speed at hub height in m/s (for mechanical/structural envelopes).
    pub wind_speed_mps: f64,

    /// Normalized dust/fouling index in [0, 1], 0 = clean, 1 = heavily fouled.
    pub dust_index_0_1: f64,

    /// Optional eco score in [0, 1] capturing combined eco-impact for this sample.
    /// If not present in CSV, this can default via derived scoring (e.g., energy output
    /// per ecological footprint). Kept as a scalar for easy fusion into KER.
    pub eco_score_0_1: f64,
}

/// Errors for Smartflower CSV loading and basic validation.
#[derive(Debug, Error)]
pub enum EcoSmartflowerCsvError {
    #[error("IO error while reading Smartflower CSV: {0}")]
    Io(#[from] std::io::Error),

    #[error("CSV parse error: {0}")]
    Csv(#[from] csv::Error),

    #[error("Constraint violation in row {row_index}: {msg}")]
    Constraint {
        row_index: usize,
        msg: String,
    },
}

/// Load EcoSmartflowerLyapunovSample2026v1 rows from a CSV file.
///
/// Expected header (comma-separated, case-sensitive):
/// sample_timestamp_utc,smartflower_id,region,pv_output_kw,irradiance_w_per_m2,
/// panel_temp_c,wind_speed_mps,dust_index_0_1,eco_score_0_1
pub fn load_smartflower_csv<P: AsRef<Path>>(
    path: P,
) -> Result<Vec<EcoSmartflowerLyapunovSample2026v1>, EcoSmartflowerCsvError> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);
    let mut csv_reader = csv::Reader::from_reader(reader);

    let mut out: Vec<EcoSmartflowerLyapunovSample2026v1> = Vec::new();

    for (idx, result) in csv_reader.deserialize().enumerate() {
        let row: EcoSmartflowerLyapunovSample2026v1 = result?;

        // Basic scalar constraints aligned with other eco/Lyapunov types:
        // - Output, irradiance, wind speed non-negative.
        // - Dust index and eco score in [0, 1].
        if row.pv_output_kw < 0.0 {
            return Err(EcoSmartflowerCsvError::Constraint {
                row_index: idx,
                msg: "pv_output_kw must be non-negative".to_string(),
            });
        }
        if row.irradiance_w_per_m2 < 0.0 {
            return Err(EcoSmartflowerCsvError::Constraint {
                row_index: idx,
                msg: "irradiance_w_per_m2 must be non-negative".to_string(),
            });
        }
        if row.wind_speed_mps < 0.0 {
            return Err(EcoSmartflowerCsvError::Constraint {
                row_index: idx,
                msg: "wind_speed_mps must be non-negative".to_string(),
            });
        }
        if !(0.0..=1.0).contains(&row.dust_index_0_1) {
            return Err(EcoSmartflowerCsvError::Constraint {
                row_index: idx,
                msg: "dust_index_0_1 must be within [0, 1]".to_string(),
            });
        }
        if !(0.0..=1.0).contains(&row.eco_score_0_1) {
            return Err(EcoSmartflowerCsvError::Constraint {
                row_index: idx,
                msg: "eco_score_0_1 must be within [0, 1]".to_string(),
            });
        }

        out.push(row);
    }

    Ok(out)
}

/// Compute a simple per-sample eco-per-joule efficiency metric.
///
/// This returns a dimensionless scalar proportional to:
///   pv_output_kw / irradiance_w_per_m2,
/// with a small epsilon to avoid division-by-zero. It can be used as
/// a component in a Lyapunov-like eco stability envelope.
pub fn eco_efficiency_per_sample(sample: &EcoSmartflowerLyapunovSample2026v1) -> f64 {
    // pv_output_kw ~ kW, irradiance_w_per_m2 ~ W/m^2. This is not a strict
    // physical efficiency, but a normalized ratio you can further scale.
    let eps = 1e-9;
    sample.pv_output_kw / (sample.irradiance_w_per_m2 + eps)
}

/// Simple Lyapunov-like scalar for Smartflower eco/thermal state.
///
/// Example structure:
///   V_t = w_p * pv_norm^2 + w_T * (delta_T)^2 + w_d * dust_index^2
/// where:
///   pv_norm  = pv_output_kw / pv_ref_kw
///   delta_T  = panel_temp_c - temp_ref_c
///   dust_index in [0, 1].
///
/// This is intentionally simple; more complex Koopman/ISS models can be
/// built on top and share the same sample type.
pub fn lyapunov_scalar_for_sample(
    sample: &EcoSmartflowerLyapunovSample2026v1,
    pv_ref_kw: f64,
    temp_ref_c: f64,
    w_p: f64,
    w_t: f64,
    w_d: f64,
) -> f64 {
    let pv_ref = if pv_ref_kw <= 0.0 { 1.0 } else { pv_ref_kw };
    let pv_norm = sample.pv_output_kw / pv_ref;

    let delta_t = sample.panel_temp_c - temp_ref_c;

    let dust = sample.dust_index_0_1;

    // All weights are assumed non-negative; caller is responsible for
    // choosing stable values. This function does no guard decisions,
    // it only computes V_t.
    w_p * pv_norm * pv_norm + w_t * delta_t * delta_t + w_d * dust * dust
}

/// Aggregate eco-efficiency and Lyapunov scalars over a set of samples.
/// These helpers are useful for Titan metrics dashboards or KER aggregates.
pub fn aggregate_eco_efficiency(samples: &[EcoSmartflowerLyapunovSample2026v1]) -> f64 {
    if samples.is_empty() {
        return 0.0;
    }
    let mut acc = 0.0;
    for s in samples {
        acc += eco_efficiency_per_sample(s);
    }
    acc / (samples.len() as f64)
}

pub fn aggregate_lyapunov_scalar(
    samples: &[EcoSmartflowerLyapunovSample2026v1],
    pv_ref_kw: f64,
    temp_ref_c: f64,
    w_p: f64,
    w_t: f64,
    w_d: f64,
) -> f64 {
    if samples.is_empty() {
        return 0.0;
    }
    let mut acc = 0.0;
    for s in samples {
        acc += lyapunov_scalar_for_sample(s, pv_ref_kw, temp_ref_c, w_p, w_t, w_d);
    }
    acc / (samples.len() as f64)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn eco_efficiency_is_non_negative_for_valid_sample() {
        let s = EcoSmartflowerLyapunovSample2026v1 {
            sample_timestamp_utc: 1_777_000_000,
            smartflower_id: "PHX-SMARTFLOWER-001".to_string(),
            region: "PHX-NORTH-GATEWAY".to_string(),
            pv_output_kw: 4.0,
            irradiance_w_per_m2: 800.0,
            panel_temp_c: 45.0,
            wind_speed_mps: 3.0,
            dust_index_0_1: 0.2,
            eco_score_0_1: 0.8,
        };

        let eff = eco_efficiency_per_sample(&s);
        assert!(eff >= 0.0);
    }

    #[test]
    fn lyapunov_scalar_grows_with_temp_and_dust() {
        let clean = EcoSmartflowerLyapunovSample2026v1 {
            sample_timestamp_utc: 1_777_000_000,
            smartflower_id: "PHX-SMARTFLOWER-001".to_string(),
            region: "PHX-NORTH-GATEWAY".to_string(),
            pv_output_kw: 4.0,
            irradiance_w_per_m2: 800.0,
            panel_temp_c: 35.0,
            wind_speed_mps: 3.0,
            dust_index_0_1: 0.0,
            eco_score_0_1: 0.9,
        };

        let dusty_hot = EcoSmartflowerLyapunovSample2026v1 {
            sample_timestamp_utc: 1_777_000_100,
            smartflower_id: "PHX-SMARTFLOWER-001".to_string(),
            region: "PHX-NORTH-GATEWAY".to_string(),
            pv_output_kw: 4.0,
            irradiance_w_per_m2: 800.0,
            panel_temp_c: 55.0,
            wind_speed_mps: 3.0,
            dust_index_0_1: 0.5,
            eco_score_0_1: 0.6,
        };

        let v_clean = lyapunov_scalar_for_sample(&clean, 4.0, 35.0, 1.0, 1.0, 1.0);
        let v_dusty_hot = lyapunov_scalar_for_sample(&dusty_hot, 4.0, 35.0, 1.0, 1.0, 1.0);

        assert!(v_dusty_hot > v_clean);
    }

    #[test]
    fn aggregate_helpers_return_zero_for_empty_slice() {
        let empty: Vec<EcoSmartflowerLyapunovSample2026v1> = Vec::new();
        assert_eq!(aggregate_eco_efficiency(&empty), 0.0);
        assert_eq!(
            aggregate_lyapunov_scalar(&empty, 4.0, 35.0, 1.0, 1.0, 1.0),
            0.0
        );
    }
}
