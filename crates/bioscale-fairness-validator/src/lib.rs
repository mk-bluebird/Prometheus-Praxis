// filename: src/lib.rs
// destination: eco_restoration_shard/crates/bioscale-fairness-validator/src/lib.rs

use kerresidual::{check_safestep, compute_e, compute_k, compute_r, compute_residual, KerSnapshot, PlaneWeights, RiskVector};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FairnessInput {
    pub risk_vector_before: RiskVector,
    pub risk_vector_after: RiskVector,
    pub weights: PlaneWeights,
    pub k_before: f32,
    pub k_after: f32,
    pub e_before: f32,
    pub e_after: f32,
    pub r_before: f32,
    pub r_after: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FairnessVerdict {
    pub safestep_ok: bool,
    pub k_monotone_ok: bool,
    pub e_monotone_ok: bool,
    pub r_monotone_ok: bool,
}

pub fn evaluate_fairness(input: &FairnessInput) -> FairnessVerdict {
    let v_before = compute_residual(&input.weights, &input.risk_vector_before);
    let v_after = compute_residual(&input.weights, &input.risk_vector_after);

    let safestep_ok = check_safestep(v_before, v_after);

    let k_before = compute_k(input.k_before);
    let k_after = compute_k(input.k_after);
    let e_before = compute_e(input.e_before);
    let e_after = compute_e(input.e_after);
    let r_before = compute_r(input.r_before);
    let r_after = compute_r(input.r_after);

    let k_monotone_ok = k_after >= k_before;
    let e_monotone_ok = e_after >= e_before;
    let r_monotone_ok = r_after <= r_before;

    FairnessVerdict {
        safestep_ok,
        k_monotone_ok,
        e_monotone_ok,
        r_monotone_ok,
    }
}
