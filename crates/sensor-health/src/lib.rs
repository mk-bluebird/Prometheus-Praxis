use serde::{Deserialize, Serialize};
use time::OffsetDateTime;

use sensor_telemetry::{SensorHealthParticle, SensorTrustWeight};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorHealthPolicy {
    pub quarantine_threshold: f64,
    pub decay_rate: f64, // per check
}

pub fn update_sensor_trust(
    health: &SensorHealthParticle,
    prev_weight: f64,
    policy: &SensorHealthPolicy,
) -> SensorTrustWeight {
    if !health.healthy || health.deviation > policy.quarantine_threshold {
        SensorTrustWeight {
            sensor_id: health.sensor_id.clone(),
            weight: 0.0,
        }
    } else {
        let new_weight = (prev_weight * (1.0 - policy.decay_rate)).clamp(0.0, 1.0);
        SensorTrustWeight {
            sensor_id: health.sensor_id.clone(),
            weight: new_weight,
        }
    }
}

/// Extension trait for converting sensor health into risk coordinates.
pub trait IntoRiskCoord {
    fn into_risk_coord(self) -> prometheus_praxis_spine::RiskCoord;
}

impl IntoRiskCoord for SensorHealthParticle {
    /// Convert sensor health state into a RiskCoord for prometheus_praxis_spine integration.
    /// Stub implementation - mapping logic to be filled in.
    fn into_risk_coord(self) -> prometheus_praxis_spine::RiskCoord {
        // TODO: Implement mapping from SensorHealthParticle to RiskCoord
        // This stub tags the sensor-health plane and provides placeholder risk values.
        prometheus_praxis_spine::RiskCoord {
            plane_id: prometheus_praxis_spine::PlaneId::new("sensor-health"),
            risk_value: 0.0,
            confidence: 0.0,
        }
    }
}

/// Direct conversion function as requested in the task.
pub fn into_risk_coord(health: SensorHealthParticle) -> prometheus_praxis_spine::RiskCoord {
    health.into_risk_coord()
}
