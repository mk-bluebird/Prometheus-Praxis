use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_float, c_int};

#[repr(C)]
pub struct KerParticle2026v1 {
    pub particle_id: *const c_char,
    pub topic_id: *const c_char,
    pub lane: *const c_char,
    pub K: c_float,
    pub E: c_float,
    pub R: c_float,
    pub evidencehex: *const c_char,
    pub signinghex: *const c_char,
}

#[repr(C)]
pub struct KerComposition2026v1 {
    pub left_particle_id: *const c_char,
    pub right_particle_id: *const c_char,
    pub combined_id: *const c_char,
    pub K_combined: c_float,
    pub E_combined: c_float,
    pub R_combined: c_float,
    pub members: *const c_char,
    pub rule_id: *const c_char,
    pub evidencehex: *const c_char,
    pub signinghex: *const c_char,
}

#[link(name = "ker_oplus_geom_min_max")]
extern "C" {
    fn ker_oplus_geom_min_max(
        left: *const KerParticle2026v1,
        right: *const KerParticle2026v1,
        out_comp: *mut KerComposition2026v1,
    ) -> c_int;
}

#[derive(Debug, Clone)]
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

#[derive(Debug, Clone)]
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

fn to_c_string(s: &str) -> CString {
    CString::new(s).expect("string contains interior NUL")
}

fn from_c_str(ptr: *const c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
}

pub fn ker_oplus_geom_min_max_rust(
    left: &RustKerParticle,
    right: &RustKerParticle,
) -> Result<RustKerComposition, i32> {
    let c_left_id = to_c_string(&left.particle_id);
    let c_left_topic = to_c_string(&left.topic_id);
    let c_left_lane = to_c_string(&left.lane);
    let c_left_evidence = to_c_string(&left.evidencehex);
    let c_left_signing = to_c_string(&left.signinghex);

    let c_right_id = to_c_string(&right.particle_id);
    let c_right_topic = to_c_string(&right.topic_id);
    let c_right_lane = to_c_string(&right.lane);
    let c_right_evidence = to_c_string(&right.evidencehex);
    let c_right_signing = to_c_string(&right.signinghex);

    let c_left = KerParticle2026v1 {
        particle_id: c_left_id.as_ptr(),
        topic_id: c_left_topic.as_ptr(),
        lane: c_left_lane.as_ptr(),
        K: left.k,
        E: left.e,
        R: left.r,
        evidencehex: c_left_evidence.as_ptr(),
        signinghex: c_left_signing.as_ptr(),
    };

    let c_right = KerParticle2026v1 {
        particle_id: c_right_id.as_ptr(),
        topic_id: c_right_topic.as_ptr(),
        lane: c_right_lane.as_ptr(),
        K: right.k,
        E: right.e,
        R: right.r,
        evidencehex: c_right_evidence.as_ptr(),
        signinghex: c_right_signing.as_ptr(),
    };

    let mut out = KerComposition2026v1 {
        left_particle_id: std::ptr::null(),
        right_particle_id: std::ptr::null(),
        combined_id: std::ptr::null(),
        K_combined: 0.0,
        E_combined: 0.0,
        R_combined: 0.0,
        members: std::ptr::null(),
        rule_id: std::ptr::null(),
        evidencehex: std::ptr::null(),
        signinghex: std::ptr::null(),
    };

    let status = unsafe { ker_oplus_geom_min_max(&c_left, &c_right, &mut out) };
    if status != 0 {
        return Err(status);
    }

    Ok(RustKerComposition {
        left_particle_id: from_c_str(out.left_particle_id),
        right_particle_id: from_c_str(out.right_particle_id),
        combined_id: from_c_str(out.combined_id),
        k_combined: out.K_combined,
        e_combined: out.E_combined,
        r_combined: out.R_combined,
        members: from_c_str(out.members),
        rule_id: from_c_str(out.rule_id),
        evidencehex: if out.evidencehex.is_null() {
            None
        } else {
            Some(from_c_str(out.evidencehex))
        },
        signinghex: if out.signinghex.is_null() {
            None
        } else {
            Some(from_c_str(out.signinghex))
        },
    })
}
