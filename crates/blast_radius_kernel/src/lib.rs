// File: crates/blast_radius_kernel/src/lib.rs
#![deny(unsafe_op_in_unsafe_fn)]

pub mod eco_weight;
pub mod ffi;
pub mod lambda_compute;
pub mod model;

use jni::{
    objects::{JClass, JValue},
    sys::{jdouble, jobject},
    JNIEnv,
};

pub use crate::ffi::{
    eco_blast_radius_free_cstring, eco_lambda_for_region_json, eco_lambda_for_segment_json,
};
pub use crate::lambda_compute::compute_lambda_for_segment;
pub use crate::model::{LambdaQuery, LambdaSummary};

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct BlastRadiusInput {
    pub energy_j: f64,
    pub energy_corridor_j: f64,
    pub attenuation_m_inv: f64,
    pub base_radius_m: f64,
    pub biodiversity_risk: f64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct BlastRadiusOutput {
    pub radius_m: f64,
    pub normalized_energy_risk: f64,
    pub eco_impact_value: f64,
    pub within_corridor: u8,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BlastRadiusError {
    NonFiniteInput,
    NegativeEnergy,
    InvalidEnergyCorridor,
    InvalidAttenuation,
    NegativeBaseRadius,
    InvalidBiodiversityRisk,
}

impl BlastRadiusInput {
    pub fn validate(self) -> Result<(), BlastRadiusError> {
        let values = [
            self.energy_j,
            self.energy_corridor_j,
            self.attenuation_m_inv,
            self.base_radius_m,
            self.biodiversity_risk,
        ];
        if values.iter().any(|value| !value.is_finite()) {
            return Err(BlastRadiusError::NonFiniteInput);
        }
        if self.energy_j < 0.0 {
            return Err(BlastRadiusError::NegativeEnergy);
        }
        if self.energy_corridor_j <= 0.0 {
            return Err(BlastRadiusError::InvalidEnergyCorridor);
        }
        if self.attenuation_m_inv <= 0.0 {
            return Err(BlastRadiusError::InvalidAttenuation);
        }
        if self.base_radius_m < 0.0 {
            return Err(BlastRadiusError::NegativeBaseRadius);
        }
        if !(0.0..=1.0).contains(&self.biodiversity_risk) {
            return Err(BlastRadiusError::InvalidBiodiversityRisk);
        }
        Ok(())
    }
}

pub fn assess(input: BlastRadiusInput) -> Result<BlastRadiusOutput, BlastRadiusError> {
    input.validate()?;

    let normalized_energy_risk = (input.energy_j / input.energy_corridor_j).clamp(0.0, 1.0);
    let radius_m =
        input.base_radius_m + (1.0 + input.energy_j).ln() / input.attenuation_m_inv;
    let eco_impact_value =
        ((1.0 - normalized_energy_risk) * (1.0 - input.biodiversity_risk)).clamp(0.0, 1.0);

    Ok(BlastRadiusOutput {
        radius_m,
        normalized_energy_risk,
        eco_impact_value,
        within_corridor: u8::from(input.energy_j <= input.energy_corridor_j),
    })
}

pub fn compute_blast_radius(segment_id: &str, lambda: f64) -> blastradius::BlastRadius {
    let attenuation_m_inv = if lambda.is_finite() && lambda > 0.0 {
        lambda
    } else {
        f64::MIN_POSITIVE
    };
    let radius_meters = attenuation_m_inv.recip();
    let confidence = (attenuation_m_inv / (1.0 + attenuation_m_inv)).clamp(0.0, 1.0);

    blastradius::BlastRadius {
        segment_id: segment_id.to_owned(),
        radius_meters,
        confidence,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn compute_ecological_blast_radius(
    input: *const BlastRadiusInput,
    output: *mut BlastRadiusOutput,
) -> i32 {
    if input.is_null() || output.is_null() {
        return 1;
    }

    let input = unsafe { input.read() };
    match assess(input) {
        Ok(result) => {
            unsafe { output.write(result) };
            0
        }
        Err(_) => 2,
    }
}

#[unsafe(no_mangle)]
pub extern "system" fn Java_org_prometheuspraxis_cyboquatic_BlastRadius_nativeAssess(
    mut env: JNIEnv,
    _: JClass,
    energy_j: jdouble,
    energy_corridor_j: jdouble,
    attenuation_m_inv: jdouble,
    base_radius_m: jdouble,
    biodiversity_risk: jdouble,
) -> jobject {
    let input = BlastRadiusInput {
        energy_j,
        energy_corridor_j,
        attenuation_m_inv,
        base_radius_m,
        biodiversity_risk,
    };

    let result = match assess(input) {
        Ok(result) => result,
        Err(_) => {
            let _ = env.throw_new(
                "java/lang/IllegalArgumentException",
                "Invalid ecological blast-radius input",
            );
            return std::ptr::null_mut();
        }
    };

    match env.new_object(
        "org/prometheuspraxis/cyboquatic/BlastRadius$Assessment",
        "(DDDZ)V",
        &[
            JValue::Double(result.radius_m),
            JValue::Double(result.normalized_energy_risk),
            JValue::Double(result.eco_impact_value),
            JValue::Bool(result.within_corridor),
        ],
    ) {
        Ok(object) => object.into_raw(),
        Err(_) => {
            let _ = env.throw_new(
                "java/lang/IllegalStateException",
                "Cannot allocate BlastRadius.Assessment",
            );
            std::ptr::null_mut()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn assessment_returns_bounded_ecological_values() {
        let result = assess(BlastRadiusInput {
            energy_j: 120_000.0,
            energy_corridor_j: 250_000.0,
            attenuation_m_inv: 0.025,
            base_radius_m: 15.0,
            biodiversity_risk: 0.08,
        })
        .unwrap();

        assert!(result.radius_m >= 15.0);
        assert!((0.0..=1.0).contains(&result.normalized_energy_risk));
        assert!((0.0..=1.0).contains(&result.eco_impact_value));
        assert_eq!(result.within_corridor, 1);
    }

    #[test]
    fn excessive_energy_is_outside_the_corridor() {
        let result = assess(BlastRadiusInput {
            energy_j: 300_000.0,
            energy_corridor_j: 250_000.0,
            attenuation_m_inv: 0.025,
            base_radius_m: 15.0,
            biodiversity_risk: 0.08,
        })
        .unwrap();

        assert_eq!(result.normalized_energy_risk, 1.0);
        assert_eq!(result.within_corridor, 0);
    }

    #[test]
    fn invalid_input_is_rejected() {
        let result = assess(BlastRadiusInput {
            energy_j: -1.0,
            energy_corridor_j: 250_000.0,
            attenuation_m_inv: 0.025,
            base_radius_m: 15.0,
            biodiversity_risk: 0.08,
        });

        assert_eq!(result, Err(BlastRadiusError::NegativeEnergy));
    }

    #[test]
    fn segment_radius_uses_attenuation_length() {
        let result = compute_blast_radius("canal-segment-17", 0.25);

        assert_eq!(result.segment_id, "canal-segment-17");
        assert_eq!(result.radius_meters, 4.0);
        assert!(result.confidence > 0.0);
    }
}
