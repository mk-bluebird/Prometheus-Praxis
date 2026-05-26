// hydraulics-kernels/tests/hydraulic_decay_invariants.rs

use hydraulics_kernels::hydraulic_decay_workload::{
    HydraulicDecayInput,
    HydraulicDecayWorkload,
};
use ecosafety_core::plane_weights::PlaneWeights;
use ecosafety_core::workload_invariant_tests::{
    HarmAxisConfig,
    assert_boundedness,
    assert_monotonicity,
};

#[test]
fn hydraulic_decay_bounded_and_monotone() {
    let plane_weights = PlaneWeights::load_from_sqlite("test_planeweights.db").unwrap();
    let workload = HydraulicDecayWorkload {
        plane_weights,
        policy_id: "Phoenix-CentralAZ-2026".to_string(),
    };

    let axis = HarmAxisConfig {
        name: "contaminant_load",
        values_increasing: vec![0.0, 0.25, 0.5, 0.75, 1.0],
    };

    let make_input = |load: &f64| HydraulicDecayInput {
        contaminant_load: *load,
        residence_time_h: 12.0,
        flow_rate_m3s: 1.0,
    };

    assert_monotonicity(&workload, &axis, make_input);

    let mid = make_input(&0.5);
    let out = workload.execute(mid);
    assert_boundedness(&out);
}
