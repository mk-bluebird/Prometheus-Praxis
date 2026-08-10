// File: crates/cyboquatic-core/src/lib.rs
#![deny(unsafe_op_in_unsafe_fn)]

mod cyboquatic_index;
mod energy_sampling;
mod frame_registry;
mod lane_promotion;
mod metrics;

pub mod thresholds;

use std::sync::OnceLock;

use jni::objects::{JClass, JValue};
use jni::sys::{jdouble, jobject};
use jni::JNIEnv;

use thresholds::Thresholds;

pub use crate::cyboquatic_index::{
    aggregate_by_region, build_cyboquatic_index, emit_region_geojson,
    espd_ecosafety_from_sample, make_phoenix_synthetic_sample, CyboquaticEcoPlot,
    CyboquaticIndex, CyboquaticNodeSample, CyboquaticRestorationSurface,
    CyboquaticWindowPlane, CyboquaticWindowWithPlanes, GeoJsonFeature,
    GeoJsonFeatureCollection, NodeRiskSample, RegionAggregate, Scalar, E_FACTOR,
    K_FACTOR, R_FACTOR,
};
pub use crate::energy_sampling::{
    EnergyAwareSamplingAdvice, EnergyAwareSamplingFrame, EnergyAwareSamplingParams,
};
pub use crate::frame_registry::{FrameKind, FrameRegistry};
pub use crate::lane_promotion::{
    KerSnapshot, Lane, LanePromotionRecommender, LanePromotionSuggestion,
};
pub use crate::metrics::{export_last_metrics, record_metrics_snapshot, MetricsSnapshot};

const STATUS_OK: i32 = 0;
const STATUS_NULL_POINTER: i32 = 1;
const STATUS_INVALID_INPUT: i32 = 2;
const STATUS_CONFIGURATION_ERROR: i32 = 3;

const WATER_DENSITY_KG_M3: f64 = 997.0;
const GRAVITY_M_S2: f64 = 9.80665;

static THRESHOLDS: OnceLock<Result<Thresholds, String>> = OnceLock::new();

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct WorkloadInput {
    pub flow_m3_s: f64,
    pub lift_m: f64,
    pub efficiency: f64,
    pub runtime_s: f64,
    pub voltage_drop_v: f64,
    pub renewable_fraction: f64,
    pub embodied_carbon_g_per_j: f64,
    pub biodiversity_risk: f64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WorkloadAssessment {
    pub energyreq_j: f64,
    pub delta_vt: f64,
    pub knowledge_factor: f64,
    pub eco_impact_value: f64,
    pub accepted: u8,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WorkloadError {
    InvalidInput,
    InvalidConfiguration,
    NonFiniteResult,
}

fn thresholds() -> Result<Thresholds, WorkloadError> {
    THRESHOLDS
        .get_or_init(Thresholds::load)
        .as_ref()
        .copied()
        .map_err(|_| WorkloadError::InvalidConfiguration)
}

fn bounded(value: f64, lower: f64, upper: f64) -> bool {
    value.is_finite() && (lower..=upper).contains(&value)
}

impl WorkloadInput {
    pub fn is_valid(self) -> bool {
        bounded(self.flow_m3_s, 0.0, f64::MAX)
            && bounded(self.lift_m, 0.0, f64::MAX)
            && bounded(self.efficiency, f64::MIN_POSITIVE, 1.0)
            && bounded(self.runtime_s, 0.0, f64::MAX)
            && bounded(self.voltage_drop_v, 0.0, f64::MAX)
            && bounded(self.renewable_fraction, 0.0, 1.0)
            && bounded(self.embodied_carbon_g_per_j, 0.0, f64::MAX)
            && bounded(self.biodiversity_risk, 0.0, 1.0)
    }
}

pub fn workload_thresholds() -> Result<Thresholds, WorkloadError> {
    thresholds()
}

pub fn assess_workload_value(
    input: WorkloadInput,
) -> Result<WorkloadAssessment, WorkloadError> {
    if !input.is_valid() {
        return Err(WorkloadError::InvalidInput);
    }

    let limits = thresholds()?;
    let energyreq_j = WATER_DENSITY_KG_M3
        * GRAVITY_M_S2
        * input.flow_m3_s
        * input.lift_m
        * input.runtime_s
        / input.efficiency;

    if !energyreq_j.is_finite() {
        return Err(WorkloadError::NonFiniteResult);
    }

    let carbon_g =
        energyreq_j * (1.0 - input.renewable_fraction) * input.embodied_carbon_g_per_j;
    let delta_vt = 0.55 * (carbon_g / 1000.0).min(1.0)
        + 0.30 * (input.voltage_drop_v / 24.0).min(1.0)
        + 0.15 * input.biodiversity_risk;
    let knowledge_factor = (1.0 - 0.5 * input.biodiversity_risk).clamp(0.0, 1.0);
    let eco_impact_value = ((0.55 * input.renewable_fraction + 0.45 * (1.0 - delta_vt))
        * (1.0 - input.biodiversity_risk))
        .clamp(0.0, 1.0);

    if !delta_vt.is_finite() || !knowledge_factor.is_finite() || !eco_impact_value.is_finite() {
        return Err(WorkloadError::NonFiniteResult);
    }

    Ok(WorkloadAssessment {
        energyreq_j,
        delta_vt,
        knowledge_factor,
        eco_impact_value,
        accepted: u8::from(
            delta_vt <= limits.delta_vt_max
                && knowledge_factor >= limits.knowledge_factor_min
                && eco_impact_value >= limits.eco_impact_min,
        ),
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn assess_workload(
    input: *const WorkloadInput,
    output: *mut WorkloadAssessment,
) -> i32 {
    if input.is_null() || output.is_null() {
        return STATUS_NULL_POINTER;
    }

    let input_value = unsafe { *input };
    match assess_workload_value(input_value) {
        Ok(assessment) => {
            unsafe { *output = assessment };
            STATUS_OK
        }
        Err(WorkloadError::InvalidConfiguration) => STATUS_CONFIGURATION_ERROR,
        Err(WorkloadError::InvalidInput | WorkloadError::NonFiniteResult) => STATUS_INVALID_INPUT,
    }
}

#[unsafe(no_mangle)]
pub extern "system" fn Java_org_prometheuspraxis_cyboquatic_WorkloadTelemetry_nativeAssess(
    mut env: JNIEnv,
    _: JClass,
    flow_m3_s: jdouble,
    lift_m: jdouble,
    efficiency: jdouble,
    runtime_s: jdouble,
    voltage_drop_v: jdouble,
    renewable_fraction: jdouble,
    embodied_carbon_g_per_j: jdouble,
    biodiversity_risk: jdouble,
) -> jobject {
    let input = WorkloadInput {
        flow_m3_s,
        lift_m,
        efficiency,
        runtime_s,
        voltage_drop_v,
        renewable_fraction,
        embodied_carbon_g_per_j,
        biodiversity_risk,
    };

    let assessment = match assess_workload_value(input) {
        Ok(value) => value,
        Err(WorkloadError::InvalidConfiguration) => {
            let _ = env.throw_new(
                "java/lang/IllegalStateException",
                "Cyboquatic threshold configuration is unavailable or invalid",
            );
            return std::ptr::null_mut();
        }
        Err(WorkloadError::InvalidInput | WorkloadError::NonFiniteResult) => {
            let _ = env.throw_new(
                "java/lang/IllegalArgumentException",
                "Invalid cyboquatic workload telemetry",
            );
            return std::ptr::null_mut();
        }
    };

    match env.new_object(
        "org/prometheuspraxis/cyboquatic/WorkloadTelemetry$Assessment",
        "(DDDDZ)V",
        &[
            JValue::Double(assessment.energyreq_j),
            JValue::Double(assessment.delta_vt),
            JValue::Double(assessment.knowledge_factor),
            JValue::Double(assessment.eco_impact_value),
            JValue::Bool(assessment.accepted),
        ],
    ) {
        Ok(object) => object.into_raw(),
        Err(_) => {
            let _ = env.throw_new(
                "java/lang/IllegalStateException",
                "Unable to construct workload assessment",
            );
            std::ptr::null_mut()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> WorkloadInput {
        WorkloadInput {
            flow_m3_s: 0.035,
            lift_m: 4.2,
            efficiency: 0.78,
            runtime_s: 900.0,
            voltage_drop_v: 2.1,
            renewable_fraction: 0.82,
            embodied_carbon_g_per_j: 0.000035,
            biodiversity_risk: 0.08,
        }
    }

    #[test]
    fn valid_input_produces_bounded_assessment() {
        let result = assess_workload_value(sample()).expect("sample must be valid");
        assert!(result.energyreq_j > 0.0);
        assert!((0.0..=1.0).contains(&result.delta_vt));
        assert!((0.0..=1.0).contains(&result.knowledge_factor));
        assert!((0.0..=1.0).contains(&result.eco_impact_value));
    }

    #[test]
    fn invalid_efficiency_is_rejected() {
        let mut input = sample();
        input.efficiency = 0.0;
        assert_eq!(
            assess_workload_value(input),
            Err(WorkloadError::InvalidInput)
        );
    }
}
