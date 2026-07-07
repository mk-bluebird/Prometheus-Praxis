// Filename: crates/fog_router/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct QpuDataShard {
    pub node_id: String,
    pub vt_prev: f64,
    pub vt_next_est: f64,
    pub tailwind_valid: bool,
    pub biosurface_ok: bool,
    pub hydraulic_ok: bool,
    pub lyapunov_ok: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WorkloadDescriptor {
    pub workload_id: String,
    pub energy_req_j: f64,
    pub media_class: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum RouterDecision {
    Accept { node_id: String, projected_shard: QpuDataShard },
    Reject { reason: String },
    Reroute { suggested_node_id: String, reason: String },
}

fn all_predicates_hold(shard: &QpuDataShard) -> bool {
    shard.tailwind_valid &&
    shard.biosurface_ok &&
    shard.hydraulic_ok &&
    shard.lyapunov_ok &&
    shard.vt_next_est <= shard.vt_prev
}

fn evaluate_workload(
    shard: &QpuDataShard,
    workload: &WorkloadDescriptor,
    reroute_node_id: Option<String>,
) -> RouterDecision {
    if !all_predicates_hold(shard) {
        if let Some(node) = reroute_node_id {
            return RouterDecision::Reroute {
                suggested_node_id: node,
                reason: "predicate failure, rerouting workload".to_string(),
            };
        } else {
            return RouterDecision::Reject {
                reason: "predicate failure, reject workload".to_string(),
            };
        }
    }

    // Predicates hold; compute projected shard (vt_next_est already encoded).
    let projected = QpuDataShard {
        node_id: shard.node_id.clone(),
        vt_prev: shard.vt_prev,
        vt_next_est: shard.vt_next_est,
        tailwind_valid: shard.tailwind_valid,
        biosurface_ok: shard.biosurface_ok,
        hydraulic_ok: shard.hydraulic_ok,
        lyapunov_ok: shard.lyapunov_ok,
    };

    RouterDecision::Accept {
        node_id: shard.node_id.clone(),
        projected_shard: projected,
    }
}
