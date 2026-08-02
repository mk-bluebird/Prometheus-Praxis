// Path: Prometheus-Praxis/eval/components/phx_component_profiles.rs
// License: MIT OR Apache-2.0
// Rust edition: 2024, rust-version = "1.85"

use serde::{Deserialize, Serialize};

use crate::eval::rubric::{
    ComponentEvaluable, Dimension, PhoenixThresholds, Score, SevenDimProfile,
    SystemEligibility, SystemEvaluable,
};

/// Phoenix-specific configuration context.
/// Bind canyon geometries, FOG channels, industrial flows, and governance parameters here.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixContext {
    pub monsoon_intensity_index: f32,
    pub canyon_heat_gradient: f32,
    pub fog_channel_density: f32,
    pub industrial_waste_load: f32,
    pub sovereignty_weight: f32,
    pub energy_constraint: f32,
}

/// Advection kernel descriptor for Phoenix.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AdvectionKernel {
    pub scheme_name: String,
    pub cfl_safety_margin: f32,
    pub physical_fidelity_index: f32,
    pub restored_flow_ratio: f32,
    pub numerical_robustness_index: f32,
    pub phoenix_ctx: PhoenixContext,
}

impl ComponentEvaluable for AdvectionKernel {
    fn id(&self) -> &'static str {
        "advection_kernel"
    }

    fn evaluate_component(&self) -> SevenDimProfile {
        // Knowledge-factor: grounded in PDE theory, validated against Phoenix flows.
        let knowledge = Score(
            0.5
                + 0.5 * self.physical_fidelity_index.min(1.0)
        );

        // Eco-impact: driven by restored_flow_ratio and FOG effectiveness.
        let eco = Score(
            (self.restored_flow_ratio * 0.7
                + self.phoenix_ctx.fog_channel_density * 0.3)
                .min(1.0),
        );

        // Risk-of-harm: lower is better; derive from overshoot and instability proxies.
        let risk = Score(
            (1.0 - self.cfl_safety_margin.max(0.0).min(1.0))
                * 0.5
                + (1.0 - self.numerical_robustness_index.min(1.0)) * 0.5,
        )
        .clamp();

        // Robustness: numerical robustness plus sensitivity to monsoon extremes.
        let robustness = Score(
            0.5 * self.numerical_robustness_index.min(1.0)
                + 0.5 * (1.0
                    - self.phoenix_ctx.monsoon_intensity_index.max(0.0).min(1.0)
                        * 0.2),
        );

        // Sovereignty: advection kernel must not encode hidden control; mostly governance-bound.
        let sovereignty = Score(
            0.8 * self.phoenix_ctx.sovereignty_weight.min(1.0)
                + 0.2 * self.physical_fidelity_index.min(1.0),
        );

        // Energy efficiency: compute cost vs flow coverage.
        let energy_eff = Score(
            (1.0 - self.phoenix_ctx.energy_constraint.max(0.0).min(1.0)) * 0.7
                + 0.3 * self.numerical_robustness_index.min(1.0),
        );

        // Governance alignment: how well the kernel exposes safe knobs to Cybercore.
        let governance = Score(
            0.7 * self.phoenix_ctx.sovereignty_weight.min(1.0)
                + 0.3 * self.physical_fidelity_index.min(1.0),
        );

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

/// MARL architecture descriptor for Phoenix city dynamics.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MarlArchitecture {
    pub policy_alignment_index: f32,
    pub rogue_pattern_resilience: f32,
    pub multi_actor_scalability: f32,
    pub consent_corridor_strength: f32,
    pub cybercore_binding_strength: f32,
    pub phoenix_ctx: PhoenixContext,
}

impl ComponentEvaluable for MarlArchitecture {
    fn id(&self) -> &'static str {
        "marl_architecture"
    }

    fn evaluate_component(&self) -> SevenDimProfile {
        let knowledge = Score(
            0.5 * self.policy_alignment_index.min(1.0)
                + 0.5 * self.multi_actor_scalability.min(1.0),
        );

        let eco = Score(
            0.6 * self.policy_alignment_index.min(1.0)
                + 0.4 * self.rogue_pattern_resilience.min(1.0),
        );

        let risk = Score(
            (1.0 - self.rogue_pattern_resilience.min(1.0)) * 0.6
                + (1.0 - self.consent_corridor_strength.min(1.0)) * 0.4,
        )
        .clamp();

        let robustness = Score(
            0.5 * self.rogue_pattern_resilience.min(1.0)
                + 0.5 * self.multi_actor_scalability.min(1.0),
        );

        let sovereignty = Score(
            0.5 * self.consent_corridor_strength.min(1.0)
                + 0.5 * self.cybercore_binding_strength.min(1.0),
        );

        let energy_eff = Score(
            (1.0 - self.phoenix_ctx.energy_constraint.max(0.0).min(1.0)) * 0.5
                + 0.5 * self.multi_actor_scalability.min(1.0),
        );

        let governance = Score(
            0.7 * self.cybercore_binding_strength.min(1.0)
                + 0.3 * self.policy_alignment_index.min(1.0),
        );

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

/// Streaming pipeline descriptor for Phoenix biosignals and sensor data.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StreamingPipeline {
    pub end_to_end_latency_ms: f32,
    pub failure_recovery_index: f32,
    pub data_sovereignty_index: f32,
    pub energy_cost_per_event: f32,
    pub biosignal_integration_index: f32,
    pub phoenix_ctx: PhoenixContext,
}

impl ComponentEvaluable for StreamingPipeline {
    fn id(&self) -> &'static str {
        "streaming_pipeline"
    }

    fn evaluate_component(&self) -> SevenDimProfile {
        let latency_score = (1.0
            - (self.end_to_end_latency_ms / 1000.0).min(1.0))
            .max(0.0);

        let knowledge = Score(
            0.5 * latency_score
                + 0.5 * self.biosignal_integration_index.min(1.0),
        );

        let eco = Score(
            (1.0 - self.energy_cost_per_event.max(0.0).min(1.0)) * 0.6
                + 0.4 * self.failure_recovery_index.min(1.0),
        );

        let risk = Score(
            (1.0 - self.failure_recovery_index.min(1.0)) * 0.5
                + (1.0 - self.data_sovereignty_index.min(1.0)) * 0.5,
        )
        .clamp();

        let robustness = Score(
            0.6 * self.failure_recovery_index.min(1.0)
                + 0.4 * latency_score,
        );

        let sovereignty = Score(self.data_sovereignty_index.min(1.0));

        let energy_eff = Score(
            (1.0 - self.energy_cost_per_event.max(0.0).min(1.0)) * 0.8
                + 0.2 * self.phoenix_ctx.energy_constraint.min(1.0),
        );

        let governance = Score(
            0.6 * self.data_sovereignty_index.min(1.0)
                + 0.4 * self.failure_recovery_index.min(1.0),
        );

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

/// End-to-end Phoenix stack descriptor:
/// ties together the three components and evaluates integrated eligibility.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhoenixStack {
    pub advection: AdvectionKernel,
    pub marl: MarlArchitecture,
    pub streaming: StreamingPipeline,
    pub thresholds: PhoenixThresholds,
}

impl SystemEvaluable<Box<dyn ComponentEvaluable>> for PhoenixStack {
    fn evaluate_system(&self, components: &[Box<dyn ComponentEvaluable>]) -> SystemEligibility {
        // Phase A: verify each component satisfies per-dimension minima.
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

        // Aggregate interactions: simple conservative policy for now.
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
                // Use worst-case risk_of_harm among components.
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

        // Integrated eligibility gate.
        let eligible = aggregated.satisfies_threshold(self.thresholds.system_min)
            && aggregated.risk_of_harm.0 <= self.thresholds.max_risk_of_harm
            && notes.is_empty();

        if !eligible {
            notes.push_str("Integrated stack failed Phoenix eligibility thresholds.\n");
        } else {
            notes.push_str("Integrated stack passes Phoenix eligibility thresholds.\n");
        }

        SystemEligibility {
            profile: aggregated,
            eligible,
            notes,
        }
    }
}
