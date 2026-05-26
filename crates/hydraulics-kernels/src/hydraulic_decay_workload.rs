// hydraulics-kernels/src/hydraulic_decay_workload.rs

use ecosafety_core::non_actuating_workload::NonActuatingWorkload;
use ecosafety_core::risk_vector::RiskVector;        // type wrapping ecosafety.risk_vector.v2
use ecosafety_core::ker::KerWindow;                 // carries V_t, K, E, R, lane, ker_deployable
use ecosafety_core::lanes::Lane;                    // RESEARCH, EXPPROD, PROD
use ecosafety_core::plane_weights::PlaneWeights;    // wrapper over planeweights table

/// Input shard (projection of HydrologicalBufferPhoenix2026v1).
pub struct HydraulicDecayInput {
    pub contaminant_load: f64,
    pub residence_time_h: f64,
    pub flow_rate_m3s: f64,
    // other fields as needed...
}

/// Output = RiskVector + KER window.
pub struct HydraulicDecayOutput {
    pub risk_vector: RiskVector,
    pub ker_window: KerWindow,
}

impl crate::workload_invariant_tests::WorkloadResultView for HydraulicDecayOutput {
    fn risk_coords(&self) -> &[f64] {
        self.risk_vector.as_slice()
    }

    fn v_t(&self) -> f64 {
        self.ker_window.v_t
    }
}

/// Pure non‑actuating decay kernel.
pub struct HydraulicDecayWorkload {
    pub plane_weights: PlaneWeights,
    pub policy_id: String,
}

impl NonActuatingWorkload for HydraulicDecayWorkload {
    type Input = HydraulicDecayInput;
    type Output = HydraulicDecayOutput;

    fn execute(&self, input: Self::Input) -> Self::Output {
        // 1. Compute updated hydraulics risk coordinate (monotone in harmful axes).
        let r_hydraulics = crate::kernels::hydraulic_decay_risk(
            input.contaminant_load,
            input.residence_time_h,
            input.flow_rate_m3s,
        );

        // 2. Start with previous RiskVector or construct from inputs; here we build a
        //    minimal vector and set r_hydraulics, leaving other planes unchanged.
        let mut rv = RiskVector::zero();
        rv.set_r_hydraulics(r_hydraulics);

        // 3. Look up plane weights for current policy.
        let w = self.plane_weights.for_policy(&self.policy_id);

        // 4. Compute V_t = Σ_j w_j r_j².
        let v_t = ecosafety_core::residual::compute_v_t(&rv, &w);

        // 5. Compute K, E, R (E and R via standard patterns, K via evidence windows).
        let (k, e, r) = ecosafety_core::ker::compute_ker(&rv);

        // 6. Apply corridor clipping and safestep policies via shared kernel.
        let (rv_clipped, ker_window) =
            ecosafety_core::residual::apply_corridors_and_safestep(rv, v_t, k, e, r, Lane::Research);

        HydraulicDecayOutput {
            risk_vector: rv_clipped,
            ker_window,
        }
    }
}
