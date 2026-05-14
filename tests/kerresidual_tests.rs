// filename tests/kerresidual_tests.rs
// destination EcoNet/tests/kerresidual_tests.rs

use crate::kerresidual::{
    compute_residual, check_safestep, LyapunovWeights, PlaneSample, PlaneWeight, RiskVector,
    SafestepVerdict,
};

fn make_default_weights() -> LyapunovWeights {
    LyapunovWeights {
        planes: vec![
            PlaneWeight {
                planeid: "energy".to_string(),
                weight: 0.7,
                nonoffsettable: false,
                softband: 0.10,
                hardband: 0.20,
            },
            PlaneWeight {
                planeid: "hydrologyMAR".to_string(),
                weight: 1.0,
                nonoffsettable: true,
                softband: 0.10,
                hardband: 0.13,
            },
            PlaneWeight {
                planeid: "biodiversity".to_string(),
                weight: 1.0,
                nonoffsettable: true,
                softband: 0.10,
                hardband: 0.13,
            },
        ],
        w_topology: 0.5,
    }
}

#[test]
fn test_residual_monotone_when_all_planes_improve() {
    let w = make_default_weights();

    let r_prev = RiskVector {
        planes: vec![
            PlaneSample {
                planeid: "energy".to_string(),
                value: 0.3,
            },
            PlaneSample {
                planeid: "hydrologyMAR".to_string(),
                value: 0.2,
            },
            PlaneSample {
                planeid: "biodiversity".to_string(),
                value: 0.2,
            },
        ],
        rtopology: Some(0.1),
    };

    let r_next = RiskVector {
        planes: vec![
            PlaneSample {
                planeid: "energy".to_string(),
                value: 0.2,
            },
            PlaneSample {
                planeid: "hydrologyMAR".to_string(),
                value: 0.1,
            },
            PlaneSample {
                planeid: "biodiversity".to_string(),
                value: 0.1,
            },
        ],
        rtopology: Some(0.05),
    };

    let v_prev = compute_residual(&r_prev, &w);
    let v_next = compute_residual(&r_next, &w);
    assert!(v_next <= v_prev);

    let verdict = check_safestep(&r_prev, &r_next, &w);
    match verdict {
        SafestepVerdict::Ok => {}
        _ => panic!("expected Ok verdict"),
    }
}

#[test]
fn test_nonoffsettable_violation_detected() {
    let w = make_default_weights();

    let r_prev = RiskVector {
        planes: vec![PlaneSample {
            planeid: "hydrologyMAR".to_string(),
            value: 0.2,
        }],
        rtopology: None,
    };

    let r_next = RiskVector {
        planes: vec![PlaneSample {
            planeid: "hydrologyMAR".to_string(),
            value: 0.3,
        }],
        rtopology: None,
    };

    let verdict = check_safestep(&r_prev, &r_next, &w);
    match verdict {
        SafestepVerdict::NonOffsettableViolation { planeid, .. } => {
            assert_eq!(planeid, "hydrologyMAR");
        }
        _ => panic!("expected NonOffsettableViolation"),
    }
}

#[test]
fn test_topology_violation_detected() {
    let w = make_default_weights();

    let r_prev = RiskVector {
        planes: Vec::new(),
        rtopology: Some(0.2),
    };

    let r_next = RiskVector {
        planes: Vec::new(),
        rtopology: Some(0.3),
    };

    let verdict = check_safestep(&r_prev, &r_next, &w);
    match verdict {
        SafestepVerdict::TopologyViolation { .. } => {}
        _ => panic!("expected TopologyViolation"),
    }
}
