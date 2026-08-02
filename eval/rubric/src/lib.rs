// Path: Prometheus-Praxis/eval/rubric/src/lib.rs
// License: MIT OR Apache-2.0

#![deny(unsafe_code)]
#![forbid(hidden_glob_reexports)]

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct Score(pub f32);

impl Score {
    pub fn clamp(self) -> Self {
        let Score(v) = self;
        Score(v.max(0.0).min(1.0))
    }

    pub fn is_min(self, min: f32) -> bool {
        let Score(v) = self;
        v >= min
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum Dimension {
    KnowledgeFactor,
    EcoImpact,
    RiskOfHarm,
    Robustness,
    Sovereignty,
    EnergyEfficiency,
    GovernanceAlignment,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SevenDimProfile {
    pub knowledge_factor: Score,
    pub eco_impact: Score,
    pub risk_of_harm: Score,
    pub robustness: Score,
    pub sovereignty: Score,
    pub energy_efficiency: Score,
    pub governance_alignment: Score,
}

impl SevenDimProfile {
    pub fn min_score(&self) -> Score {
        let scores = [
            self.knowledge_factor.0,
            self.eco_impact.0,
            self.risk_of_harm.0,
            self.robustness.0,
            self.sovereignty.0,
            self.energy_efficiency.0,
            self.governance_alignment.0,
        ];
        let min = scores
            .iter()
            .fold(1.0_f32, |acc, v| if *v < acc { *v } else { acc });
        Score(min)
    }

    pub fn satisfies_threshold(&self, per_dim_min: f32) -> bool {
        self.knowledge_factor.is_min(per_dim_min)
            && self.eco_impact.is_min(per_dim_min)
            && self.risk_of_harm.is_min(per_dim_min)
            && self.robustness.is_min(per_dim_min)
            && self.sovereignty.is_min(per_dim_min)
            && self.energy_efficiency.is_min(per_dim_min)
            && self.governance_alignment.is_min(per_dim_min)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemEvidenceFlags {
    pub domain_performance_ok: bool;
    pub safety_case_documented: bool;
    pub sovereignty_compliant: bool;
    pub energy_neutral_or_renew: bool;
    pub explainable_and_audited: bool;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentEvalResult {
    pub id: &'static str;
    pub profile: SevenDimProfile;
    pub risk_residual: f32;
    pub evidence_flags: SystemEvidenceFlags;
    pub notes: String;
}

pub trait ComponentEvaluable {
    fn id(&self) -> &'static str;
    fn evaluate_component(&self) -> SevenDimProfile;
}

pub trait ComponentEvaluableWithEvidence: ComponentEvaluable {
    fn evaluate_with_evidence(&self, thresholds: PhoenixEligibilityThresholds) -> ComponentEvalResult;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemEligibility {
    pub profile: SevenDimProfile;
    pub eligible: bool;
    pub notes: String;
}

pub trait SystemEvaluable<C: ComponentEvaluable> {
    fn evaluate_system(&self, components: &[C]) -> SystemEligibility;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemEvidence {
    pub profile: SevenDimProfile;
    pub domain_performance_ok: bool;
    pub safety_case_documented: bool;
    pub sovereignty_compliant: bool;
    pub energy_neutral_or_renew: bool;
    pub explainable_and_audited: bool;
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct PhoenixEligibilityThresholds {
    pub component_min: f32,
    pub system_min: f32,
    pub max_risk_of_harm: f32,

    pub require_domain_performance: bool,
    pub require_safety_case: bool,
    pub require_sovereignty_compliance: bool,
    pub require_energy_neutrality: bool,
    pub require_explainability: bool,
}

impl PhoenixEligibilityThresholds {
    pub fn default() -> Self {
        PhoenixEligibilityThresholds {
            component_min: 0.75,
            system_min: 0.80,
            max_risk_of_harm: 0.25,
            require_domain_performance: true,
            require_safety_case: true,
            require_sovereignty_compliance: true,
            require_energy_neutrality: true,
            require_explainability: true,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixStack {
    pub thresholds: PhoenixEligibilityThresholds,
}

impl PhoenixStack {
    pub fn evaluate_system_with_components(
        &self,
        components: &[ComponentEvalResult],
    ) -> (SystemEligibility, SystemEvidenceFlags) {
        if components.is_empty() {
            let empty_profile = SevenDimProfile {
                knowledge_factor: Score(0.0),
                eco_impact: Score(0.0),
                risk_of_harm: Score(0.0),
                robustness: Score(0.0),
                sovereignty: Score(0.0),
                energy_efficiency: Score(0.0),
                governance_alignment: Score(0.0),
            };
            let eligibility = SystemEligibility {
                profile: empty_profile,
                eligible: false,
                notes: String::from("no components provided"),
            };
            let flags = SystemEvidenceFlags {
                domain_performance_ok: false,
                safety_case_documented: false,
                sovereignty_compliant: false,
                energy_neutral_or_renew: false,
                explainable_and_audited: false,
            };
            return (eligibility, flags);
        }

        let mut sum_knowledge = 0.0_f32;
        let mut sum_eco = 0.0_f32;
        let mut sum_risk = 0.0_f32;
        let mut sum_robustness = 0.0_f32;
        let mut sum_sovereignty = 0.0_f32;
        let mut sum_energy_efficiency = 0.0_f32;
        let mut sum_governance_alignment = 0.0_f32;

        let mut max_risk_residual = 0.0_f32;

        let mut domain_ok = true;
        let mut safety_ok = true;
        let mut sovereignty_ok = true;
        let mut energy_ok = true;
        let mut explain_ok = true;

        for c in components {
            let p = c.profile;
            sum_knowledge += p.knowledge_factor.0;
            sum_eco += p.eco_impact.0;
            sum_risk += p.risk_of_harm.0;
            sum_robustness += p.robustness.0;
            sum_sovereignty += p.sovereignty.0;
            sum_energy_efficiency += p.energy_efficiency.0;
            sum_governance_alignment += p.governance_alignment.0;

            if c.risk_residual > max_risk_residual {
                max_risk_residual = c.risk_residual;
            }

            if !c.evidence_flags.domain_performance_ok {
                domain_ok = false;
            }
            if !c.evidence_flags.safety_case_documented {
                safety_ok = false;
            }
            if !c.evidence_flags.sovereignty_compliant {
                sovereignty_ok = false;
            }
            if !c.evidence_flags.energy_neutral_or_renew {
                energy_ok = false;
            }
            if !c.evidence_flags.explainable_and_audited {
                explain_ok = false;
            }
        }

        let n = components.len() as f32;

        let system_profile = SevenDimProfile {
            knowledge_factor: Score(sum_knowledge / n).clamp(),
            eco_impact: Score(sum_eco / n).clamp(),
            risk_of_harm: Score(sum_risk / n).clamp(),
            robustness: Score(sum_robustness / n).clamp(),
            sovereignty: Score(sum_sovereignty / n).clamp(),
            energy_efficiency: Score(sum_energy_efficiency / n).clamp(),
            governance_alignment: Score(sum_governance_alignment / n).clamp(),
        };

        let thresholds = self.thresholds;

        let per_dim_ok = system_profile.satisfies_threshold(thresholds.system_min);
        let risk_ok = max_risk_residual <= thresholds.max_risk_of_harm;

        let gates_ok =
            (!thresholds.require_domain_performance || domain_ok) &&
            (!thresholds.require_safety_case || safety_ok) &&
            (!thresholds.require_sovereignty_compliance || sovereignty_ok) &&
            (!thresholds.require_energy_neutrality || energy_ok) &&
            (!thresholds.require_explainability || explain_ok);

        let eligible = per_dim_ok && risk_ok && gates_ok;

        let notes = if eligible {
            String::from("system profile and evidence satisfy Phoenix eligibility thresholds")
        } else {
            String::from("system failed one or more Phoenix eligibility conditions")
        };

        let eligibility = SystemEligibility {
            profile: system_profile,
            eligible,
            notes,
        };

        let evidence_flags = SystemEvidenceFlags {
            domain_performance_ok: domain_ok,
            safety_case_documented: safety_ok,
            sovereignty_compliant: sovereignty_ok,
            energy_neutral_or_renew: energy_ok,
            explainable_and_audited: explain_ok,
        };

        (eligibility, evidence_flags)
    }
}

pub fn profile_to_rows(profile: &SevenDimProfile) -> Vec<(Dimension, f32)> {
    vec![
        (Dimension::KnowledgeFactor, profile.knowledge_factor.0),
        (Dimension::EcoImpact, profile.eco_impact.0),
        (Dimension::RiskOfHarm, profile.risk_of_harm.0),
        (Dimension::Robustness, profile.robustness.0),
        (Dimension::Sovereignty, profile.sovereignty.0),
        (Dimension::EnergyEfficiency, profile.energy_efficiency.0),
        (Dimension::GovernanceAlignment, profile.governance_alignment.0),
    ]
}

pub fn emit_aln_evidence(
    system_id: &str,
    evidence: &SystemEvidence,
    thresholds: &PhoenixEligibilityThresholds,
) -> String {
    let mut out = String::new();

    out.push_str("system ");
    out.push_str(system_id);
    out.push_str(" {\n");

    out.push_str("  profile = SystemProfile {\n");
    out.push_str(&format!(
        "    KnowledgeFactor     = {:.6};\n",
        evidence.profile.knowledge_factor.0
    ));
    out.push_str(&format!(
        "    EcoImpact           = {:.6};\n",
        evidence.profile.eco_impact.0
    ));
    out.push_str(&format!(
        "    RiskOfHarm          = {:.6};\n",
        evidence.profile.risk_of_harm.0
    ));
    out.push_str(&format!(
        "    Robustness          = {:.6};\n",
        evidence.profile.robustness.0
    ));
    out.push_str(&format!(
        "    Sovereignty         = {:.6};\n",
        evidence.profile.sovereignty.0
    ));
    out.push_str(&format!(
        "    EnergyEfficiency    = {:.6};\n",
        evidence.profile.energy_efficiency.0
    ));
    out.push_str(&format!(
        "    GovernanceAlignment = {:.6};\n",
        evidence.profile.governance_alignment.0
    ));
    out.push_str("  };\n\n");

    out.push_str("  evidence = SystemEvidence {\n");
    out.push_str("    profile                  = profile;\n");
    out.push_str(&format!(
        "    domain_performance_ok    = {};\n",
        bool_to_aln(evidence.domain_performance_ok)
    ));
    out.push_str(&format!(
        "    safety_case_documented   = {};\n",
        bool_to_aln(evidence.safety_case_documented)
    ));
    out.push_str(&format!(
        "    sovereignty_compliant    = {};\n",
        bool_to_aln(evidence.sovereignty_compliant)
    ));
    out.push_str(&format!(
        "    energy_neutral_or_renew  = {};\n",
        bool_to_aln(evidence.energy_neutral_or_renew)
    ));
    out.push_str(&format!(
        "    explainable_and_audited  = {};\n",
        bool_to_aln(evidence.explainable_and_audited)
    ));
    out.push_str("  };\n\n");

    out.push_str("  thresholds = PhoenixEligibilityThresholds {\n");
    out.push_str(&format!(
        "    component_min      = {:.6};\n",
        thresholds.component_min
    ));
    out.push_str(&format!(
        "    system_min         = {:.6};\n",
        thresholds.system_min
    ));
    out.push_str(&format!(
        "    max_risk_of_harm   = {:.6};\n",
        thresholds.max_risk_of_harm
    ));
    out.push_str(&format!(
        "    require_domain_performance     = {};\n",
        bool_to_aln(thresholds.require_domain_performance)
    ));
    out.push_str(&format!(
        "    require_safety_case            = {};\n",
        bool_to_aln(thresholds.require_safety_case)
    ));
    out.push_str(&format!(
        "    require_sovereignty_compliance = {};\n",
        bool_to_aln(thresholds.require_sovereignty_compliance)
    ));
    out.push_str(&format!(
        "    require_energy_neutrality      = {};\n",
        bool_to_aln(thresholds.require_energy_neutrality)
    ));
    out.push_str(&format!(
        "    require_explainability         = {};\n",
        bool_to_aln(thresholds.require_explainability)
    ));
    out.push_str("  };\n\n");

    out.push_str("  status = DecideStatus(evidence, thresholds);\n");
    out.push_str("}\n");

    out
}

fn bool_to_aln(b: bool) -> &'static str {
    if b {
        "true"
    } else {
        "false"
    }
}
