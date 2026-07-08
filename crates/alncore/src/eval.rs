// filename: crates/alncore/src/eval.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use crate::model::{DeployDecisionKernel, KerSnapshot, SafeStepRule};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DeployDecision {
    Admissible,
    Rejected { reason: String },
}

pub fn eval_safestep(v_t: f64, v_t1: f64, rule: &SafeStepRule) -> bool {
    let epsilon = rule.epsilon.max(0.0);
    if v_t1 > v_t + epsilon {
        return false;
    }
    if let Some(ceil) = rule.vt_ceiling {
        if v_t1 > ceil {
            return false;
        }
    }
    true
}

pub fn eval_deploy(k: f32, e: f32, r: f32, kernel: &DeployDecisionKernel) -> bool {
    let k_clamped = k.clamp(0.0, 1.0);
    let e_clamped = e.clamp(0.0, 1.0);
    let r_clamped = r.clamp(0.0, 1.0);

    if k_clamped < kernel.k_min {
        return false;
    }
    if e_clamped < kernel.e_min {
        return false;
    }
    if r_clamped > kernel.r_max {
        return false;
    }
    true
}

pub fn check_move(
    snapshot: &KerSnapshot,
    previous_vt: f32,
    rule: &SafeStepRule,
    kernel: &DeployDecisionKernel,
) -> DeployDecision {
    if !eval_safestep(previous_vt as f64, snapshot.vt as f64, rule) {
        return DeployDecision::Rejected {
            reason: "Lyapunov residual increased beyond epsilon or ceiling".to_string(),
        };
    }
    if !eval_deploy(snapshot.k, snapshot.e, snapshot.r, kernel) {
        return DeployDecision::Rejected {
            reason: "KER thresholds not satisfied for deploy kernel".to_string(),
        };
    }
    DeployDecision::Admissible
}
