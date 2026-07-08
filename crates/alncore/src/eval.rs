// filename: crates/alncore/src/eval.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use crate::model::{DeployDecisionKernel, KerSnapshot, Lane, SafeStepRule};

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
    // Lane-scope awareness: only apply kernels where lanescope matches snapshot.lane
    let lane_matches = lane_scope_matches(&snapshot.lane, &kernel.lane_scope);
    if !lane_matches {
        // Kernel doesn't apply to this lane - consider it admissible by default
        return DeployDecision::Admissible;
    }

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

fn lane_scope_matches(snapshot_lane: &Lane, lane_scope: &str) -> bool {
    let scope_upper = lane_scope.to_uppercase();
    match snapshot_lane {
        Lane::Research => scope_upper == "RESEARCH" || scope_upper == "ALL" || scope_upper.is_empty(),
        Lane::Exp => scope_upper == "EXP" || scope_upper == "EXPPROD" || scope_upper == "ALL" || scope_upper.is_empty(),
        Lane::Sim => scope_upper == "SIM" || scope_upper == "ALL" || scope_upper.is_empty(),
        Lane::Prod => scope_upper == "PROD" || scope_upper == "EXPPROD" || scope_upper == "ALL" || scope_upper.is_empty(),
    }
}

/// Returns a concise text explaining which thresholds were violated or satisfied.
pub fn explain_deploy(snapshot: &KerSnapshot, kernel: &DeployDecisionKernel) -> String {
    let mut explanations = Vec::new();

    let k_ok = snapshot.k_clamped() >= kernel.k_min;
    let e_ok = snapshot.e_clamped() >= kernel.e_min;
    let r_ok = snapshot.r_clamped() <= kernel.r_max;

    if k_ok {
        explanations.push(format!(
            "✓ K={:.3} meets minimum {:.3}",
            snapshot.k_clamped(),
            kernel.k_min
        ));
    } else {
        explanations.push(format!(
            "✗ K={:.3} below minimum {:.3}",
            snapshot.k_clamped(),
            kernel.k_min
        ));
    }

    if e_ok {
        explanations.push(format!(
            "✓ E={:.3} meets minimum {:.3}",
            snapshot.e_clamped(),
            kernel.e_min
        ));
    } else {
        explanations.push(format!(
            "✗ E={:.3} below minimum {:.3}",
            snapshot.e_clamped(),
            kernel.e_min
        ));
    }

    if r_ok {
        explanations.push(format!(
            "✓ R={:.3} within maximum {:.3}",
            snapshot.r_clamped(),
            kernel.r_max
        ));
    } else {
        explanations.push(format!(
            "✗ R={:.3} exceeds maximum {:.3}",
            snapshot.r_clamped(),
            kernel.r_max
        ));
    }

    let lane_match = lane_scope_matches(&snapshot.lane, &kernel.lane_scope);
    if lane_match {
        explanations.push(format!(
            "✓ Lane {:?} matches kernel scope '{}'",
            snapshot.lane, kernel.lane_scope
        ));
    } else {
        explanations.push(format!(
            "✗ Lane {:?} does not match kernel scope '{}'",
            snapshot.lane, kernel.lane_scope
        ));
    }

    if k_ok && e_ok && r_ok && lane_match {
        format!(
            "DeployDecisionKernel '{}': ADMISSIBLE\n  {}",
            kernel.kernel_id,
            explanations.join("\n  ")
        )
    } else {
        format!(
            "DeployDecisionKernel '{}': REJECTED\n  {}",
            kernel.kernel_id,
            explanations.join("\n  ")
        )
    }
}
