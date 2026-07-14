// eco_restoration_shard/crates/ker_triad/src/lib.rs
//! ROLE: KER Triad + ESS distribution crate for Prometheus‑Praxis.
//! - Wraps the pure KER engine (Knowledge, Eco‑impact, Risk‑of‑harm).
//! - Exposes an ESS scalar and 9‑D ESS state path for governance.
//! - Provides hooks for KS-based first‑order stochastic dominance checks.

#![forbid(unsafe_code)]

pub mod ess;
pub mod ess_ks_gate;

use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};
use prometheuspraxisker::{KerOutput, KnowledgeEvidence, EcoImpactEvidence, RiskEvidence};

/// 9‑D ESS state vector stub: each coordinate is a scalar in \([0,1]\).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EssState9D {
    pub c1: Decimal,
    pub c2: Decimal,
    pub c3: Decimal,
    pub c4: Decimal,
    pub c5: Decimal,
    pub c6: Decimal,
    pub c7: Decimal,
    pub c8: Decimal,
    pub c9: Decimal,
}

/// ESS scalar and associated KER output.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EssSnapshot {
    pub ess: Decimal,
    pub ker: KerOutput,
}

/// Convenience function: compute KER triad and a trivial ESS scalar.
///
/// In a real implementation, ESS would be derived from KER, RoH, Lyapunov,
/// and domain envelopes; here we simply reuse KER.E as a placeholder ESS.
pub fn compute_ker_and_ess(
    knowledge: KnowledgeEvidence,
    eco: EcoImpactEvidence,
    risk: RiskEvidence,
) -> EssSnapshot {
    let ker = prometheuspraxisker::computeker(knowledge, eco, risk);
    let ess = ker.e; // Minimal stub: Eco‑impact reused as ESS scalar.
    EssSnapshot { ess, ker }
}

/// Minimal homotopy path stub: returns a constant 9‑D ESS state.
///
/// Real code will construct a piecewise‑linear path that respects corridors;
/// this stub keeps the type available for downstream wiring.
pub fn ess_homotopy_path_stub(ess: Decimal) -> Vec<EssState9D> {
    let state = EssState9D {
        c1: ess,
        c2: ess,
        c3: ess,
        c4: ess,
        c5: ess,
        c6: ess,
        c7: ess,
        c8: ess,
        c9: ess,
    };
    vec![state]
}
