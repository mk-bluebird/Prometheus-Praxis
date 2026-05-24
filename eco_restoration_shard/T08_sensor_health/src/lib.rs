// filename: T08_sensor_health/src/lib.rs

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum SensorHealthError {
    #[error("invalid reading window: {0}")]
    InvalidWindow(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorReading {
    pub timestamp: DateTime<Utc>,
    pub value: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorHealthSummary {
    pub sensor_id: String,
    pub count: usize,
    pub mean: f64,
    pub min: f64,
    pub max: f64,
    pub std_dev: f64,
    pub missing_fraction: f64,
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
        });
    }

    let mut min_v = readings[0].value;
    let mut max_v = readings[0].value;
    let mut sum = 0.0;
    for r in readings {
        let v = r.value;
        if v < min_v {
            min_v = v;
        }
        if v > max_v {
            max_v = v;
        }
        sum += v;
    }

    let mean = sum / count as f64;

    let mut var_sum = 0.0;
    for r in readings {
        let dv = r.value - mean;
        var_sum += dv * dv;
    }
    let std_dev = if count > 1 {
        (var_sum / (count as f64 - 1.0)).sqrt()
    } else {
        0.0
    };

    let missing_fraction = if expected_window_len > count {
        (expected_window_len - count) as f64 / expected_window_len as f64
    } else {
        0.0
    };

    Ok(SensorHealthSummary {
        sensor_id: sensor_id.to_string(),
        count,
        mean,
        min: min_v,
        max: max_v,
        std_dev,
        missing_fraction,
    })
}
