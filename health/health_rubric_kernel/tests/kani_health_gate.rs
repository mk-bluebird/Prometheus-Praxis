// Prometheus-Praxis/rust/health/health_rubric_kernel/tests/kani_health_gate.rs
#![allow(dead_code)]
#![feature(kani)]

use health_rubric_kernel::{check_health_gate, GateVerdict, HealthRubricEnvelope, ImmuneStatus};

#[kani::proof]
fn health_gate_stops_on_roh_ceiling_violation() {
    let mut env = kani::any::<HealthRubricEnvelope>();

    // Force RoH ceiling violation
    env.rohscalarnorm = 0.31;
    env.thermalburdennorm = 0.10;
    env.immune_status = ImmuneStatus::Green;
    env.psychcontinuitypressure = 0.10;
    env.neurorights_ok = true;
    env.consent_ok = true;
    env.governance_ok = true;
    env.continuity_grade = 'A';

    let verdict = check_health_gate(&env);
    kani::assert!(matches!(verdict, GateVerdict::Stop));
}

#[kani::proof]
fn health_gate_stops_on_thermal_ceiling_violation() {
    let mut env = kani::any::<HealthRubricEnvelope>();

    env.rohscalarnorm = 0.10;
    env.thermalburdennorm = 0.31;
    env.immune_status = ImmuneStatus::Green;
    env.psychcontinuitypressure = 0.10;
    env.neurorights_ok = true;
    env.consent_ok = true;
    env.governance_ok = true;
    env.continuity_grade = 'A';

    let verdict = check_health_gate(&env);
    kani::assert!(matches!(verdict, GateVerdict::Stop));
}

#[kani::proof]
fn health_gate_stops_on_neurorights_or_consent_failure() {
    let mut env = kani::any::<HealthRubricEnvelope>();

    env.rohscalarnorm = 0.10;
    env.thermalburdennorm = 0.10;
    env.immune_status = ImmuneStatus::Green;
    env.psychcontinuitypressure = 0.10;
    env.neurorights_ok = false; // violation
    env.consent_ok = true;
    env.governance_ok = true;
    env.continuity_grade = 'A';

    let verdict = check_health_gate(&env);
    kani::assert!(matches!(verdict, GateVerdict::Stop));

    env.neurorights_ok = true;
    env.consent_ok = false; // consent violation

    let verdict2 = check_health_gate(&env);
    kani::assert!(matches!(verdict2, GateVerdict::Stop));
}

#[kani::proof]
fn health_gate_allows_or_derates_only_when_all_invariants_hold() {
    let mut env = kani::any::<HealthRubricEnvelope>();

    env.rohscalarnorm = 0.10;
    env.thermalburdennorm = 0.10;
    env.immune_status = ImmuneStatus::Green;
    env.psychcontinuitypressure = 0.20;
    env.neurorights_ok = true;
    env.consent_ok = true;
    env.governance_ok = true;

    // Continuity grade A/B => Allow
    env.continuity_grade = 'A';
    let verdict_a = check_health_gate(&env);
    kani::assert!(matches!(verdict_a, GateVerdict::Allow));

    env.continuity_grade = 'C';
    let verdict_c = check_health_gate(&env);
    kani::assert!(matches!(verdict_c, GateVerdict::Derate));
}
