// Path: Prometheus-Praxis/eval/components/src/lib.rs
// License: MIT OR Apache-2.0

#![deny(unsafe_code)]
#![forbid(hidden_glob_reexports)]

use serde::{Deserialize, Serialize};

use ppx_eval_rubric::{
    ComponentEvaluable,
    PhoenixEligibilityThresholds,
    Score,
    SevenDimProfile,
    SystemEligibility,
    SystemEvaluable,
    SystemEvidence,
};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixContext {
    pub monsoon_intensity_index: f32,
    pub canyon_heat_gradient: f32,
    pub fog_channel_density: f32,
    pub industrial_waste_load: f32,
    pub sovereignty_weight: f32,
    pub energy_constraint: f32,
}

impl PhoenixContext {
    pub fn phoenix_default() -> Self {
        PhoenixContext {
            monsoon_intensity_index: 0.85,
            canyon_heat_gradient: 0.90,
            fog_channel_density: 0.75,
            industrial_waste_load: 0.80,
            sovereignty_weight: 0.95,
            energy_constraint: 0.70,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AdvectionKernel {
    pub scheme_name: String,
    pub cfl_safety_margin: f32,
    pub physical_fidelity_index: f32,
    pub restored_flow_ratio: f32,
    pub numerical_robustness_index: f32,
    pub ctx: PhoenixContext,
}

impl ComponentEvaluable for AdvectionKernel {
    fn id(&self) -> &'static str {
        "advection_kernel"
    }

    fn evaluate_component(&self) -> SevenDimProfile {
        let fidelity = self.physical_fidelity_index.max(0.0).min(1.0);
        let cfl = self.cfl_safety_margin.max(0.0).min(1.0);
        let restored = self.restored_flow_ratio.max(0.0).min(1.0);
        let num_rob = self.numerical_robustness_index.max(0.0).min(1.0);
        let fog = self.ctx.fog_channel_density.max(0.0).min(1.0);
        let monsoon = self.ctx.monsoon_intensity_index.max(0.0).min(1.0);
        let sov = self.ctx.sovereignty_weight.max(0.0).min(1.0);
        let energy = self.ctx.energy_constraint.max(0.0).min(1.0);

        let knowledge = Score(0.5 + 0.5 * fidelity);
        let eco = Score(restored * 0.7 + fog * 0.3);
        let risk = Score((1.0 - cfl) * 0.5 + (1.0 - num_rob) * 0.5).clamp();
        let robustness = Score(0.5 * num_rob + 0.5 * (1.0 - monsoon * 0.2));
        let sovereignty = Score(0.8 * sov + 0.2 * fidelity);
        let energy_eff = Score((1.0 - energy) * 0.7 + 0.3 * num_rob);
        let governance = Score(0.7 * sov + 0.3 * fidelity);

        SevenDimProfile {
            knowledge_factor: knowledge.clamp(),
            eco_impact: eco.clamp(),
            risk_of_harm: risk.clamp(),
            robustness: robustness.clamp(),
            sovereignty: sovereignty.clamp(),
            energy_efficiency: energy_eff.clamp(),
            governance_alignment: governance.clamp(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MarlArchitecture {
    pub policy_alignment_index: f32,
    pub rogue_pattern_resilience: f32,
    pub multi_actor_scalability: f32,
    pub consent_corridor_strength: f32,
    pub cybercore_binding_strength: f32,
    pub ctx: PhoenixContext,
}

impl ComponentEvaluable for MarlArchitecture {
    fn id(&self) -> &'static str {
        "marl_architecture"
    }

    fn evaluate_component(&self) -> SevenDimProfile {
        let policy = self.policy_alignment_index.max(0.0).min(1.0);
        let rogue = self.rogue_pattern_resilience.max(0.0).min(1.0);
        let multi = self.multi_actor_scalability.max(0.0).min(1.0);
        let consent = self.consent_corridor_strength.max(0.0).min(1.0);
        let core = self.cybercore_binding_strength.max(0.0).min(1.0);
        let energy = self.ctx.energy_constraint.max(0.0).min(1.0);

        let knowledge = Score(0.5 * policy + 0.5 * multi);
        let eco = Score(0.6 * policy + 0.4 * rogue);
        let risk = Score((1.0 - rogue) * 0.6 + (1.0 - consent) * 0.4).clamp();
        let robustness = Score(0.5 * rogue + 0.5 * multi);
        let sovereignty = Score(0.5 * consent + 0.5 * core);
        let energy_eff = Score((1.0 - energy) * 0.5 + 0.5 * multi);
        let governance = Score(0.7 * core + 0.3 * policy);

        SevenDimProfile {
            knowledge_factor: knowledge.clamp(),
            eco_impact: eco.clamp(),
            risk_of_harm: risk.clamp(),
            robustness: robustness.clamp(),
            sovereignty: sovereignty.clamp(),
            energy_efficiency: energy_eff.clamp(),
            governance_alignment: governance.clamp(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StreamingPipeline {
    pub end_to_end_latency_ms: f32,
    pub failure_recovery_index: f32,
    pub data_sovereignty_index: f32,
    pub energy_cost_per_event: f32,
    pub biosignal_integration_index: f32,
    pub ctx: PhoenixContext,
}

impl ComponentEvaluable for StreamingPipeline {
    fn id(&self) -> &'static str {
        "streaming_pipeline"
    }

    fn evaluate_component(&self) -> SevenDimProfile {
        let latency_score =
            (1.0 - (self.end_to_end_latency_ms / 1000.0).max(0.0).min(1.0)).max(0.0);
        let failure = self.failure_recovery_index.max(0.0).min(1.0);
        let data_sov = self.data_sovereignty_index.max(0.0).min(1.0);
        let energy_cost = self.energy_cost_per_event.max(0.0).min(1.0);
        let bio = self.biosignal_integration_index.max(0.0).min(1.0);
        let energy = self.ctx.energy_constraint.max(0.0).min(1.0);

        let knowledge = Score(0.5 * latency_score + 0.5 * bio);
        let eco = Score((1.0 - energy_cost) * 0.6 + 0.4 * failure);
        let risk = Score((1.0 - failure) * 0.5 + (1.0 - data_sov) * 0.5).clamp();
        let robustness = Score(0.6 * failure + 0.4 * latency_score);
        let sovereignty = Score(data_sov);
        let energy_eff = Score((1.0 - energy_cost) * 0.8 + 0.2 * energy);
        let governance = Score(0.6 * data_sov + 0.4 * failure);

        SevenDimProfile {
            knowledge_factor: knowledge.clamp(),
            eco_impact: eco.clamp(),
            risk_of_harm: risk.clamp(),
            robustness: robustness.clamp(),
            sovereignty: sovereignty.clamp(),
            energy_efficiency: energy_eff.clamp(),
            governance_alignment: governance.clamp(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixStack {
    pub advection: AdvectionKernel,
    pub marl: MarlArchitecture,
    pub streaming: StreamingPipeline,
    pub thresholds: PhoenixEligibilityThresholds,
}

impl PhoenixStack {
    pub fn new(
        advection: AdvectionKernel,
        marl: MarlArchitecture,
        streaming: StreamingPipeline,
        thresholds: PhoenixEligibilityThresholds,
    ) -> Self {
        PhoenixStack {
            advection,
            marl,
            streaming,
            thresholds,
        }
    }
}

impl SystemEvaluable<Box<dyn ComponentEvaluable>> for PhoenixStack {
    fn evaluate_system(&self, components: &[Box<dyn ComponentEvaluable>]) -> SystemEligibility {
        let mut notes = String::new();

        for c in components {
            let profile = c.evaluate_component();
            if !profile.satisfies_threshold(self.thresholds.component_min) {
                notes.push_str(&format!(
                    "Component '{}' failed per-dimension component_min.\n",
                    c.id()
                ));
            }
            if profile.risk_of_harm.0 > self.thresholds.max_risk_of_harm {
                notes.push_str(&format!(
                    "Component '{}' exceeded max_risk_of_harm.\n",
                    c.id()
                ));
            }
        }

        let adv = self.advection.evaluate_component();
        let marl = self.marl.evaluate_component();
        let stream = self.streaming.evaluate_component();

        let aggregated = SevenDimProfile {
            knowledge_factor: Score(
                (adv.knowledge_factor.0 + marl.knowledge_factor.0 + stream.knowledge_factor.0)
                    / 3.0,
            ),
            eco_impact: Score(
                (adv.eco_impact.0 + marl.eco_impact.0 + stream.eco_impact.0) / 3.0,
            ),
            risk_of_harm: Score(
                adv.risk_of_harm
                    .0
                    .max(marl.risk_of_harm.0)
                    .max(stream.risk_of_harm.0),
            ),
            robustness: Score(
                (adv.robustness.0 + marl.robustness.0 + stream.robustness.0) / 3.0,
            ),
            sovereignty: Score(
                (adv.sovereignty.0 + marl.sovereignty.0 + stream.sovereignty.0) / 3.0,
            ),
            energy_efficiency: Score(
                (adv.energy_efficiency.0
                    + marl.energy_efficiency.0
                    + stream.energy_efficiency.0)
                    / 3.0,
            ),
            governance_alignment: Score(
                (adv.governance_alignment.0
                    + marl.governance_alignment.0
                    + stream.governance_alignment.0)
                    / 3.0,
            ),
        };

        // Governance flags should be set by higher-level tooling once evidence is available.
        let evidence = SystemEvidence {
            profile: aggregated.clone(),
            domain_performance_ok: false,
            safety_case_documented: false,
            sovereignty_compliant: false,
            energy_neutral_or_renew: false,
            explainable_and_audited: false,
        };

        let eligible = aggregated.satisfies_threshold(self.thresholds.system_min)
            && aggregated.risk_of_harm.0 <= self.thresholds.max_risk_of_harm
            && notes.is_empty()
            && evidence.domain_performance_ok
            && evidence.safety_case_documented
            && evidence.sovereignty_compliant
            && evidence.energy_neutral_or_renew
            && evidence.explainable_and_audited;

        if !eligible {
            notes.push_str(
                "Integrated stack failed Phoenix eligibility thresholds and/or governance gates.\n",
            );
        } else {
            notes.push_str(
                "Integrated stack passes Phoenix eligibility thresholds and governance gates.\n",
            );
        }

        SystemEligibility {
            profile: aggregated,
            eligible,
            notes,
        }
    }
}

pub fn phoenix_component_matrix(
    advection: &AdvectionKernel,
    marl: &MarlArchitecture,
    streaming: &StreamingPipeline,
) -> [(&'static str, SevenDimProfile); 3] {
    [
        (advection.id(), advection.evaluate_component()),
        (marl.id(), marl.evaluate_component()),
        (streaming.id(), streaming.evaluate_component()),
    ]
}
