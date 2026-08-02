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

pub trait ComponentEvaluable {
    fn id(&self) -> &'static str;
    fn evaluate_component(&self) -> SevenDimProfile;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemEligibility {
    pub profile: SevenDimProfile,
    pub eligible: bool,
    pub notes: String,
}

pub trait SystemEvaluable<C: ComponentEvaluable> {
    fn evaluate_system(&self, components: &[C]) -> SystemEligibility;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemEvidence {
    pub profile: SevenDimProfile,
    pub domain_performance_ok: bool,
    pub safety_case_documented: bool,
    pub sovereignty_compliant: bool,
    pub energy_neutral_or_renew: bool,
    pub explainable_and_audited: bool,
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
