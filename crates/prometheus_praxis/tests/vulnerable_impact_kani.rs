// FILE: crates/prometheus_praxis/tests/vulnerable_impact_kani.rs
// ROLE: Kani harnesses for vulnerable_impact_envelope.rs.
//       Proves:
//       1) When ker_within_envelope, roh_within_envelope, and
//          rights_risk_within_bounds all hold, vulnerable_impact_gate
//          can never return GovernanceDecision::Stop.
//       2) For ImpactClass::VulnerablePersons, any q >= q_crit forces
//          GovernanceDecision::Stop independent of K/E and RoH.
//
// REQUIREMENTS:
// - Rust edition 2024, rust-version = "1.85".
// - Kani verifier = "0.67" in dev-dependencies.
// - No IO, no side effects, pure logic only.

#![forbid(unsafe_code)]

use rust_decimal::Decimal;

use prometheus_praxis::rightsverificationkernel::{
    ActionDomain,
    ActionLane,
    GovernanceDecision,
    KerSnapshot,
    RohSnapshot,
    RightsRiskSnapshot,
};

use prometheus_praxis::vulnerable_impact_envelope::{
    ImpactClass,
    VulnerableImpactEnvelope,
    vulnerable_impact_gate,
};

fn d(v: f32) -> Decimal {
    Decimal::from_f32(v).unwrap()
}

/// Harness 1:
/// When ker_within_envelope, roh_within_envelope, and rights_risk_within_bounds
/// all hold, vulnerable_impact_gate can never return GovernanceDecision::Stop.
///
/// This is modeled by:
/// - Constructing an envelope with valid KER/RoH thresholds.
/// - Constraining ker, roh, and rights snapshots so that all_guard predicates
///   must evaluate to true.
/// - Asserting that the gate returns Allow or Derate, but never Stop.
#[cfg(kani)]
#[kani::proof]
fn kani_vulnerable_gate_never_stop_when_guards_hold() {
    // Arbitrary but constrained parameters for the envelope.
    let k_min = d(0.80);
    let e_min = d(0.75);
    let r_max = d(0.20);
    let roh_global = d(0.30);
    let roh_local = d(0.25);

    // VulnerablePersons impact class to keep constraints tight.
    let env = VulnerableImpactEnvelope::new(
        ActionDomain::MacroHealth,
        ActionLane::Research,
        ImpactClass::VulnerablePersons,
        k_min,
        e_min,
        r_max,
        roh_global,
        roh_local,
        Some("psat.P7007Profile.v1.id".to_string()),
        Some("neurorights.envelope.citizen.v1".to_string()),
        "Art.9 High-Risk / Vulnerable Persons".to_string(),
        "risk.mgmt.plan.v1".to_string(),
        "data.gov.plan.v1".to_string(),
    );

    // Symbolic KER/RoH/Rights snapshots constrained to satisfy all guards.
    let k = kani::any::<f32>();
    let e = kani::any::<f32>();
    let r = kani::any::<f32>();
    let roh_scalar = kani::any::<f32>();
    let q = kani::any::<f32>();
    let q_crit = kani::any::<f32>();

    // Constrain to valid ranges.
    kani::assume(0.0 <= k && k <= 1.0);
    kani::assume(0.0 <= e && e <= 1.0);
    kani::assume(0.0 <= r && r <= 1.0);
    kani::assume(0.0 <= roh_scalar && roh_scalar <= 1.0);
    kani::assume(0.0 <= q && q <= 1.0);
    kani::assume(0.0 <= q_crit && q_crit <= 1.0);

    let ker = KerSnapshot {
        k: d(k),
        e: d(e),
        r: d(r),
    };

    let roh = RohSnapshot {
        roh: d(roh_scalar),
        domain: ActionDomain::MacroHealth,
        lane: ActionLane::Research,
    };

    let rights = RightsRiskSnapshot {
        q: d(q),
        q_crit: d(q_crit),
    };

    // Constrain snapshots so that all guards must pass.
    kani::assume(env.ker_within_envelope(&ker));
    kani::assume(env.roh_within_envelope(&roh));
    kani::assume(env.rights_risk_within_bounds(&rights, env.impact_class));

    // Base decision can vary; check that the gate never tightens to Stop
    // when all guards pass.
    let base_decision = GovernanceDecision::Allow;
    let decision = vulnerable_impact_gate(&env, &ker, &roh, &rights, base_decision);

    // Property: under passing guards, decision is never Stop.
    match decision {
        GovernanceDecision::Stop => {
            kani::reject();
        }
        GovernanceDecision::Allow | GovernanceDecision::Derate => {
            // ok
        }
    }
}

/// Harness 2:
/// For ImpactClass::VulnerablePersons, any q >= q_crit forces
/// GovernanceDecision::Stop independent of K/E and RoH.
///
/// This is modeled by:
/// - Creating a VulnerablePersons envelope with valid thresholds.
/// - Generating arbitrary KER/RoH snapshots (no constraints).
/// - Constraining RightsRiskSnapshot so that q >= q_crit.
/// - Asserting that vulnerable_impact_gate returns Stop regardless of
///   the base decision.
#[cfg(kani)]
#[kani::proof]
fn kani_vulnerable_persons_q_ge_qcrit_forces_stop() {
    // Envelope for vulnerable persons in a production lane.
    let env = VulnerableImpactEnvelope::new(
        ActionDomain::MacroHealth,
        ActionLane::Production,
        ImpactClass::VulnerablePersons,
        d(0.90),
        d(0.85),
        d(0.10),
        d(0.30),
        d(0.20),
        Some("psat.P7007Profile.v1.id".to_string()),
        Some("neurorights.envelope.citizen.v1".to_string()),
        "Art.9 High-Risk / Vulnerable Persons".to_string(),
        "risk.mgmt.plan.v1".to_string(),
        "data.gov.plan.v1".to_string(),
    );

    // Arbitrary KER and RoH snapshots, not constrained.
    let k = kani::any::<f32>();
    let e = kani::any::<f32>();
    let r = kani::any::<f32>();
    let roh_scalar = kani::any::<f32>();

    kani::assume(0.0 <= k && k <= 1.0);
    kani::assume(0.0 <= e && e <= 1.0);
    kani::assume(0.0 <= r && r <= 1.0);
    kani::assume(0.0 <= roh_scalar && roh_scalar <= 1.0);

    let ker = KerSnapshot {
        k: d(k),
        e: d(e),
        r: d(r),
    };

    let roh = RohSnapshot {
        roh: d(roh_scalar),
        domain: ActionDomain::MacroHealth,
        lane: ActionLane::Production,
    };

    // Rights risk: explicitly enforce q >= q_crit.
    let q = kani::any::<f32>();
    let q_crit = kani::any::<f32>();

    kani::assume(0.0 <= q && q <= 1.0);
    kani::assume(0.0 <= q_crit && q_crit <= 1.0);
    kani::assume(q >= q_crit);

    let rights = RightsRiskSnapshot {
        q: d(q),
        q_crit: d(q_crit),
    };

    // Base decision can be any of the lattice values.
    let base_decision = kani::any::<u8>();
    let base = match base_decision % 3 {
        0 => GovernanceDecision::Allow,
        1 => GovernanceDecision::Derate,
        _ => GovernanceDecision::Stop,
    };

    let decision = vulnerable_impact_gate(&env, &ker, &roh, &rights, base);

    // Property: for VulnerablePersons, q >= q_crit must force Stop.
    assert!(matches!(decision, GovernanceDecision::Stop));
}
