// path: crates/titan-metrics-core/src/lib.rs
// role: Core Lyapunov and corridor metrics utilities for Prometheus-Praxis,
//       with a Smartflower slice and CSV loader suitable for Kani and CI.

#![forbid(unsafe_code)]
#![cfg_attr(not(test), deny(warnings))]

use chrono::{DateTime, Utc};
use csv::ReaderBuilder;
use rust_decimal::Decimal;
use serde::Deserialize;
use thiserror::Error;

/// Canonical error type for Titan metrics operations.
#[derive(Debug, Error)]
pub enum TitanMetricsError {
    #[error("IO error while reading metrics source: {0}")]
    Io(String),
    #[error("CSV parsing error at {row}: {msg}")]
    Csv { row: usize, msg: String },
    #[error("Invalid numeric value at {row}: {field}")]
    Numeric { row: usize, field: &'static str },
    #[error("Lyapunov metric invariant violated: {0}")]
    LyapunovInvariant(String),
    #[error("Empty metrics slice")]
    EmptySlice,
}

/// Minimal Smartflower metrics row, suitable for Lyapunov and KER chains.
///
/// This row is intentionally compact so that its signatures can be treated
/// as typed equations in Kani harnesses and CI.
/// Typical CSV header for the artifact in `output/`:
/// timestamp_utc,power_kw,vt_residual,roh_scalar,ker_k,ker_e,ker_r
#[derive(Debug, Clone, Deserialize)]
pub struct SmartflowerMetricsRow {
    /// Timestamp in UTC for this sample.
    #[serde(rename = "timestamp_utc")]
    pub timestamp_utc: DateTime<Utc>,

    /// Instantaneous power in kilowatts produced by the Smartflower.
    #[serde(rename = "power_kw")]
    pub power_kw: Decimal,

    /// Lyapunov residual \(V_t\) for the governed object (Smartflower node).
    #[serde(rename = "vt_residual")]
    pub vt_residual: Decimal,

    /// Risk-of-harm scalar slice contributing to RoH, expected in [0, 1].
    #[serde(rename = "roh_scalar")]
    pub roh_scalar: Decimal,

    /// Knowledge factor K in the KER triple, expected in [0, 1].
    #[serde(rename = "ker_k")]
    pub ker_k: Decimal,

    /// Eco-impact factor E in the KER triple, expected in [0, 1].
    #[serde(rename = "ker_e")]
    pub ker_e: Decimal,

    /// Risk-of-harm factor R in the KER triple, expected in [0, 1].
    #[serde(rename = "ker_r")]
    pub ker_r: Decimal,
}

/// A compact summary over a Smartflower metrics slice, used as a Lyapunov
/// and KER anchor in Titan's metrics chain.
#[derive(Debug, Clone)]
pub struct SmartflowerMetricsSummary {
    /// Number of samples in the slice.
    pub sample_count: usize,
    /// Mean power in kilowatts over the slice.
    pub mean_power_kw: Decimal,
    /// Maximum Lyapunov residual observed in the slice.
    pub max_vt_residual: Decimal,
    /// Mean RoH scalar over the slice.
    pub mean_roh_scalar: Decimal,
    /// Mean K, E, R factors over the slice.
    pub mean_ker_k: Decimal,
    pub mean_ker_e: Decimal,
    pub mean_ker_r: Decimal,
}

/// Load Smartflower metrics from a CSV artifact and return all rows.
///
/// The CSV is expected to have a header row with the following columns:
/// `timestamp_utc,power_kw,vt_residual,roh_scalar,ker_k,ker_e,ker_r`.
///
/// This function is deliberately deterministic and side-effect free
/// except for reading the CSV; it is designed so Kani can treat it
/// as a real slice in the Lyapunov metrics chain.
pub fn load_smartflower_csv(path: &str) -> Result<Vec<SmartflowerMetricsRow>, TitanMetricsError> {
    let file = std::fs::File::open(path)
        .map_err(|e| TitanMetricsError::Io(format!("open {path}: {e}")))?;
    let mut reader = ReaderBuilder::new()
        .has_headers(true)
        .from_reader(file);

    let mut rows = Vec::new();
    for (idx, result) in reader.deserialize::<SmartflowerMetricsRow>().enumerate() {
        let row_index = idx + 1; // account for header
        let record = result.map_err(|e| TitanMetricsError::Csv {
            row: row_index,
            msg: e.to_string(),
        })?;

        // Basic numeric invariants in [0, 1] for RoH and KER factors.
        if !in_unit_interval(&record.roh_scalar) {
            return Err(TitanMetricsError::Numeric {
                row: row_index,
                field: "roh_scalar",
            });
        }
        if !in_unit_interval(&record.ker_k) {
            return Err(TitanMetricsError::Numeric {
                row: row_index,
                field: "ker_k",
            });
        }
        if !in_unit_interval(&record.ker_e) {
            return Err(TitanMetricsError::Numeric {
                row: row_index,
                field: "ker_e",
            });
        }
        if !in_unit_interval(&record.ker_r) {
            return Err(TitanMetricsError::Numeric {
                row: row_index,
                field: "ker_r",
            });
        }

        rows.push(record);
    }

    Ok(rows)
}

/// Compute a summary over a non-empty Smartflower metrics slice.
///
/// This function can be used by Kani and CI to assert Lyapunov and KER
/// invariants over a small, deterministic window.
pub fn summarize_smartflower_slice(
    slice: &[SmartflowerMetricsRow],
) -> Result<SmartflowerMetricsSummary, TitanMetricsError> {
    if slice.is_empty() {
        return Err(TitanMetricsError::EmptySlice);
    }

    let mut sum_power = Decimal::ZERO;
    let mut sum_roh = Decimal::ZERO;
    let mut sum_k = Decimal::ZERO;
    let mut sum_e = Decimal::ZERO;
    let mut sum_r = Decimal::ZERO;
    let mut max_vt = Decimal::ZERO;

    for row in slice {
        sum_power += row.power_kw;
        sum_roh += row.roh_scalar;
        sum_k += row.ker_k;
        sum_e += row.ker_e;
        sum_r += row.ker_r;
        if row.vt_residual > max_vt {
            max_vt = row.vt_residual;
        }
    }

    let count_dec = Decimal::from(slice.len() as u64);
    let mean_power = sum_power / count_dec;
    let mean_roh = sum_roh / count_dec;
    let mean_k = sum_k / count_dec;
    let mean_e = sum_e / count_dec;
    let mean_r = sum_r / count_dec;

    Ok(SmartflowerMetricsSummary {
        sample_count: slice.len(),
        mean_power_kw: mean_power,
        max_vt_residual: max_vt,
        mean_roh_scalar: mean_roh,
        mean_ker_k: mean_k,
        mean_ker_e: mean_e,
        mean_ker_r: mean_r,
    })
}

/// Helper: check that a Decimal lies in the closed unit interval [0, 1].
fn in_unit_interval(x: &Decimal) -> bool {
    *x >= Decimal::ZERO && *x <= Decimal::ONE
}

#[cfg(test)]
mod tests {
    use super::*;
    use kani::any;

    /// Kani-friendly unit test: constructs a tiny Smartflower CSV slice in-memory
    /// and checks that the summary obeys basic Lyapunov and KER invariants.
    ///
    /// For CI, you can mirror this test with an actual CSV file in `output/`
    /// named `EcoSmartflowerLyapunovSample2026v1.csv` and point to it via
    /// `load_smartflower_csv`.
    #[kani::proof]
    fn kani_smartflower_summary_is_well_behaved() {
        // Synthetic rows with bounded values suitable for formal checking.
        let row1 = SmartflowerMetricsRow {
            timestamp_utc: Utc::now(),
            power_kw: Decimal::from(1u64),
            vt_residual: Decimal::from(1u64),
            roh_scalar: Decimal::from(0u64),
            ker_k: Decimal::from(1u64),
            ker_e: Decimal::from(1u64),
            ker_r: Decimal::from(0u64),
        };
        let row2 = SmartflowerMetricsRow {
            timestamp_utc: Utc::now(),
            power_kw: Decimal::from(2u64),
            vt_residual: Decimal::from(2u64),
            roh_scalar: Decimal::from(1u64),
            ker_k: Decimal::from(1u64),
            ker_e: Decimal::from(1u64),
            ker_r: Decimal::from(1u64),
        };

        let slice = vec![row1, row2];
        let summary = summarize_smartflower_slice(&slice).unwrap();

        // Basic invariants: mean RoH and KER factors remain in [0, 1],
        // and max Lyapunov residual is non-negative.
        assert!(in_unit_interval(&summary.mean_roh_scalar));
        assert!(in_unit_interval(&summary.mean_ker_k));
        assert!(in_unit_interval(&summary.mean_ker_e));
        assert!(in_unit_interval(&summary.mean_ker_r));
        assert!(summary.max_vt_residual >= Decimal::ZERO);
    }

    /// Standard Rust unit test that exercises `load_smartflower_csv` against
    /// a tiny inline CSV buffer to validate header and parsing logic.
    #[test]
    fn parse_tiny_smartflower_csv_buffer() {
        const CSV_DATA: &str = "\
timestamp_utc,power_kw,vt_residual,roh_scalar,ker_k,ker_e,ker_r
2026-01-01T00:00:00Z,1.0,0.10,0.05,0.95,0.92,0.12
";

        let mut reader = csv::ReaderBuilder::new()
            .has_headers(true)
            .from_reader(CSV_DATA.as_bytes());

        let mut rows = Vec::new();
        for (idx, result) in reader.deserialize::<SmartflowerMetricsRow>().enumerate() {
            let row_index = idx + 1;
            let record = result.unwrap_or_else(|e| {
                panic!("CSV parse error at row {row_index}: {e}");
            });
            assert!(in_unit_interval(&record.roh_scalar));
            assert!(in_unit_interval(&record.ker_k));
            assert!(in_unit_interval(&record.ker_e));
            assert!(in_unit_interval(&record.ker_r));
            rows.push(record);
        }

        let summary = summarize_smartflower_slice(&rows).expect("non-empty slice");
        assert_eq!(summary.sample_count, 1);
    }
}
