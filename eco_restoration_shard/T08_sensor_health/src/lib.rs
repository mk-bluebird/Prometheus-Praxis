// filename: T08_sensor_health/src/lib.rs
// Edition: 2024, rust-version = "1.85"
// Dependencies (all real and verifiable):
//   chrono = "0.4"
//   serde = { version = "1.0.203", features = ["derive"] }
//   thiserror = "1.0"
//
// Upgrade goals:
// - Numerical robustness (streaming variance, NaN/inf handling).
// - Time-window awareness (start/end, duration).
// - Explicit eco-impact and risk scoring for cyboquatic machinery.
// - Clear, deterministic behavior for empty and partial windows.

use chrono::{DateTime, Duration, Utc}; // [web:41][web:43][web:48]
use serde::{Deserialize, Serialize};   // [web:47][web:50]
use thiserror::Error;                  // [web:46][web:49]

#[derive(Debug, Error)]
pub enum SensorHealthError {
    #[error("invalid reading window: {0}")]
    InvalidWindow(String),

    #[error("non-finite reading encountered (NaN or infinite)")]
    NonFiniteReading,
}

/// Single sensor reading with UTC timestamp and numeric value.
/// `value` is expected to be finite (no NaN, no +/-inf).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorReading {
    pub timestamp: DateTime<Utc>,
    pub value: f64,
}

/// Extended health summary including eco-impact and risk.
/// All statistics are defined for a single contiguous window.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorHealthSummary {
    pub sensor_id: String,
    pub count: usize,
    pub mean: f64,
    pub min: f64,
    pub max: f64,
    pub std_dev: f64,
    pub missing_fraction: f64,
    pub window_start: Option<DateTime<Utc>>,
    pub window_end: Option<DateTime<Utc>>,
    pub window_duration_secs: Option<i64>,
    /// Eco-impact score (0–1, higher means healthier contribution to restoration pipeline).
    pub eco_score: f64,
    /// Risk score (0–1, higher means more risk; implementations should aim to minimize this).
    pub risk_score: f64,
}

pub fn analyze_sensor_health(
    sensor_id: &str,
    readings: &[SensorReading],
    expected_window_len: usize,
) -> Result<SensorHealthSummary, SensorHealthError> {
    if expected_window_len == 0 {
        return Err(SensorHealthError::InvalidWindow(
            "expected_window_len must be > 0".to_string(),
        ));
    }

    if readings.iter().any(|r| !r.value.is_finite()) {
        return Err(SensorHealthError::NonFiniteReading);
    }

    let count = readings.len();
    if count == 0 {
        return Ok(SensorHealthSummary {
            sensor_id: sensor_id.to_string(),
            count: 0,
            mean: 0.0,
            min: 0.0,
            max: 0.0,
            std_dev: 0.0,
            missing_fraction: 1.0,
            window_start: None,
            window_end: None,
            window_duration_secs: None,
            eco_score: 0.0,
            risk_score: 1.0,
        });
    }

    // Streaming mean and variance (Welford) for numerical stability.
    let mut min_v = readings[0].value;
    let mut max_v = readings[0].value;
    let mut mean = 0.0;
    let mut m2 = 0.0;
    let mut n = 0usize;

    for r in readings {
        let v = r.value;
        if v < min_v {
            min_v = v;
        }
        if v > max_v {
            max_v = v;
        }

        n += 1;
        let delta = v - mean;
        mean += delta / n as f64;
        let delta2 = v - mean;
        m2 += delta2 * delta2;
    }

    let std_dev = if n > 1 {
        (m2 / (n as f64 - 1.0)).sqrt()
    } else {
        0.0
    };

    let missing_fraction = if expected_window_len > count {
        (expected_window_len - count) as f64 / expected_window_len as f64
    } else {
        0.0
    };

    // Determine time window bounds and duration.
    let window_start = readings.iter().map(|r| r.timestamp).min();
    let window_end = readings.iter().map(|r| r.timestamp).max();
    let window_duration_secs = match (window_start, window_end) {
        (Some(start), Some(end)) => {
            let duration: Duration = end - start;
            Some(duration.num_seconds())
        }
        _ => None,
    };

    // Simple eco-impact and risk heuristics:
    // - Lower missing_fraction increases eco_score.
    // - Lower std_dev relative to |mean| indicates stable sensor behavior.
    // These are bounded in [0, 1] for downstream composability.
    let completeness_score = 1.0 - missing_fraction.clamp(0.0, 1.0);
    let stability_score = if mean.abs() > f64::EPSILON {
        let ratio = (std_dev / mean.abs()).clamp(0.0, 10.0);
        1.0 - (ratio / 10.0)
    } else {
        1.0
    };

    let eco_score = ((completeness_score + stability_score) / 2.0).clamp(0.0, 1.0);
    let risk_score = (1.0 - eco_score).clamp(0.0, 1.0);

    Ok(SensorHealthSummary {
        sensor_id: sensor_id.to_string(),
        count,
        mean,
        min: min_v,
        max: max_v,
        std_dev,
        missing_fraction,
        window_start,
        window_end,
        window_duration_secs,
        eco_score,
        risk_score,
    })
}
