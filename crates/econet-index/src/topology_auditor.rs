// econet-index/src/topology_auditor.rs

use ecosafety_core::non_actuating_workload::{NonActuatingGovernanceKernel, NonActuatingWorkload};
use ecosafety_core::risk_vector::RiskVector;
use ecosafety_core::ker::KerWindow;
use ecosafety_core::lanes::Lane;
use ecosafety_core::workload_invariant_tests::WorkloadResultView;

/// Raw input from CI manifest scans.
pub struct TopologyAuditInput {
    pub n_mislabel: u32,
    pub n_missing_manifest: u32,
    pub w_mislabel: f64,
    pub w_missing: f64,
    pub plane_weight_topology: f64,
}

pub struct TopologyAuditOutput {
    pub risk_vector: RiskVector,
    pub ker_window: KerWindow,
}

impl WorkloadResultView for TopologyAuditOutput {
    fn risk_coords(&self) -> &[f64] {
        self.risk_vector.as_slice()
    }

    fn v_t(&self) -> f64 {
        self.ker_window.v_t
    }
}

pub struct TopologyAuditor;

impl NonActuatingWorkload for TopologyAuditor {
    type Input = TopologyAuditInput;
    type Output = TopologyAuditOutput;

    fn execute(&self, input: Self::Input) -> Self::Output {
        let i_topology =
            input.w_mislabel * input.n_mislabel as f64 + input.w_missing * input.n_missing_manifest as f64;

        let r_topology = normalize_i_topology(i_topology);

        let mut rv = RiskVector::zero();
        rv.set_r_topology(r_topology);

        let v_t = compute_v_t_from_rv(&rv, input.plane_weight_topology);
        let (k, e, r) = compute_ker_from_rv(&rv);

        let ker_window = KerWindow {
            v_t,
            k_metric: k,
            e_metric: e,
            r_metric: r,
            lane: Lane::Research,
            ker_deployable: false,
        };

        TopologyAuditOutput { risk_vector: rv, ker_window }
    }
}

impl NonActuatingGovernanceKernel for TopologyAuditor {}

fn normalize_i_topology(i_topology: f64) -> f64 {
    let safe = 0.0;
    let gold = 5.0;
    let hard = 20.0;

    if i_topology <= safe {
        0.0
    } else if i_topology <= gold {
        (i_topology - safe) / (gold - safe) * 0.33
    } else if i_topology <= hard {
        0.33 + (i_topology - gold) / (hard - gold) * 0.34
    } else {
        1.0
    }
}

fn compute_v_t_from_rv(rv: &RiskVector, w_topology: f64) -> f64 {
    let r_topology = rv.r_topology();
    w_topology * r_topology * r_topology
}

fn compute_ker_from_rv(rv: &RiskVector) -> (f64, f64, f64) {
    let r = rv.as_slice().iter().cloned().fold(0.0_f64, |acc, x| acc.max(x));
    let e = 1.0 - r;
    let k = 1.0;
    (k, e, r)
}
