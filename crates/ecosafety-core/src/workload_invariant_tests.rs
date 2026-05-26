// ecosafety-core/src/workload_invariant_tests.rs

use crate::non_actuating_workload::NonActuatingWorkload;

/// Harmful‑axis sweep specification used in invariant tests.
pub struct HarmAxisConfig<T> {
    pub name: &'static str,
    pub values_increasing: Vec<T>,
}

/// View over a workload result used for invariants.
pub trait WorkloadResultView {
    /// Normalized risk coordinates slice r_j ∈ [0,1].
    fn risk_coords(&self) -> &[f64];
    /// Lyapunov residual V_t.
    fn v_t(&self) -> f64;
}

/// Boundedness: ∀j, r_j ∈ [0,1].
pub fn assert_boundedness<R: WorkloadResultView>(result: &R) {
    for (idx, r_j) in result.risk_coords().iter().enumerate() {
        assert!(
            *r_j >= 0.0 && *r_j <= 1.0,
            "risk coordinate out of bounds: idx={}, r_j={}",
            idx,
            r_j
        );
    }
}

/// Monotonicity: along each harmful axis, V_t must be non‑decreasing.
pub fn assert_monotonicity<W, F, T>(workload: &W, axis: &HarmAxisConfig<T>, make_input: F)
where
    W: NonActuatingWorkload,
    W::Output: WorkloadResultView,
    F: Fn(&T) -> W::Input,
{
    let mut last_vt: Option<f64> = None;

    for val in axis.values_increasing.iter() {
        let input = make_input(val);
        let out = workload.execute(input);
        let vt = out.v_t();

        if let Some(prev) = last_vt {
            assert!(
                vt >= prev,
                "monotonicity violated on axis {}: V_t decreased ({} -> {})",
                axis.name,
                prev,
                vt
            );
        }

        last_vt = Some(vt);
    }
}
