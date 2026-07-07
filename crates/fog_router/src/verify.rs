// Filename: crates/fog_router/src/verify.rs
#![allow(dead_code)]
use kani::proof;
use crate::{QpuDataShard, WorkloadDescriptor, RouterDecision, evaluate_workload};

fn sample_shard(preds_ok: bool) -> QpuDataShard {
    QpuDataShard {
        node_id: "vault-001".to_string(),
        vt_prev: 0.10,
        vt_next_est: if preds_ok { 0.09 } else { 0.11 },
        tailwind_valid: preds_ok,
        biosurface_ok: preds_ok,
        hydraulic_ok: preds_ok,
        lyapunov_ok: preds_ok,
    }
}

fn sample_workload() -> WorkloadDescriptor {
    WorkloadDescriptor {
        workload_id: "wl-001".to_string(),
        energy_req_j: 1000.0,
        media_class: "water".to_string(),
    }
}

#[proof]
fn no_accept_if_any_predicate_fails() {
    let shard_bad = sample_shard(false);
    let wl = sample_workload();

    let decision = evaluate_workload(&shard_bad, &wl, None);

    match decision {
        RouterDecision::Accept { .. } => {
            kani::assert!(false, "Accept must not occur when predicates fail");
        }
        RouterDecision::Reject { .. } => {
            kani::assert!(true);
        }
        RouterDecision::Reroute { .. } => {
            kani::assert!(true);
        }
    }
}

#[proof]
fn accept_only_when_predicates_hold() {
    let shard_good = sample_shard(true);
    let wl = sample_workload();

    let decision = evaluate_workload(&shard_good, &wl, None);

    match decision {
        RouterDecision::Accept { .. } => {
            kani::assert!(true);
        }
        RouterDecision::Reject { .. } | RouterDecision::Reroute { .. } => {
            kani::assert!(false, "Accept must occur when all predicates hold");
        }
    }
}
