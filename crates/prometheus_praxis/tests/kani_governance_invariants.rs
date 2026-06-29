// FILE: crates/prometheus_praxis/tests/kani_governance_invariants.rs
// ROLE: Kani harnesses for Prometheus-Praxis governance invariants.
//       Prove: if K,E above lane thresholds and R below lane max, and
//       RoH/Lyapunov invariants hold, then governance cannot return Stop.

#![forbid(unsafe_code)]

use rust_decimal::Decimal;

use crate::praxis_governance_kernel::{
    ActionDomain,
    ActionLane,
    AlnShardId,
    GovernanceDecision,
    KerSnapshot,
    LyapunovResidualSnapshot,
    MacroActionContext,
    PraxisGovernanceConfig,
    PraxisGovernanceKernel,
    RohSnapshot,
    ROH_CEILING,
};

/// Helper: construct a Decimal in [0,1] from a Kani symbolic f32.
fn bounded01(x: f32) -> Decimal {
    let zero = Decimal::ZERO;
    let one = Decimal::ONE;
    let d = Decimal::from_f32(x).unwrap_or(zero);
    if d < zero {
        zero
    } else if d > one {
        one
    } else {
        d
    }
}

/// Kani proof: for RESEARCH lane, if K,E >= lane mins, R <= lane max,
/// RoH <= ROH_CEILING, and Lyapunov is non-increasing within epsilon,
/// then decision is not Stop.
#[kani::proof]
fn governance_does_not_stop_when_ker_roh_lyap_ok_research() {
    let cfg = PraxisGovernanceConfig::default();
    let kernel = PraxisGovernanceKernel::new(cfg.clone());

    // Symbolic KER values in [0,1].
    let k_val = bounded01(kani::any());
    let e_val = bounded01(kani::any());
    let r_val = bounded01(kani::any());

    // Assume K,E above lane minima and R below lane max for RESEARCH.
    kani::assume(k_val >= cfg.k_min_research);
    kani::assume(e_val >= cfg.e_min_research);
    kani::assume(r_val <= cfg.r_max_research);

    // Symbolic RoH within ceiling.
    let roh_val = bounded01(kani::any());
    kani::assume(roh_val <= ROH_CEILING);

    // Symbolic Lyapunov residuals with non-increasing constraint.
    let v_current = bounded01(kani::any());
    let epsilon = bounded01(kani::any());
    let v_next = bounded01(kani::any());

    // Enforce V_next <= V_current + epsilon.
    kani::assume(v_next <= v_current + epsilon);

    let ctx = MacroActionContext {
        action_id: "KANI-RESEARCH".to_string(),
        domain: ActionDomain::EcoRestoration,
        lane: ActionLane::Research,
        ker: KerSnapshot {
            k: k_val,
            e: e_val,
            r: r_val,
        },
        roh: RohSnapshot {
            roh: roh_val,
            domain: ActionDomain::EcoRestoration,
            lane: ActionLane::Research,
        },
        lyapunov: LyapunovResidualSnapshot {
            v_current,
            v_next,
            epsilon,
        },
        aln_envelope: AlnShardId {
            name: "ecosafety.corridor.v1".to_string(),
            version: "1.0.0".to_string(),
        },
    };

    let (decision, _shard) = kernel.validate_eco_action(ctx);
    // Invariant: under these assumptions, decision != Stop.
    assert!(decision != GovernanceDecision::Stop);
}

/// Kani proof: same invariant for PILOT lane.
#[kani::proof]
fn governance_does_not_stop_when_ker_roh_lyap_ok_pilot() {
    let cfg = PraxisGovernanceConfig::default();
    let kernel = PraxisGovernanceKernel::new(cfg.clone());

    let k_val = bounded01(kani::any());
    let e_val = bounded01(kani::any());
    let r_val = bounded01(kani::any());

    kani::assume(k_val >= cfg.k_min_pilot);
    kani::assume(e_val >= cfg.e_min_pilot);
    kani::assume(r_val <= cfg.r_max_pilot);

    let roh_val = bounded01(kani::any());
    kani::assume(roh_val <= ROH_CEILING);

    let v_current = bounded01(kani::any());
    let epsilon = bounded01(kani::any());
    let v_next = bounded01(kani::any());
    kani::assume(v_next <= v_current + epsilon);

    let ctx = MacroActionContext {
        action_id: "KANI-PILOT".to_string(),
        domain: ActionDomain::CityOperations,
        lane: ActionLane::Pilot,
        ker: KerSnapshot {
            k: k_val,
            e: e_val,
            r: r_val,
        },
        roh: RohSnapshot {
            roh: roh_val,
            domain: ActionDomain::CityOperations,
            lane: ActionLane::Pilot,
        },
        lyapunov: LyapunovResidualSnapshot {
            v_current,
            v_next,
            epsilon,
        },
        aln_envelope: AlnShardId {
            name: "city.corridor.v1".to_string(),
            version: "1.0.0".to_string(),
        },
    };

    let (decision, _shard) = kernel.validate_city_action(ctx);
    assert!(decision != GovernanceDecision::Stop);
}

/// Kani proof: same invariant for PRODUCTION lane.
#[kani::proof]
fn governance_does_not_stop_when_ker_roh_lyap_ok_production() {
    let cfg = PraxisGovernanceConfig::default();
    let kernel = PraxisGovernanceKernel::new(cfg.clone());

    let k_val = bounded01(kani::any());
    let e_val = bounded01(kani::any());
    let r_val = bounded01(kani::any());

    kani::assume(k_val >= cfg.k_min_production);
    kani::assume(e_val >= cfg.e_min_production);
    kani::assume(r_val <= cfg.r_max_production);

    let roh_val = bounded01(kani::any());
    kani::assume(roh_val <= ROH_CEILING);

    let v_current = bounded01(kani::any());
    let epsilon = bounded01(kani::any());
    let v_next = bounded01(kani::any());
    kani::assume(v_next <= v_current + epsilon);

    let ctx = MacroActionContext {
        action_id: "KANI-PRODUCTION".to_string(),
        domain: ActionDomain::MacroHealth,
        lane: ActionLane::Production,
        ker: KerSnapshot {
            k: k_val,
            e: e_val,
            r: r_val,
        },
        roh: RohSnapshot {
            roh: roh_val,
            domain: ActionDomain::MacroHealth,
            lane: ActionLane::Production,
        },
        lyapunov: LyapunovResidualSnapshot {
            v_current,
            v_next,
            epsilon,
        },
        aln_envelope: AlnShardId {
            name: "macrohealth.corridor.v1".to_string(),
            version: "1.0.0".to_string(),
        },
    };

    let (decision, _shard) = kernel.validate_health_action(ctx);
    assert!(decision != GovernanceDecision::Stop);
}
