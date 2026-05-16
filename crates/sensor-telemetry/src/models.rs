use serde::{Deserialize, Serialize};
use time::OffsetDateTime;

use aln_core::{Did, HexHash};
use ecospine::{RiskCoord, KER};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeoPoint {
    pub lat: f64,
    pub lon: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MetricSample {
    pub metric_name: String,   // "soil_moisture_vol", "flow_m3s", "temperature_c", "canopy_cover_pct"
    pub value: f64,
    pub unit: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TelemetryParticle {
    pub sensor_id: String,
    pub sensor_did: Did,
    pub location: GeoPoint,
    pub observed_at: OffsetDateTime,
    pub metrics: Vec<MetricSample>,
    pub device_signature: HexHash,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorKerDelta {
    pub node_id: String,
    pub sensor_id: String,
    pub observed_at: OffsetDateTime,
    pub delta_k: f64,
    pub delta_e: f64,
    pub delta_r: f64,
    pub ker_before: KER,
    pub ker_after: KER,
    pub residual_before: f64,
    pub residual_after: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorHealthParticle {
    pub sensor_id: String,
    pub sensor_did: Did,
    pub checked_at: OffsetDateTime,
    pub reference_metric: String,
    pub deviation: f64,
    pub deviation_threshold: f64,
    pub healthy: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorTrustWeight {
    pub sensor_id: String,
    pub weight: f64, // 0.0 .. 1.0, decays to 0 on quarantine
}

pub trait SensorKerEvaluator {
    fn evaluate(
        &self,
        telemetry: &TelemetryParticle,
        target_corridors: &[(String, RiskCoord)],
        current_ker: &KER,
        current_residual: f64,
        trust_weight: f64,
    ) -> SensorKerDelta;
}
