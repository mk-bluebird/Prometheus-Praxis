//! sensor_telemetry library

use serde::{Deserialize, Serialize};

/// Sensor telemetry particle representing raw sensor data.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorTelemetryParticle {
    pub sensor_id: String,
    pub timestamp: i64,
    pub value: f64,
}

pub fn hello() -> &'static str {
    "sensor_telemetry"
}

/// Extension trait for converting sensor telemetry into risk coordinates.
pub trait IntoRiskCoord {
    fn into_risk_coord(self) -> prometheus_praxis_spine::RiskCoord;
}

impl IntoRiskCoord for SensorTelemetryParticle {
    /// Convert sensor telemetry into a RiskCoord for prometheus_praxis_spine integration.
    /// Stub implementation - mapping logic to be filled in, mirroring sensor-health's interface.
    fn into_risk_coord(self) -> prometheus_praxis_spine::RiskCoord {
        // TODO: Implement mapping from SensorTelemetryParticle to RiskCoord
        // This stub mirrors the sensor-health interface with placeholder values.
        prometheus_praxis_spine::RiskCoord {
            plane_id: prometheus_praxis_spine::PlaneId::new("sensor-telemetry"),
            risk_value: 0.0,
            confidence: 0.0,
        }
    }
}

/// Direct conversion function as requested in the task.
pub fn into_risk_coord(telemetry: SensorTelemetryParticle) -> prometheus_praxis_spine::RiskCoord {
    telemetry.into_risk_coord()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "sensor_telemetry");
    }
}
