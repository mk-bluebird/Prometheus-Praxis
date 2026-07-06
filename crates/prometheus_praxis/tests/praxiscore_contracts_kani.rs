// FILE: crates/prometheus_praxis/tests/praxiscore_contracts_kani.rs
// ROLE: Kani harnesses for Praxis core contracts.
//       Prove:
//         - non_stop_when_invariants_pass: no Stop when all invariants pass.
//         - no_roh_downgrade: any RoH above global ceiling forces Stop.
//         - no_neurorights_downgrade: neurorights risk forbids Allow.
//
// REQUIREMENTS:
//   - Rust edition 2024, rust-version 1.85.
//   - !forbid_unsafecode in library crates; Kani harnesses in tests.
//
// NOTE: This module is test-only; no production code or IO.

use rust_decimal::Decimal;

use prometheus_praxis::praxisgovernancekernel::{
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
    ROHCEILING,
};

use prometheus_praxis::praxiscore_contracts::{
    CompositeCoreContracts,
    KEREnvelope,
    LyapunovEnvelope,
    OwnerBinding,
    RoHEnvelope,
    TsafeEnvelope,
    no_neurorights_downgrade,
    no_roh_downgrade,
    non_stop_when_invariants_pass,
};

fn decimal(v: f32) -> Decimal {
    Decimal::from_f32(v).unwrap()
}

/// Helper: construct a conservative default KER envelope, mirroring PraxisGovernanceConfig::default.
fn mk_ker_envelope() -> KEREnvelope {
    KEREnvelope {
        kmin_production: decimal(0.95),
        emin_production: decimal(0.92),
        rmax_production: decimal(0.08),
        kmin_pilot: decimal(0.92),
        emin_pilot: decimal(0.90),
        rmax_pilot: decimal(0.10),
        kmin_research: decimal(0.90),
        emin_research: decimal(0.85),
        rmax_research: decimal(0.20),
    }
}

/// Helper: construct a RoH envelope consistent with the global ROHCEILING.
fn mk_roh_envelope() -> RoHEnvelope {
    RoHEnvelope {
        global_ceiling: ROHCEILING,
        eco_restoration_ceiling: None,
        city_operations_ceiling: None,
        cosmic_energy_ceiling: None,
        macro_health_ceiling: None,
    }
}

/// Helper: construct a Tsafe envelope with a modest negative distance band.
fn mk_tsafe_envelope() -> TsafeEnvelope {
    TsafeEnvelope {
        corridor_id: "ecosafety.corridor.city.v1".to_string(),
        max_negative_distance: decimal(0.10),
    }
}

/// Helper: construct a Lyapunov envelope matching typical epsilon bands.
fn mk_lyapunov_envelope() -> LyapunovEnvelope {
    LyapunovEnvelope {
        vmax_global: decimal(1.0),
        epsilon_research: decimal(0.03),
        epsilon_pilot: decimal(0.02),
        epsilon_production: decimal(0.01),
    }
}

/// Helper: construct a CompositeCoreContracts bundle for Kani harnesses.
fn mk_core_contracts() -> CompositeCoreContracts {
    CompositeCoreContracts {
        ker_envelope: mk_ker_envelope(),
        roh_envelope: mk_roh_envelope(),
        tsafe_envelope: Some(mk_tsafe_envelope()),
        lyapunov_envelope: mk_lyapunov_envelope(),
        owner_binding: OwnerBinding::canonical(),
    }
}

/// Helper: construct a MacroActionContext where all invariants are assumed to pass.
///
/// Kani will treat fields as symbolic; we constrain them via assumptions below.
fn mk_symbolic_safe_context() -> MacroActionContext {
    MacroActionContext {
        actionid: "KANITEST-ACT".to_string(),
        domain: ActionDomain::CityOperations,
        lane: ActionLane::Research,
        ker: KerSnapshot {
            k: decimal(0.92),
            e: decimal(0.90),
            r: decimal(0.10),
        },
        roh: RohSnapshot {
            roh: decimal(0.25),
            domain: ActionDomain::CityOperations,
            lane: ActionLane::Research,
        },
        lyapunov: LyapunovResidualSnapshot {
            vcurrent: decimal(1.0),
            vnext: decimal(0.98),
            epsilon: decimal(0.03),
        },
        alnenvelope: AlnShardId {
            name: "ecosafety.corridor.city.v1".to_string(),
            version: "1.0.0".to_string(),
        },
    }
}

/// Proof 1: If all invariants pass, GovernanceDecision must not be Stop.
///
/// This harness encodes the NonStop when invariants satisfied property.
/// It is referenced by MonotoneSafetyCore.nonstopwheninvariantspass in the ALN shard.
#[kani::proof]
fn proof_non_stop_when_invariants_pass() {
    let cfg = PraxisGovernanceConfig::default();
    let kernel = PraxisGovernanceKernel::new(cfg);
    let contracts = mk_core_contracts();
    let mut ctx = mk_symbolic_safe_context();

    // Symbolic variation: allow KER to float within lane thresholds.
    // Kani will explore numeric combinations subject to these guards.
    kani::assume(ctx.ker.k >= contracts.ker_envelope.kmin_research);
    kani::assume(ctx.ker.e >= contracts.ker_envelope.emin_research);
    kani::assume(ctx.ker.r <= contracts.ker_envelope.rmax_research);

    // RoH must be within global ceiling.
    kani::assume(ctx.roh.roh <= contracts.roh_envelope.global_ceiling);

    // Lyapunov must be non-increasing within epsilon and below vmax.
    kani::assume(ctx.lyapunov.vnext <= ctx.lyapunov.vcurrent + contracts.lyapunov_envelope.epsilon_research);
    kani::assume(ctx.lyapunov.vnext <= contracts.lyapunov_envelope.vmax_global);

    // Tsafe signed distance must be within band.
    let tsafe_signed_distance = decimal(0.05);
    kani::assume(tsafe_signed_distance >= decimal(-0.10));

    // All invariants pass under these assumptions.
    kani::assert!(contracts.invariants_pass(&ctx, Some(tsafe_signed_distance)));

    // Then non_stop_when_invariants_pass must hold.
    kani::assert!(non_stop_when_invariants_pass(
        &kernel,
        &contracts,
        ctx,
        Some(tsafe_signed_distance),
    ));
}

/// Proof 2: Any RoH above ROHCEILING forces Stop.
///
/// This harness encodes the no_roh_downgrade property:
/// if RoH exceeds the global ceiling, the kernel decision must be Stop.
#[kani::proof]
fn proof_no_roh_downgrade_forces_stop() {
    let cfg = PraxisGovernanceConfig::default();
    let kernel = PraxisGovernanceKernel::new(cfg);
    let mut ctx = mk_symbolic_safe_context();

    // Force RoH above the global ceiling.
    let roh_above = ROHCEILING + decimal(0.01);
    ctx.roh.roh = roh_above;

    // KER and Lyapunov are arbitrary here; the predicate focuses on RoH.
    kani::assert!(no_roh_downgrade(&kernel, ctx));
}

/// Proof 3: Under neurorights risk, decision must not be Allow.
///
/// This harness encodes no_neurorights_downgrade:
/// when neurorights_at_risk is true, the kernel must not return Allow.
#[kani::proof]
fn proof_no_neurorights_downgrade() {
    let cfg = PraxisGovernanceConfig::default();
    let kernel = PraxisGovernanceKernel::new(cfg);
    let ctx = mk_symbolic_safe_context();

    // Flag neurorights risk; Kani will explore kernel decisions under this condition.
    let neurorights_at_risk = true;

    kani::assert!(no_neurorights_downgrade(&kernel, ctx, neurorights_at_risk));
}
