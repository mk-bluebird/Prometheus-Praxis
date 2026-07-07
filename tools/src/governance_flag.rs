//! Governance flag computation for MCP tools.
//!
//! This module implements the blast-radius indicator B_f and the
//! governance-required flag G_f for functions described in
//! `ppx.function.meta.v1`. It is intended for CI checks and MCP
//! server wiring, not for runtime actuation.

#![forbid(unsafe_code)]

/// In-memory representation of a PpxFunctionMetaV1 row.
#[derive(Clone, Debug)]
pub struct FunctionMeta {
    pub id: String,
    pub domains: Vec<String>,
    pub corridors: Vec<String>,
    pub capitals: Vec<String>,
    pub nodecount: u32,
    pub actuating: bool,
    pub ecosafetyrequired: bool,
    pub policytag: String,
}

/// Blast-radius indicator B_f.
///
/// B_f = 0 for local, single-domain, single-corridor, single-capital
/// diagnostics over a small node set; 1 otherwise.
pub fn blast_radius_indicator(meta: &FunctionMeta, n_local: u32) -> u8 {
    let mut b = 0u8;

    if meta.domains.len() > 1 {
        b = 1;
    }
    if meta.corridors.len() > 1 {
        b = 1;
    }
    if meta.capitals.len() > 1 {
        b = 1;
    }
    if meta.nodecount > n_local {
        b = 1;
    }

    b
}

/// Healthcare/cybernetics/nanoswarm reward flag H_f.
///
/// H_f = 1 if the function is actuating and falls into any of the
/// sensitive policy domains where governance must always gate.
pub fn reward_actuation_flag(meta: &FunctionMeta) -> u8 {
    if !meta.actuating {
        return 0;
    }

    let policy = meta.policytag.to_lowercase();

    if policy.contains("nanoswarmreward")
        || policy.contains("healthcare")
        || policy.contains("cybernetics")
        || policy.contains("dataaslaborreward")
    {
        1
    } else {
        0
    }
}

/// Governance-required flag G_f = max(B_f, H_f).
///
/// If this function returns true, CI and MCP must ensure that:
/// - `meta.ecosafetyrequired` is true, and
/// - a `REQUIRES_GATE` binding exists in ALN for this function.
pub fn governance_required(meta: &FunctionMeta, n_local: u32) -> bool {
    let b_f = blast_radius_indicator(meta, n_local);
    let h_f = reward_actuation_flag(meta);
    (b_f | h_f) == 1
}
