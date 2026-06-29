// FILE: crates/prometheus_praxis/src/lib.rs
// ROLE: Prometheus-Praxis facade crate.
//       Re-exports PraxisGovernanceKernel and binds Prometheus-style
//       metrics/telemetry into KER evidence bundles for macro-governance.

#![forbid(unsafe_code)]
#![warn(missing_docs)]

pub mod praxis_governance_kernel;

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

use praxis_governance_kernel::{
    ActionDomain,
    ActionLane,
    AlnShardId,
    GovernanceDecision,
    KerSnapshot,
    LyapunovResidualSnapshot,
    MacroActionContext,
    PraxisGovernanceConfig,
    PraxisGovernanceKernel,
    QpuDataShardDescriptor,
    RohSnapshot,
};

pub mod guards;
pub mod logging;
pub mod metrics;
pub mod planner;
pub mod types;

pub use guards::{CompositeGuards, GuardError, PrometheusGuards};
pub use logging::{DecisionLog, DecisionLogger, VeritasChainClient};
pub use planner::{DefaultPrometheusPlanner, ExecutionPlan, PlanStep, PrometheusPlanner};
pub use types::{Bounded01, PrometheusTask, TaskKind};

use prometheus_praxis_ker::{
    EcoImpactEvidence,
    KerOutput,
    KnowledgeEvidence,
    RiskEvidence,
    compute_ker,
};

/// Prometheus-Praxis: Adaptive Implementation Engine
///
/// This crate provides the non-actuating planning, guard, and logging layers
/// for the Synoptic Network, translating high-level strategic goals into
/// verifiable, neurorights-safe task graphs that can be audited and replayed
/// from KER evidence bundles.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PraxisTelemetrySnapshot {
    /// RoH scalar for this host or workflow (0..1).
    pub roh_scalar: Decimal,

    /// Corridor safety margin (0..1); larger is safer.
    pub tsafe_signed_distance: Decimal,

    /// City-object Lyapunov residual V_t for this governed object.
    pub lyapunov_v_current: Decimal,

    /// Predicted next Lyapunov residual V_{t+1} under proposed action.
    pub lyapunov_v_next: Decimal,

    /// Allowed Lyapunov epsilon band for noise and estimation error.
    pub lyapunov_epsilon: Decimal,

    /// Prometheus-derived KER outputs for this workflow.
    pub ker_output: KerOutput,

    /// Domain metadata for this action.
    pub domain: ActionDomain,

    /// Lane metadata for this action.
    pub lane: ActionLane,

    /// ALN envelope shard describing corridors and treaties.
    pub aln_envelope: AlnShardId,

    /// Time at which telemetry snapshot was assembled.
    pub captured_at: DateTime<Utc>,
}

/// Minimal metric bag modelled on Prometheus gauges/counters for KER mapping.
///
/// In a deployment, this would typically be fed by Prometheus registries or
/// a JSON snapshot API, but here it is a pure struct for verifier and CI wiring.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PraxisMetricBag {
    /// Fraction of models deployed that have passed validation (0..1).
    pub validated_model_coverage: Decimal,

    /// Consistency of telemetry streams and invariants (0..1).
    pub telemetry_consistency: Decimal,

    /// Fraction of envelopes backed by formal proofs (0..1).
    pub proof_backed_envelopes: Decimal,

    /// Net carbon reduction contribution (0..1).
    pub carbon_reduction: Decimal,

    /// Net gain in water safety corridor metrics (0..1).
    pub water_safety_gain: Decimal,

    /// Net biodiversity gain signal (0..1).
    pub biodiversity_gain: Decimal,

    /// Rate of guard violations over the last window (0..1).
    pub guard_violation_rate: Decimal,

    /// Fraction of actions executed in thin safety margins (0..1).
    pub thin_margin_fraction: Decimal,

    /// Tail risk for Lyapunov residual excursions (0..1).
    pub lyapunov_tail_risk: Decimal,

    /// RoH scalar for the governed object (0..1).
    pub roh_scalar: Decimal,

    /// Signed safety margin in corridor space (0..1).
    pub tsafe_signed_distance: Decimal,

    /// Current Lyapunov residual for the object.
    pub lyapunov_v_current: Decimal,

    /// Predicted Lyapunov residual after proposed action.
    pub lyapunov_v_next: Decimal,

    /// Epsilon band for Lyapunov residual tolerance.
    pub lyapunov_epsilon: Decimal,
}

impl PraxisMetricBag {
    /// Convert Prometheus-style metrics into KER evidence bundles.
    pub fn to_ker_evidence(&self) -> (KnowledgeEvidence, EcoImpactEvidence, RiskEvidence) {
        let k_ev = KnowledgeEvidence {
            validated_model_coverage: self.validated_model_coverage,
            telemetry_consistency: self.telemetry_consistency,
            proof_backed_envelopes: self.proof_backed_envelopes,
        };
        let e_ev = EcoImpactEvidence {
            carbon_reduction: self.carbon_reduction,
            water_safety_gain: self.water_safety_gain,
            biodiversity_gain: self.biodiversity_gain,
        };
        let r_ev = RiskEvidence {
            guard_violation_rate: self.guard_violation_rate,
            thin_margin_fraction: self.thin_margin_fraction,
            lyapunov_tail_risk: self.lyapunov_tail_risk,
        };
        (k_ev, e_ev, r_ev)
    }

    /// Build a complete telemetry snapshot from metrics, domain, lane, and ALN envelope.
    pub fn to_telemetry_snapshot(
        &self,
        domain: ActionDomain,
        lane: ActionLane,
        aln_envelope: AlnShardId,
    ) -> PraxisTelemetrySnapshot {
        let (k_ev, e_ev, r_ev) = self.to_ker_evidence();
        let ker_output = compute_ker(&k_ev, &e_ev, &r_ev);

        PraxisTelemetrySnapshot {
            roh_scalar: self.roh_scalar,
            tsafe_signed_distance: self.tsafe_signed_distance,
            lyapunov_v_current: self.lyapunov_v_current,
            lyapunov_v_next: self.lyapunov_v_next,
            lyapunov_epsilon: self.lyapunov_epsilon,
            ker_output,
            domain,
            lane,
            aln_envelope,
            captured_at: Utc::now(),
        }
    }
}

/// High-level governance facade over `PraxisGovernanceKernel`.
///
/// This struct owns a kernel instance and exposes domain-specific validation
/// functions that depend only on metric-derived telemetry snapshots.
#[derive(Debug, Clone)]
pub struct PrometheusPraxisGovernance {
    kernel: PraxisGovernanceKernel,
}

impl PrometheusPraxisGovernance {
    /// Construct a new governance facade with the given config.
    pub fn new(config: PraxisGovernanceConfig) -> Self {
        let kernel = PraxisGovernanceKernel::new(config);
        Self { kernel }
    }

    /// Validate a macro action based on Prometheus-derived telemetry.
    ///
    /// This is the canonical entry point for EcoNet and city-object integration.
    pub fn validate_from_telemetry(
        &self,
        action_id: String,
        telemetry: PraxisTelemetrySnapshot,
    ) -> (GovernanceDecision, QpuDataShardDescriptor) {
        let ker_snap = KerSnapshot {
            k: telemetry.ker_output.k,
            e: telemetry.ker_output.e,
            r: telemetry.ker_output.r,
        };

        let roh_snap = RohSnapshot {
            roh: telemetry.roh_scalar,
            domain: telemetry.domain,
            lane: telemetry.lane,
        };

        let lyap_snap = LyapunovResidualSnapshot {
            v_current: telemetry.lyapunov_v_current,
            v_next: telemetry.lyapunov_v_next,
            epsilon: telemetry.lyapunov_epsilon,
        };

        let ctx = MacroActionContext {
            action_id,
            domain: telemetry.domain,
            lane: telemetry.lane,
            ker: ker_snap,
            roh: roh_snap,
            lyapunov: lyap_snap,
            aln_envelope: telemetry.aln_envelope.clone(),
        };

        match telemetry.domain {
            ActionDomain::EcoRestoration => self.kernel.validate_eco_action(ctx),
            ActionDomain::CityOperations => self.kernel.validate_city_action(ctx),
            ActionDomain::CosmicEnergy => self.kernel.validate_energy_action(ctx),
            ActionDomain::MacroHealth => self.kernel.validate_health_action(ctx),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn telemetry_mapping_preserves_basic_invariants() {
        let bag = PraxisMetricBag {
            validated_model_coverage: Decimal::from_f32(0.9).unwrap(),
            telemetry_consistency: Decimal::from_f32(0.9).unwrap(),
            proof_backed_envelopes: Decimal::from_f32(0.9).unwrap(),
            carbon_reduction: Decimal::from_f32(0.9).unwrap(),
            water_safety_gain: Decimal::from_f32(0.9).unwrap(),
            biodiversity_gain: Decimal::from_f32(0.9).unwrap(),
            guard_violation_rate: Decimal::from_f32(0.05).unwrap(),
            thin_margin_fraction: Decimal::from_f32(0.05).unwrap(),
            lyapunov_tail_risk: Decimal::from_f32(0.05).unwrap(),
            roh_scalar: Decimal::from_f32(0.20).unwrap(),
            tsafe_signed_distance: Decimal::from_f32(0.50).unwrap(),
            lyapunov_v_current: Decimal::from_f32(1.0).unwrap(),
            lyapunov_v_next: Decimal::from_f32(0.98).unwrap(),
            lyapunov_epsilon: Decimal::from_f32(0.03).unwrap(),
        };

        let aln = AlnShardId {
            name: "ecosafety.corridor.city.v1".to_string(),
            version: "1.0.0".to_string(),
        };

        let snap = bag.to_telemetry_snapshot(
            ActionDomain::CityOperations,
            ActionLane::Research,
            aln,
        );

        assert!(snap.ker_output.k >= Decimal::ZERO && snap.ker_output.k <= Decimal::ONE);
        assert!(snap.ker_output.e >= Decimal::ZERO && snap.ker_output.e <= Decimal::ONE);
        assert!(snap.ker_output.r >= Decimal::ZERO && snap.ker_output.r <= Decimal::ONE);
        assert!(snap.lyapunov_epsilon >= Decimal::ZERO);
    }
}
