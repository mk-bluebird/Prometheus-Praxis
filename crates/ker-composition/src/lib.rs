// File: crates/ker-composition/src/lib.rs
#![forbid(unsafe_code)]

pub mod workload_proof;

pub use workload_proof::{
    ker_margin, prove_accepted_frame, prove_all_accepted, KerProofError, WorkloadKerFrame,
};

use rlua::{Function, Lua, Table};
use std::{
    ffi::{CStr, CString, NulError},
    os::raw::{c_char, c_float, c_int},
};

const LUA_VALIDATOR_SOURCE: &str =
    include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/lua/ker_composition_validator.lua"));

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KerParticle2026v1 {
    pub particle_id: *const c_char,
    pub topic_id: *const c_char,
    pub lane: *const c_char,
    pub k: c_float,
    pub e: c_float,
    pub r: c_float,
    pub evidencehex: *const c_char,
    pub signinghex: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KerComposition2026v1 {
    pub left_particle_id: *const c_char,
    pub right_particle_id: *const c_char,
    pub combined_id: *const c_char,
    pub k_combined: c_float,
    pub e_combined: c_float,
    pub r_combined: c_float,
    pub members: *const c_char,
    pub rule_id: *const c_char,
    pub evidencehex: *const c_char,
    pub signinghex: *const c_char,
}

unsafe extern "C" {
    fn ker_oplus_geom_min_max(
        left: *const KerParticle2026v1,
        right: *const KerParticle2026v1,
        out_comp: *mut KerComposition2026v1,
    ) -> c_int;
}

#[derive(Debug, Clone, PartialEq)]
pub struct RustKerParticle {
    pub particle_id: String,
    pub topic_id: String,
    pub lane: String,
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub evidencehex: String,
    pub signinghex: String,
}

#[derive(Debug, Clone, PartialEq)]
pub struct RustKerComposition {
    pub left_particle_id: String,
    pub right_particle_id: String,
    pub combined_id: String,
    pub k_combined: f32,
    pub e_combined: f32,
    pub r_combined: f32,
    pub members: String,
    pub rule_id: String,
    pub evidencehex: Option<String>,
    pub signinghex: Option<String>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct RustKerCompositionRow {
    pub left: RustKerParticle,
    pub right: RustKerParticle,
    pub comp_lane: String,
    pub k_combined: f32,
    pub e_combined: f32,
    pub r_combined: f32,
    pub members: String,
    pub rule_id: String,
    pub evidencehex: Option<String>,
}

#[derive(Debug)]
pub enum KerCompositionError {
    InvalidParticle,
    InteriorNul(NulError),
    NativeFailure(c_int),
    MissingNativeField(&'static str),
    InvalidNativeUtf8(&'static str),
    InvalidNativeScore,
}

impl From<NulError> for KerCompositionError {
    fn from(error: NulError) -> Self {
        Self::InteriorNul(error)
    }
}

impl RustKerParticle {
    fn validate(&self) -> Result<(), KerCompositionError> {
        let text_fields = [
            self.particle_id.as_str(),
            self.topic_id.as_str(),
            self.lane.as_str(),
            self.evidencehex.as_str(),
            self.signinghex.as_str(),
        ];
        if text_fields.iter().any(|value| value.trim().is_empty())
            || [self.k, self.e, self.r]
                .iter()
                .any(|value| !value.is_finite() || !(0.0..=1.0).contains(value))
        {
            return Err(KerCompositionError::InvalidParticle);
        }
        Ok(())
    }
}

fn required_c_string(
    ptr: *const c_char,
    field: &'static str,
) -> Result<String, KerCompositionError> {
    if ptr.is_null() {
        return Err(KerCompositionError::MissingNativeField(field));
    }
    let value = unsafe { CStr::from_ptr(ptr) }
        .to_str()
        .map_err(|_| KerCompositionError::InvalidNativeUtf8(field))?;
    if value.is_empty() {
        return Err(KerCompositionError::MissingNativeField(field));
    }
    Ok(value.into())
}

fn optional_c_string(
    ptr: *const c_char,
    field: &'static str,
) -> Result<Option<String>, KerCompositionError> {
    if ptr.is_null() {
        return Ok(None);
    }
    let value = unsafe { CStr::from_ptr(ptr) }
        .to_str()
        .map_err(|_| KerCompositionError::InvalidNativeUtf8(field))?;
    Ok((!value.is_empty()).then(|| value.into()))
}

pub fn ker_oplus_geom_min_max_rust(
    left: &RustKerParticle,
    right: &RustKerParticle,
) -> Result<RustKerComposition, KerCompositionError> {
    left.validate()?;
    right.validate()?;

    let left_id = CString::new(left.particle_id.as_str())?;
    let left_topic = CString::new(left.topic_id.as_str())?;
    let left_lane = CString::new(left.lane.as_str())?;
    let left_evidence = CString::new(left.evidencehex.as_str())?;
    let left_signing = CString::new(left.signinghex.as_str())?;
    let right_id = CString::new(right.particle_id.as_str())?;
    let right_topic = CString::new(right.topic_id.as_str())?;
    let right_lane = CString::new(right.lane.as_str())?;
    let right_evidence = CString::new(right.evidencehex.as_str())?;
    let right_signing = CString::new(right.signinghex.as_str())?;

    let native_left = KerParticle2026v1 {
        particle_id: left_id.as_ptr(),
        topic_id: left_topic.as_ptr(),
        lane: left_lane.as_ptr(),
        k: left.k,
        e: left.e,
        r: left.r,
        evidencehex: left_evidence.as_ptr(),
        signinghex: left_signing.as_ptr(),
    };
    let native_right = KerParticle2026v1 {
        particle_id: right_id.as_ptr(),
        topic_id: right_topic.as_ptr(),
        lane: right_lane.as_ptr(),
        k: right.k,
        e: right.e,
        r: right.r,
        evidencehex: right_evidence.as_ptr(),
        signinghex: right_signing.as_ptr(),
    };
    let mut output = KerComposition2026v1 {
        left_particle_id: std::ptr::null(),
        right_particle_id: std::ptr::null(),
        combined_id: std::ptr::null(),
        k_combined: 0.0,
        e_combined: 0.0,
        r_combined: 0.0,
        members: std::ptr::null(),
        rule_id: std::ptr::null(),
        evidencehex: std::ptr::null(),
        signinghex: std::ptr::null(),
    };

    let status = unsafe { ker_oplus_geom_min_max(&native_left, &native_right, &mut output) };
    if status != 0 {
        return Err(KerCompositionError::NativeFailure(status));
    }
    if [output.k_combined, output.e_combined, output.r_combined]
        .iter()
        .any(|value| !value.is_finite() || !(0.0..=1.0).contains(value))
    {
        return Err(KerCompositionError::InvalidNativeScore);
    }

    Ok(RustKerComposition {
        left_particle_id: required_c_string(output.left_particle_id, "left_particle_id")?,
        right_particle_id: required_c_string(output.right_particle_id, "right_particle_id")?,
        combined_id: required_c_string(output.combined_id, "combined_id")?,
        k_combined: output.k_combined,
        e_combined: output.e_combined,
        r_combined: output.r_combined,
        members: required_c_string(output.members, "members")?,
        rule_id: required_c_string(output.rule_id, "rule_id")?,
        evidencehex: optional_c_string(output.evidencehex, "evidencehex")?,
        signinghex: optional_c_string(output.signinghex, "signinghex")?,
    })
}

pub fn validate_composition_with_lua(
    row: &RustKerCompositionRow,
) -> Result<bool, rlua::Error> {
    let lua = Lua::new();
    lua.context(|context| {
        let module: Table = context.load(LUA_VALIDATOR_SOURCE).eval()?;
        let validate: Function = module.get("validate")?;

        let left = context.create_table()?;
        left.set("k", row.left.k)?;
        left.set("e", row.left.e)?;
        left.set("r", row.left.r)?;
        left.set("lane", row.left.lane.as_str())?;

        let right = context.create_table()?;
        right.set("k", row.right.k)?;
        right.set("e", row.right.e)?;
        right.set("r", row.right.r)?;
        right.set("lane", row.right.lane.as_str())?;

        let composition = context.create_table()?;
        composition.set("k_combined", row.k_combined)?;
        composition.set("e_combined", row.e_combined)?;
        composition.set("r_combined", row.r_combined)?;
        composition.set("members", row.members.as_str())?;
        composition.set("rule_id", row.rule_id.as_str())?;
        composition.set("lane", row.comp_lane.as_str())?;
        composition.set("evidencehex", row.evidencehex.as_deref().unwrap_or(""))?;

        let payload = context.create_table()?;
        payload.set("left", left)?;
        payload.set("right", right)?;
        payload.set("composition", composition)?;
        validate.call(payload)
    })
}
