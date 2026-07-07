// filename: cyboquatic-ecosafety/src/jni_bindings.rs
// destination: cyboquatic-ecosafety/src/jni_bindings.rs

#![forbid(unsafe_code)]

use alloc::string::String;
use alloc::vec::Vec;

use crate::core::{EcosafetyConfig, EcosafetyWindowInput, EcosafetyDiagnostics};
use crate::serde_support::{from_json_config, to_json_diagnostics};

/// Simplified DTO for JNI, stable across Android releases.
#[derive(Debug, Clone)]
pub struct JniEcosafetyConfig {
    pub region_id: String,
    pub node_family: String,
    pub vt_ceiling: f32,
    pub roh_ceiling: f32,
}

/// Binary diagnostics envelope for JNI callers that want compact responses.
#[derive(Debug, Clone)]
pub struct JniEcosafetyDiagnosticsBin {
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub vt: f32,
    pub roh: f32,
    pub ecosafety_state: i32,
}

/// Map JNI DTO into core config.
pub fn config_from_jni(jni: JniEcosafetyConfig) -> EcosafetyConfig {
    EcosafetyConfig {
        region_id: jni.region_id,
        node_family: jni.node_family,
        vt_ceiling: jni.vt_ceiling,
        roh_ceiling: jni.roh_ceiling,
    }
}

/// Run ecosafety diagnostics and return JSON string for Kotlin.
pub fn jni_eval_window_json(
    cfg: &JniEcosafetyConfig,
    window: &EcosafetyWindowInput,
) -> Option<String> {
    let core_cfg = config_from_jni(cfg.clone());
    let diag: EcosafetyDiagnostics = crate::core::evaluate_window(&core_cfg, window)?;
    Some(to_json_diagnostics(&diag))
}

/// Run ecosafety diagnostics and return compact binary struct.
pub fn jni_eval_window_bin(
    cfg: &JniEcosafetyConfig,
    window: &EcosafetyWindowInput,
) -> Option<JniEcosafetyDiagnosticsBin> {
    let core_cfg = config_from_jni(cfg.clone());
    let diag: EcosafetyDiagnostics = crate::core::evaluate_window(&core_cfg, window)?;
    Some(JniEcosafetyDiagnosticsBin {
        k: diag.k,
        e: diag.e,
        r: diag.r,
        vt: diag.vt,
        roh: diag.roh,
        ecosafety_state: diag.state.as_i32(),
    })
}

/// Optional: accept config as JSON, mirror Android side where you hold JSON blobs.
pub fn jni_eval_window_from_json_cfg(
    cfg_json: &str,
    window: &EcosafetyWindowInput,
) -> Option<String> {
    let cfg: EcosafetyConfig = from_json_config(cfg_json).ok()?;
    let diag: EcosafetyDiagnostics = crate::core::evaluate_window(&cfg, window)?;
    Some(to_json_diagnostics(&diag))
}
