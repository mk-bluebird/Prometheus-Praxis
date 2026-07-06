// eco_restoration_shard/crates/prometheus_praxis/src/rights_verification_kernel.rs
//
// ROLE
//   Prometheus-Praxis Rights Verification Kernel.
//   Non-actuating governance layer that binds KER/RoH/Tsafe/Lyapunov invariants
//   to Themis-Axiom, Pan-Ethos, Schutz-Recht, and Res Publica before any plan
//   can be committed or scheduled.
//
// REQUIREMENTS
//   - Rust edition 2024, rust-version = "1.85".
//   - !forbid(unsafe_code).
//   - Pure logic: no IO, no network, no hardware calls.
//   - Ready for Kani harnesses over verify_action.

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

/// Maximum allowed risk-of-harm scalar for any macro action.
pub const ROH_CEILING: Decimal = Decimal::from_f32(0.30).expect("constant");

/// Macro governance decision for a proposed action.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum GovernanceDecision {
    /// Action is fully permitted under current invariants and rights gates.
    Allow,
    /// Action is permitted only in a derated / attenuated form.
    Derate,
    /// Action is rejected and must not be actuated.
    Stop,
}

/// High-level domain for a proposed action.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ActionDomain {
    EcoRestoration,
    CityOperations,
    CosmicEnergy,
    MacroHealth,
}

/// Lane for the action (RESEARCH / PILOT / PRODUCTION), used for KER floors.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ActionLane {
    Research,
    Pilot,
    Production,
}

/// KER triad snapshot for a proposed action, each in 0..1.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerSnapshot {
    pub k: Decimal,
    pub e: Decimal,
    pub r: Decimal,
}

/// Lyapunov residuals for current and next state.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovResidualSnapshot {
    pub vcurrent: Decimal,
    pub vnext: Decimal,
    pub epsilon: Decimal,
}

/// Risk-of-harm scalar and lane metadata for the action.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RohSnapshot {
    pub roh: Decimal,
    pub domain: ActionDomain,
    pub lane: ActionLane,
}

/// Identifier for an ALN shard that encodes envelopes / corridors / axioms.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlnShardId {
    pub name: String,
    pub version: String,
}

/// Lightweight record of Themis-Axiom / Pan-Ethos evaluation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AxiomEvaluation {
    /// All axioms satisfied (non-rollback, neurorights, equality, eco-responsibility).
    pub all_satisfied: bool,
    /// Number of Themis axioms violated.
    pub themis_violations: u32,
    /// Number of Pan-Ethos axioms violated.
    pub pan_ethos_violations: u32,
}

/// Corpus Juris contract envelope: budget, RoH ceiling, time window.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContractEnvelope {
    pub contract_id: String,
    pub budget_ceiling: Decimal,
    pub roh_ceiling: Decimal,
    pub start_time: DateTime<Utc>,
    pub end_time: Option<DateTime<Utc>>,
}

/// Res Publica public-good evaluation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResPublicaEvaluation {
    /// Public-good scalar PG = G - L (non-negative means public good).
    pub public_good_scalar: Decimal,
    /// Whether the action is classified as public good.
    pub is_public_good: bool,
}

/// Schutz-Recht rights-risk score and appeal hook.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RightsRiskSnapshot {
    /// Rights-risk scalar Q (neurorights margin, privacy leakage, discrimination risk).
    pub q: Decimal,
    /// Critical threshold Qcrit; Q >= Qcrit should block and open appeal.
    pub q_crit: Decimal,
}

/// Appeal record for blocked rights or continuity.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppealRecord {
    pub appeal_id: String,
    pub action_id: String,
    pub opened_at: DateTime<Utc>,
    pub reason: String,
    pub rights_risk_q: Decimal,
    pub rights_risk_q_crit: Decimal,
    pub themis_violations: u32,
    pub pan_ethos_violations: u32,
}

/// Minimal qpudatashard descriptor for Veritas-Chain anchoring.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QpuDataShardDescriptor {
    pub shard_id: String,
    pub decision: GovernanceDecision,
    pub roh: Decimal,
    pub k: Decimal,
    pub e: Decimal,
    pub r: Decimal,
    pub vcurrent: Decimal,
    pub vnext: Decimal,
    pub epsilon: Decimal,
    pub domain: ActionDomain,
    pub lane: ActionLane,
    pub alnenvelope: AlnShardId,
    pub contract_id: Option<String>,
    pub public_good_scalar: Option<Decimal>,
    pub rights_risk_q: Option<Decimal>,
    pub rights_risk_q_crit: Option<Decimal>,
    pub created_at: DateTime<Utc>,
}

/// Composite context for validating a proposed macro action.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MacroActionContext {
    /// Unique identifier for the action request ID, mission ID, etc.
    pub action_id: String,
    /// Domain and lane for the action.
    pub domain: ActionDomain,
    pub lane: ActionLane,
    /// KER snapshot for the action.
    pub ker: KerSnapshot,
    /// RoH snapshot for the action.
    pub roh: RohSnapshot,
    /// Lyapunov residual snapshot for the governed object.
    pub lyapunov: LyapunovResidualSnapshot,
    /// Envelope shard (corridor / treaty / axiom set) that this action claims to satisfy.
    pub alnenvelope: AlnShardId,
    /// Contract envelope (Corpus Juris) if applicable.
    pub contract: Option<ContractEnvelope>,
    /// Themis / Pan-Ethos evaluation.
    pub axioms: AxiomEvaluation,
    /// Res Publica evaluation.
    pub res_publica: Option<ResPublicaEvaluation>,
    /// Schutz-Recht rights-risk snapshot.
    pub rights_risk: RightsRiskSnapshot,
}

/// Governance kernel configuration: lane-specific KER thresholds.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PraxisGovernanceConfig {
    /// Minimum K required for actions in PRODUCTION lane.
    pub kmin_production: Decimal,
    /// Minimum E required for actions in PRODUCTION lane.
    pub emin_production: Decimal,
    /// Maximum R allowed for actions in PRODUCTION lane.
    pub rmax_production: Decimal,

    /// Minimum K for PILOT.
    pub kmin_pilot: Decimal,
    /// Minimum E for PILOT.
    pub emin_pilot: Decimal,
    /// Maximum R for PILOT.
    pub rmax_pilot: Decimal,

    /// Minimum K for RESEARCH.
    pub kmin_research: Decimal,
    /// Minimum E for RESEARCH.
    pub emin_research: Decimal,
    /// Maximum R for RESEARCH.
    pub rmax_research: Decimal,
}

impl Default for PraxisGovernanceConfig {
    fn default() -> Self {
        // Example thresholds; tune via ALN-backed evidence and Kani proofs.
        Self {
            kmin_production: Decimal::from_f32(0.95).unwrap(),
            emin_production: Decimal::from_f32(0.92).unwrap(),
            rmax_production: Decimal::from_f32(0.08).unwrap(),

            kmin_pilot: Decimal::from_f32(0.92).unwrap(),
            emin_pilot: Decimal::from_f32(0.90).unwrap(),
            rmax_pilot: Decimal::from_f32(0.10).unwrap(),

            kmin_research: Decimal::from_f32(0.90).unwrap(),
            emin_research: Decimal::from_f32(0.85).unwrap(),
            rmax_research: Decimal::from_f32(0.20).unwrap(),
        }
    }
}

/// Macro governance kernel (non-actuating, pure at the signature level).
#[derive(Debug, Clone)]
pub struct PraxisGovernanceKernel {
    config: PraxisGovernanceConfig,
}

impl PraxisGovernanceKernel {
    /// Construct a new governance kernel with the given configuration.
    pub fn new(config: PraxisGovernanceConfig) -> Self {
        Self { config }
    }

    /// Validate an eco-restoration action (soil, water, forest, ocean).
    pub fn validate_eco_action(
        &self,
        ctx: MacroActionContext,
    ) -> (GovernanceDecision, QpuDataShardDescriptor, Option<AppealRecord>) {
        self.verify_action(ctx)
    }

    /// Validate a city-operations action (traffic, microgrid, water, public space).
    pub fn validate_city_action(
        &self,
        ctx: MacroActionContext,
    ) -> (GovernanceDecision, QpuDataShardDescriptor, Option<AppealRecord>) {
        self.verify_action(ctx)
    }

    /// Validate a cosmic-energy orbital logistics action.
    pub fn validate_energy_action(
        &self,
        ctx: MacroActionContext,
    ) -> (GovernanceDecision, QpuDataShardDescriptor, Option<AppealRecord>) {
        self.verify_action(ctx)
    }

    /// Validate a macro-health action (Cyboquatic remediation, clinical logistics).
    pub fn validate_health_action(
        &self,
        ctx: MacroActionContext,
    ) -> (GovernanceDecision, QpuDataShardDescriptor, Option<AppealRecord>) {
        self.verify_action(ctx)
    }

    /// Core validation logic shared across domains.
    ///
    /// Decision lattice:
    /// - If any invariant (KER, RoH ceiling, Lyapunov) or rights/treaty gate fails,
    ///   decision = Stop.
    /// - If invariants pass but Lyapunov non-increase fails, decision = Derate.
    /// - Only when invariants and rights gates all pass, decision = Allow.
    pub fn verify_action(
        &self,
        ctx: MacroActionContext,
    ) -> (GovernanceDecision, QpuDataShardDescriptor, Option<AppealRecord>) {
        let ker_ok = self.ker_within_lane(&ctx);
        let roh_ok = self.roh_within_ceiling(&ctx.roh, &ctx.contract);
        let lyap_ok = self.lyapunov_non_increasing(&ctx.lyapunov);
        let axioms_ok = self.axioms_satisfied(&ctx.axioms);
        let rights_ok = self.rights_risk_within_bounds(&ctx.rights_risk);
        let contract_ok = self.contract_window_ok(&ctx.contract);

        // If any hard rights or axioms violation, we open an appeal and Stop.
        let mut appeal: Option<AppealRecord> = None;
        if !axioms_ok || !rights_ok {
            let now = Utc::now();
            appeal = Some(AppealRecord {
                appeal_id: format!("APPEAL-{}", ctx.action_id),
                action_id: ctx.action_id.clone(),
                opened_at: now,
                reason: Self::build_appeal_reason(&ctx),
                rights_risk_q: ctx.rights_risk.q,
                rights_risk_q_crit: ctx.rights_risk.q_crit,
                themis_violations: ctx.axioms.themis_violations,
                pan_ethos_violations: ctx.axioms.pan_ethos_violations,
            });
        }

        // Base decision from invariants (excluding rights and contracts).
        let invariant_decision = match (ker_ok, roh_ok, lyap_ok) {
            (true, true, true) => GovernanceDecision::Allow,
            (true, true, false) => GovernanceDecision::Derate,
            _ => GovernanceDecision::Stop,
        };

        // Rights and contracts are veto layers: they can only tighten.
        let final_decision = if !axioms_ok || !rights_ok || !contract_ok {
            GovernanceDecision::Stop
        } else {
            invariant_decision
        };

        let shard = QpuDataShardDescriptor {
            shard_id: format!("PRAXIS-{}", ctx.action_id),
            decision: final_decision,
            roh: ctx.roh.roh,
            k: ctx.ker.k,
            e: ctx.ker.e,
            r: ctx.ker.r,
            vcurrent: ctx.lyapunov.vcurrent,
            vnext: ctx.lyapunov.vnext,
            epsilon: ctx.lyapunov.epsilon,
            domain: ctx.domain,
            lane: ctx.lane,
            alnenvelope: ctx.alnenvelope.clone(),
            contract_id: ctx.contract.as_ref().map(|c| c.contract_id.clone()),
            public_good_scalar: ctx
                .res_publica
                .as_ref()
                .map(|rp| rp.public_good_scalar),
            rights_risk_q: Some(ctx.rights_risk.q),
            rights_risk_q_crit: Some(ctx.rights_risk.q_crit),
            created_at: Utc::now(),
        };

        (final_decision, shard, appeal)
    }

    fn ker_within_lane(&self, ctx: &MacroActionContext) -> bool {
        let KerSnapshot { k, e, r } = ctx.ker;
        match ctx.lane {
            ActionLane::Production => {
                k >= self.config.kmin_production
                    && e >= self.config.emin_production
                    && r <= self.config.rmax_production
            }
            ActionLane::Pilot => {
                k >= self.config.kmin_pilot
                    && e >= self.config.emin_pilot
                    && r <= self.config.rmax_pilot
            }
            ActionLane::Research => {
                k >= self.config.kmin_research
                    && e >= self.config.emin_research
                    && r <= self.config.rmax_research
            }
        }
    }

    fn roh_within_ceiling(&self, roh: &RohSnapshot, contract: &Option<ContractEnvelope>) -> bool {
        // Global non-offsettable ceiling.
        if roh.roh > ROH_CEILING {
            return false;
        }
        // Optional stricter contract RoH ceiling.
        if let Some(env) = contract {
            if roh.roh > env.roh_ceiling {
                return false;
            }
        }
        true
    }

    fn lyapunov_non_increasing(&self, lyap: &LyapunovResidualSnapshot) -> bool {
        // Enforce Vnext <= Vcurrent + epsilon (noise band).
        lyap.vnext <= lyap.vcurrent + lyap.epsilon
    }

    fn axioms_satisfied(&self, axioms: &AxiomEvaluation) -> bool {
        axioms.all_satisfied
    }

    fn rights_risk_within_bounds(&self, rights: &RightsRiskSnapshot) -> bool {
        rights.q < rights.q_crit
    }

    fn contract_window_ok(&self, contract: &Option<ContractEnvelope>) -> bool {
        if let Some(env) = contract {
            let now = Utc::now();
            if now < env.start_time {
                return false;
            }
            if let Some(end) = env.end_time {
                if now > end {
                    return false;
                }
            }
        }
        true
    }

    fn build_appeal_reason(ctx: &MacroActionContext) -> String {
        let mut parts: Vec<String> = Vec::new();
        if !ctx.axioms.all_satisfied {
            parts.push(format!(
                "Axiom violations: Themis={}, PanEthos={}",
                ctx.axioms.themis_violations, ctx.axioms.pan_ethos_violations
            ));
        }
        if ctx.rights_risk.q >= ctx.rights_risk.q_crit {
            parts.push(format!(
                "Rights risk Q={} >= Qcrit={}",
                ctx.rights_risk.q, ctx.rights_risk.q_crit
            ));
        }
        if parts.is_empty() {
            "Appeal opened for governance review".to_string()
        } else {
            parts.join("; ")
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn decimal(v: f32) -> Decimal {
        Decimal::from_f32(v).unwrap()
    }

    #[test]
    fn allow_when_all_invariants_and_rights_ok() {
        let cfg = PraxisGovernanceConfig::default();
        let kernel = PraxisGovernanceKernel::new(cfg);

        let ctx = MacroActionContext {
            action_id: "TEST-ALLOW".to_string(),
            domain: ActionDomain::EcoRestoration,
            lane: ActionLane::Research,
            ker: KerSnapshot {
                k: decimal(0.92),
                e: decimal(0.90),
                r: decimal(0.10),
            },
            roh: RohSnapshot {
                roh: decimal(0.25),
                domain: ActionDomain::EcoRestoration,
                lane: ActionLane::Research,
            },
            lyapunov: LyapunovResidualSnapshot {
                vcurrent: decimal(1.0),
                vnext: decimal(0.95),
                epsilon: decimal(0.02),
            },
            alnenvelope: AlnShardId {
                name: "ecosafety.corridor.v1".to_string(),
                version: "1.0.0".to_string(),
            },
            contract: None,
            axioms: AxiomEvaluation {
                all_satisfied: true,
                themis_violations: 0,
                pan_ethos_violations: 0,
            },
            res_publica: Some(ResPublicaEvaluation {
                public_good_scalar: decimal(0.10),
                is_public_good: true,
            }),
            rights_risk: RightsRiskSnapshot {
                q: decimal(0.05),
                q_crit: decimal(0.20),
            },
        };

        let (decision, _shard, appeal) = kernel.verify_action(ctx);
        assert_eq!(decision, GovernanceDecision::Allow);
        assert!(appeal.is_none());
    }

    #[test]
    fn stop_when_roh_exceeds_global_ceiling() {
        let cfg = PraxisGovernanceConfig::default();
        let kernel = PraxisGovernanceKernel::new(cfg);

        let ctx = MacroActionContext {
            action_id: "TEST-STOP-ROH".to_string(),
            domain: ActionDomain::EcoRestoration,
            lane: ActionLane::Research,
            ker: KerSnapshot {
                k: decimal(0.95),
                e: decimal(0.93),
                r: decimal(0.05),
            },
            roh: RohSnapshot {
                roh: decimal(0.31),
                domain: ActionDomain::EcoRestoration,
                lane: ActionLane::Research,
            },
            lyapunov: LyapunovResidualSnapshot {
                vcurrent: decimal(1.0),
                vnext: decimal(0.99),
                epsilon: decimal(0.02),
            },
            alnenvelope: AlnShardId {
                name: "ecosafety.corridor.v1".to_string(),
                version: "1.0.0".to_string(),
            },
            contract: None,
            axioms: AxiomEvaluation {
                all_satisfied: true,
                themis_violations: 0,
                pan_ethos_violations: 0,
            },
            res_publica: None,
            rights_risk: RightsRiskSnapshot {
                q: decimal(0.05),
                q_crit: decimal(0.20),
            },
        };

        let (decision, _shard, appeal) = kernel.verify_action(ctx);
        assert_eq!(decision, GovernanceDecision::Stop);
        assert!(appeal.is_none());
    }

    #[test]
    fn derate_when_lyapunov_increases_but_ker_roh_ok() {
        let cfg = PraxisGovernanceConfig::default();
        let kernel = PraxisGovernanceKernel::new(cfg);

        let ctx = MacroActionContext {
            action_id: "TEST-DERATE-LYAP".to_string(),
            domain: ActionDomain::EcoRestoration,
            lane: ActionLane::Research,
            ker: KerSnapshot {
                k: decimal(0.92),
                e: decimal(0.90),
                r: decimal(0.10),
            },
            roh: RohSnapshot {
                roh: decimal(0.25),
                domain: ActionDomain::EcoRestoration,
                lane: ActionLane::Research,
            },
            lyapunov: LyapunovResidualSnapshot {
                vcurrent: decimal(1.0),
                vnext: decimal(1.05),
                epsilon: decimal(0.02),
            },
            alnenvelope: AlnShardId {
                name: "ecosafety.corridor.v1".to_string(),
                version: "1.0.0".to_string(),
            },
            contract: None,
            axioms: AxiomEvaluation {
                all_satisfied: true,
                themis_violations: 0,
                pan_ethos_violations: 0,
            },
            res_publica: None,
            rights_risk: RightsRiskSnapshot {
                q: decimal(0.05),
                q_crit: decimal(0.20),
            },
        };

        let (decision, _shard, appeal) = kernel.verify_action(ctx);
        assert_eq!(decision, GovernanceDecision::Derate);
        assert!(appeal.is_none());
    }

    #[test]
    fn stop_and_open_appeal_when_rights_risk_exceeds_qcrit() {
        let cfg = PraxisGovernanceConfig::default();
        let kernel = PraxisGovernanceKernel::new(cfg);

        let ctx = MacroActionContext {
            action_id: "TEST-RIGHTS".to_string(),
            domain: ActionDomain::CityOperations,
            lane: ActionLane::Production,
            ker: KerSnapshot {
                k: decimal(0.96),
                e: decimal(0.95),
                r: decimal(0.05),
            },
            roh: RohSnapshot {
                roh: decimal(0.10),
                domain: ActionDomain::CityOperations,
                lane: ActionLane::Production,
            },
            lyapunov: LyapunovResidualSnapshot {
                vcurrent: decimal(0.8),
                vnext: decimal(0.78),
                epsilon: decimal(0.02),
            },
            alnenvelope: AlnShardId {
                name: "rights.envelope.city.v1".to_string(),
                version: "1.0.0".to_string(),
            },
            contract: None,
            axioms: AxiomEvaluation {
                all_satisfied: false,
                themis_violations: 1,
                pan_ethos_violations: 0,
            },
            res_publica: None,
            rights_risk: RightsRiskSnapshot {
                q: decimal(0.30),
                q_crit: decimal(0.20),
            },
        };

        let (decision, _shard, appeal) = kernel.verify_action(ctx);
        assert_eq!(decision, GovernanceDecision::Stop);
        assert!(appeal.is_some());
    }
}
