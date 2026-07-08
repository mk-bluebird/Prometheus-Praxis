
#![forbid(unsafe_code)]

use kani::proof;

use rust_decimal::Decimal;

use prometheus_praxis_rights_verification_kernel::{
    ActionDomain,
    ActionLane,
    GovernanceDecision,
    KerSnapshot,
    RohSnapshot,
    RightsRiskSnapshot,
};

use prometheus_praxis_vulnerable_impact_envelope::{
    ImpactClass,
    VulnerableImpactEnvelope,
    vulnerable_impact_gate,
};

fn d(v: f32) -> Decimal {
    Decimal::from_f32(v).expect("constant decimal")
}

/// Helper: synthesize a structurally valid envelope over the full
/// Cartesian product of domains, lanes, and impact classes.
fn mk_envelope(
    domain: ActionDomain,
    lane: ActionLane,
    impact: ImpactClass,
) -> VulnerableImpactEnvelope {
    // Conservative but non-degenerate thresholds:
    // K,E high, R below RoH ceilings, RoH ceilings within global 0.30.
    let kmin = d(0.80);
    let emin = d(0.75);
    let rmax = d(0.20);
    let roh_global = d(0.30);
    let roh_local  = d(0.25);

    let psat_id = match impact {
        ImpactClass::None => None,
        _ => Some("psat.P7007Profile.v1.id".to_string()),
    };

    let nr_env = match impact {
        ImpactClass::None => None,
        _ => Some("neurorights.envelope.citizen.v1".to_string()),
    };

    VulnerableImpactEnvelope::new(
        domain,
        lane,
        impact,
        kmin,
        emin,
        rmax,
        roh_global,
        roh_local,
        psat_id,
        nr_env,
        "Art.9 Vulnerable Persons / Neuro / Health".to_string(),
        "risk.mgmt.plan.v1".to_string(),
        "data.gov.plan.v1".to_string(),
    )
}

/// Helper: pick an arbitrary domain over the full ActionDomain enum.
fn any_domain(code: u8) -> ActionDomain {
    match code % 5 {
        0 => ActionDomain::EcoRestoration,
        1 => ActionDomain::CityOperations,
        2 => ActionDomain::MacroHealth,
        3 => ActionDomain::Cybernetics,
        _ => ActionDomain::PaymentGovernance,
    }
}

/// Helper: pick an arbitrary lane (3 production-relevant lanes).
fn any_lane(code: u8) -> ActionLane {
    match code % 3 {
        0 => ActionLane::Research,
        1 => ActionLane::Pilot,
        _ => ActionLane::Production,
    }
}

/// Helper: pick any of the four impact classes.
fn any_impact_class(code: u8) -> ImpactClass {
    match code % 4 {
        0 => ImpactClass::None,
        1 => ImpactClass::VulnerablePersons,
        2 => ImpactClass::HealthCritical,
        _ => ImpactClass::NeuroSensitive,
    }
}

/// Core harness: for all admissible envelopes and KER/RoH/Rights snapshots
/// where all guards hold, vulnerable_impact_gate never returns Stop.
/// This ranges over:
/// - all 4 ImpactClass values,
/// - all 5 ActionDomain values,
/// - all 3 ActionLane values,
/// - all admissible K,E,R,RoH,q,q_crit in [0,1] constrained by guards.
#[proof]
fn kani_vulnerable_impact_gate_never_stop_when_guards_hold() {
    // Symbolic selectors for domain, lane, impact class.
    let dom_sel: u8 = kani::any();
    let lane_sel: u8 = kani::any();
    let ic_sel:  u8 = kani::any();

    let domain = any_domain(dom_sel);
    let lane   = any_lane(lane_sel);
    let impact = any_impact_class(ic_sel);

    let env = mk_envelope(domain, lane, impact);

    // Symbolic KER, RoH, Rights scalars in 0..=1.
    let k: f32      = kani::any();
    let e: f32      = kani::any();
    let r: f32      = kani::any();
    let roh_scalar: f32 = kani::any();
    let q: f32      = kani::any();
    let qcrit: f32  = kani::any();

    kani::assume(0.0 <= k && k <= 1.0);
    kani::assume(0.0 <= e && e <= 1.0);
    kani::assume(0.0 <= r && r <= 1.0);
    kani::assume(0.0 <= roh_scalar && roh_scalar <= 1.0);
    kani::assume(0.0 <= q && q <= 1.0);
    kani::assume(0.0 <= qcrit && qcrit <= 1.0);

    let ker = KerSnapshot {
        k: d(k),
        e: d(e),
        r: d(r),
    };

    let roh = RohSnapshot {
        roh: d(roh_scalar),
        domain,
        lane,
    };

    let rights = RightsRiskSnapshot {
        q: d(q),
        qcrit: d(qcrit),
    };

    // Harness-side constraint: we are only interested in the subspace
    // where *all* guard predicates hold.
    kani::assume(env.ker_within_envelope(ker));
    kani::assume(env.roh_within_envelope(roh));
    kani::assume(env.rightsrisk_within_bounds(rights, impact));
    kani::assume(env.all_guards_pass(ker, roh, rights));

    // Base decision is allowed to vary over the lattice.
    let base_sel: u8 = kani::any();
    let base = match base_sel % 3 {
        0 => GovernanceDecision::
