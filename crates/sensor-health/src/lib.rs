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
