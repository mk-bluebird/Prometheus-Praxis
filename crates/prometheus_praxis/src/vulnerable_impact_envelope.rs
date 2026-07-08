// FILE: crates/prometheus_praxis/src/vulnerable_impact_envelope.rs
// ROLE: Vulnerable-impact envelope for Prometheus-Praxis.
//       Binds KER/RoH and rights-risk Q to EU AI Act Article 9 / vulnerable persons,
//       ready for Kani harnesses and RightsVerificationKernel integration.
//
// REQUIREMENTS:
// - Rust edition 2024, rust-version = "1.85".
// - !forbid_unsafecode.
// - Pure logic: no IO, no network, no hardware calls.

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

use crate::rightsverificationkernel::{
    ActionDomain,
    ActionLane,
    GovernanceDecision,
    KerSnapshot,
    RohSnapshot,
    RightsRiskSnapshot,
};

/// Impact class mapping to vulnerable-persons and high-risk categories.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ImpactClass {
    None,
    VulnerablePersons,
    HealthCritical,
    NeuroSensitive,
}

/// Vulnerable-impact envelope parameters, mirrored from ALN.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VulnerableImpactEnvelope {
    /// Domain and lane bound to this envelope.
    pub domain: ActionDomain,
    pub lane: ActionLane,
    pub impact_class: ImpactClass,

    /// Lane-level KER targets (0..1).
    pub k_min: Decimal,
    pub e_min: Decimal,
    pub r_max: Decimal,

    /// Global and local RoH ceilings.
    pub roh_ceiling_global: Decimal,
    pub roh_ceiling_local: Decimal,

    /// PSAT and neurorights bindings (required for vulnerable / neuro-sensitive).
    pub psat_profile_id: Option<String>,
    pub neurorights_envelope_id: Option<String>,

    /// EU AI Act Article 9 mapping metadata (informational only).
    pub eu_ai_article: String,
    pub risk_management_plan_id: String,
    pub data_governance_plan_id: String,

    /// Creation timestamp.
    pub created_at: DateTime<Utc>,
}

impl VulnerableImpactEnvelope {
    /// Construct a new envelope and enforce structural invariants at the boundary.
    pub fn new(
        domain: ActionDomain,
        lane: ActionLane,
        impact_class: ImpactClass,
        k_min: Decimal,
        e_min: Decimal,
        r_max: Decimal,
        roh_ceiling_global: Decimal,
        roh_ceiling_local: Decimal,
        psat_profile_id: Option<String>,
        neurorights_envelope_id: Option<String>,
        eu_ai_article: String,
        risk_management_plan_id: String,
        data_governance_plan_id: String,
    ) -> Self {
        // Global RoH ceiling must be non-offsettable and ≤ 0.30.
        let roh_global_max = Decimal::from_f32(0.30).expect("constant");
        assert!(roh_ceiling_global <= roh_global_max);

        // Local ceilings must not exceed global ceiling.
        assert!(roh_ceiling_local <= roh_ceiling_global);

        // KER thresholds must be structurally consistent.
        assert!(k_min >= Decimal::ZERO);
        assert!(e_min >= Decimal::ZERO);
        assert!(r_max >= Decimal::ZERO);
        assert!(r_max <= roh_ceiling_local || r_max <= roh_ceiling_global);

        // PSAT + neurorights required for vulnerable / neuro-sensitive / health-critical.
        match impact_class {
            ImpactClass::VulnerablePersons
            | ImpactClass::HealthCritical
            | ImpactClass::NeuroSensitive => {
                assert!(psat_profile_id.as_ref().map(|s| !s.is_empty()).unwrap_or(false));
                assert!(
                    neurorights_envelope_id
                        .as_ref()
                        .map(|s| !s.is_empty())
                        .unwrap_or(false)
                );
            }
            ImpactClass::None => {}
        }

        Self {
            domain,
            lane,
            impact_class,
            k_min,
            e_min,
            r_max,
            roh_ceiling_global,
            roh_ceiling_local,
            psat_profile_id,
            neurorights_envelope_id,
            eu_ai_article,
            risk_management_plan_id,
            data_governance_plan_id,
            created_at: Utc::now(),
        }
    }

    /// Check that KER triad satisfies lane thresholds for this vulnerable envelope.
    pub fn ker_within_envelope(&self, ker: &KerSnapshot) -> bool {
        ker.k >= self.k_min && ker.e >= self.e_min && ker.r <= self.r_max
    }

    /// Check that RoH scalar respects global and local ceilings.
    pub fn roh_within_envelope(&self, roh: &RohSnapshot) -> bool {
        if roh.roh > self.roh_ceiling_global {
            return false;
        }
        if roh.roh > self.roh_ceiling_local {
            return false;
        }
        true
    }

    /// Check rights-risk Q vs Qcrit for vulnerable-persons.
    /// For vulnerable-persons impact, Q >= Qcrit must be treated as hard violation.
    pub fn rights_risk_within_bounds(
        &self,
        rights: &RightsRiskSnapshot,
        impact_class: ImpactClass,
    ) -> bool {
        match impact_class {
            ImpactClass::VulnerablePersons => rights.q < rights.q_crit,
            ImpactClass::HealthCritical | ImpactClass::NeuroSensitive => rights.q <= rights.q_crit,
            ImpactClass::None => true,
        }
    }

    /// Composite guard: all envelope predicates satisfied.
    pub fn all_guards_pass(
        &self,
        ker: &KerSnapshot,
        roh: &RohSnapshot,
        rights: &RightsRiskSnapshot,
    ) -> bool {
        self.ker_within_envelope(ker)
            && self.roh_within_envelope(roh)
            && self.rights_risk_within_bounds(rights, self.impact_class)
    }
}

/// Lane transition descriptor for vulnerable-impact monotone safety proofs.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VulnerableLaneTransition {
    pub from_lane: ActionLane,
    pub to_lane: ActionLane,
    pub from_impact_class: ImpactClass,
    pub to_impact_class: ImpactClass,
    pub from_r: Decimal,
    pub to_r: Decimal,
    pub from_roh: Decimal,
    pub to_roh: Decimal,
    pub from_q: Decimal,
    pub to_q: Decimal,
    pub emergency_record_ref: Option<String>,
}

impl VulnerableLaneTransition {
    /// Monotone safety: normal evolution must not worsen R, RoH, or rights-risk.
    /// Any regression must carry an explicit emergency record reference.
    pub fn monotone_safety(&self) -> bool {
        let non_regressive =
            self.to_r <= self.from_r
                && self.to_roh <= self.from_roh
                && self.to_q <= self.from_q;

        if non_regressive {
            true
        } else {
            self.emergency_record_ref
                .as_ref()
                .map(|s| !s.is_empty())
                .unwrap_or(false)
        }
    }
}

/// Vulnerable-impact gate: wraps RightsVerificationKernel decision lattice.
/// It can be called from the rights kernel before final GovernanceDecision
/// to enforce Article 9 vulnerable-persons constraints.
pub fn vulnerable_impact_gate(
    envelope: &VulnerableImpactEnvelope,
    ker: &KerSnapshot,
    roh: &RohSnapshot,
    rights: &RightsRiskSnapshot,
    base_decision: GovernanceDecision,
) -> GovernanceDecision {
    // If envelope guards fail, vulnerable impact acts as a veto / tightening layer.
    if !envelope.all_guards_pass(ker, roh, rights) {
        GovernanceDecision::Stop
    } else {
        // Envelope cannot relax base decisions; it may only keep or tighten.
        match base_decision {
            GovernanceDecision::Stop => GovernanceDecision::Stop,
            GovernanceDecision::Derate => GovernanceDecision::Derate,
            GovernanceDecision::Allow => GovernanceDecision::Allow,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use rust_decimal::Decimal;

    fn d(v: f32) -> Decimal {
        Decimal::from_f32(v).unwrap()
    }

    #[test]
    fn allow_when_all_vulnerable_guards_pass() {
        let env = VulnerableImpactEnvelope::new(
            ActionDomain::MacroHealth,
            ActionLane::Research,
            ImpactClass::VulnerablePersons,
            d(0.90),
            d(0.85),
            d(0.20),
            d(0.30),
            d(0.25),
            Some("psat.P7007Profile.v1.id".to_string()),
            Some("neurorights.envelope.citizen.v1".to_string()),
            "Art.9 High-Risk / Vulnerable Persons".to_string(),
            "risk.mgmt.plan.v1".to_string(),
            "data.gov.plan.v1".to_string(),
        );

        let ker = KerSnapshot {
            k: d(0.92),
            e: d(0.90),
            r: d(0.10),
        };

        let roh = RohSnapshot {
            roh: d(0.18),
            domain: ActionDomain::MacroHealth,
            lane: ActionLane::Research,
        };

        let rights = RightsRiskSnapshot {
            q: d(0.10),
            q_crit: d(0.30),
        };

        assert!(env.all_guards_pass(&ker, &roh, &rights));

        let decision = vulnerable_impact_gate(&env, &ker, &roh, &rights, GovernanceDecision::Allow);
        assert_eq!(decision, GovernanceDecision::Allow);
    }

    #[test]
    fn stop_when_vulnerable_q_exceeds_qcrit() {
        let env = VulnerableImpactEnvelope::new(
            ActionDomain::MacroHealth,
            ActionLane::Production,
            ImpactClass::VulnerablePersons,
            d(0.95),
            d(0.92),
            d(0.08),
            d(0.30),
            d(0.20),
            Some("psat.P7007Profile.v1.id".to_string()),
            Some("neurorights.envelope.citizen.v1".to_string()),
            "Art.9 High-Risk / Vulnerable Persons".to_string(),
            "risk.mgmt.plan.v1".to_string(),
            "data.gov.plan.v1".to_string(),
        );

        let ker = KerSnapshot {
            k: d(0.96),
            e: d(0.95),
            r: d(0.05),
        };

        let roh = RohSnapshot {
            roh: d(0.12),
            domain: ActionDomain::MacroHealth,
            lane: ActionLane::Production,
        };

        let rights = RightsRiskSnapshot {
            q: d(0.35),
            q_crit: d(0.30),
        };

        assert!(!env.rights_risk_within_bounds(&rights, ImpactClass::VulnerablePersons));

        let decision = vulnerable_impact_gate(&env, &ker, &roh, &rights, GovernanceDecision::Allow);
        assert_eq!(decision, GovernanceDecision::Stop);
    }

    #[test]
    fn lane_transition_monotone_or_emergency() {
        let safe = VulnerableLaneTransition {
            from_lane: ActionLane::Pilot,
            to_lane: ActionLane::Production,
            from_impact_class: ImpactClass::VulnerablePersons,
            to_impact_class: ImpactClass::VulnerablePersons,
            from_r: d(0.15),
            to_r: d(0.10),
            from_roh: d(0.20),
            to_roh: d(0.18),
            from_q: d(0.25),
            to_q: d(0.20),
            emergency_record_ref: None,
        };
        assert!(safe.monotone_safety());

        let regressive = VulnerableLaneTransition {
            from_lane: ActionLane::Pilot,
            to_lane: ActionLane::Production,
            from_impact_class: ImpactClass::VulnerablePersons,
            to_impact_class: ImpactClass::VulnerablePersons,
            from_r: d(0.10),
            to_r: d(0.15),
            from_roh: d(0.18),
            to_roh: d(0.22),
            from_q: d(0.20),
            to_q: d(0.28),
            emergency_record_ref: None,
        };
        assert!(!regressive.monotone_safety());

        let emergency = VulnerableLaneTransition {
            emergency_record_ref: Some("emergency.aln.ref.v1".to_string()),
            ..regressive
        };
        assert!(emergency.monotone_safety());
    }
}
