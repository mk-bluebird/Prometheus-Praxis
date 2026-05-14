// filename src/kerresidual.rs
// destination EcoNet/src/kerresidual.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeight {
    pub planeid: String,
    pub weight: f32,
    pub nonoffsettable: bool,
    pub softband: f32,
    pub hardband: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneSample {
    pub planeid: String,
    pub value: f32, // r_j in [0,1]
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RiskVector {
    pub planes: Vec<PlaneSample>,
    pub rtopology: Option<f32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovWeights {
    pub planes: Vec<PlaneWeight>,
    pub w_topology: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum SafestepVerdict {
    Ok,
    LyapunovIncrease { v_prev: f32, v_next: f32 },
    NonOffsettableViolation {
        planeid: String,
        r_prev: f32,
        r_next: f32,
    },
    TopologyViolation {
        rtop_prev: f32,
        rtop_next: f32,
    },
}

pub fn compute_residual(r: &RiskVector, w: &LyapunovWeights) -> f32 {
    let mut v = 0.0_f32;

    for pw in &w.planes {
        if let Some(ps) = r.planes.iter().find(|p| p.planeid == pw.planeid) {
            let rj = ps.value.max(0.0).min(1.0);
            v += pw.weight * rj * rj;
        }
    }

    if let Some(rt) = r.rtopology {
        let rt_clamped = rt.max(0.0).min(1.0);
        v += w.w_topology * rt_clamped * rt_clamped;
    }

    v
}

pub fn check_safestep(
    r_prev: &RiskVector,
    r_next: &RiskVector,
    w: &LyapunovWeights,
) -> SafestepVerdict {
    let v_prev = compute_residual(r_prev, w);
    let v_next = compute_residual(r_next, w);

    if v_next > v_prev {
        return SafestepVerdict::LyapunovIncrease {
            v_prev,
            v_next,
        };
    }

    for pw in &w.planes {
        if !pw.nonoffsettable {
            continue;
        }

        let prev = r_prev
            .planes
            .iter()
            .find(|p| p.planeid == pw.planeid)
            .map(|p| p.value)
            .unwrap_or(0.0);
        let next = r_next
            .planes
            .iter()
            .find(|p| p.planeid == pw.planeid)
            .map(|p| p.value)
            .unwrap_or(0.0);

        let r_prev = prev.max(0.0).min(1.0);
        let r_next = next.max(0.0).min(1.0);

        if r_next > r_prev {
            return SafestepVerdict::NonOffsettableViolation {
                planeid: pw.planeid.clone(),
                r_prev,
                r_next,
            };
        }
    }

    if let (Some(rt_prev), Some(rt_next)) = (r_prev.rtopology, r_next.rtopology) {
        if rt_next > rt_prev {
            return SafestepVerdict::TopologyViolation {
                rtop_prev: rt_prev,
                rtop_next: rt_next,
            };
        }
    }

    SafestepVerdict::Ok
}
