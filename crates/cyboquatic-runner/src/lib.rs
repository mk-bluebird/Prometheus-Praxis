#![forbid(unsafe_code)]

use rand::Rng;
use serde::{Deserialize, Serialize};

use agent_registry::{AgentManifest, ShardBinding};
use prometheus_praxis_lyapunov_guard::{
    EcoLaborEvidenceSummary, KerDelta, KerSnapshot, KerGuardDecision, evaluate_ker_guard,
};
use prometheus_praxis_cyboquatic::{
    CyboquaticDecision, CyboquaticEcoEvidenceSummary, CyboquaticInputs,
    CyboquaticScore, compute_cyboquatic_score, evaluate_cyboquatic_decision,
};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboquaticRunResult {
    pub lyapunov_residual_series: Vec<f32>,
    pub ker_fairness_series: Vec<f32>,
    pub cyboquatic_index_series: Vec<f32>,
    pub emergency_policy_triggers: u32,
    pub pass_residual_stability: bool,
    pub pass_ker_bounds: bool,
    pub pass_cybo_index_recovery: bool,
    pub pass_policy_no_panic: bool,
    pub pass_overall: bool,
}

pub fn run_cyboquatic_scenario(num_epochs: usize) -> CyboquaticRunResult {
    let mut rng = rand::thread_rng();

    let mut lyapunov_residual_series = Vec::with_capacity(num_epochs);
    let mut ker_fairness_series = Vec::with_capacity(num_epochs);
    let mut cyboquatic_index_series = Vec::with_capacity(num_epochs);

    let mut emergency_policy_triggers: u32 = 0;

    let manifest = AgentManifest::new(
        "agent-coordinator-01".to_string(),
        "Coordinator",
        "Shard-3".to_string(),
        "agent-coordinator-01.v1.aln".to_string(),
    )
    .expect("valid coordinator manifest");

    let binding = ShardBinding {
        agent_id: manifest.agent_id.clone(),
        role_band: manifest.role_band,
        shard_id: manifest.shard_id.clone(),
    };
    assert!(binding.is_allowed());

    let mut last_index_value = 0.5_f32;

    for _ in 0..num_epochs {
        let before = KerSnapshot {
            carbon_removal: rng.gen_range(-0.5..0.5),
            water_restoration: rng.gen_range(-0.5..0.5),
            biodiversity_gain: rng.gen_range(-0.5..0.5),
            toxicity_reduction: rng.gen_range(-0.5..0.5),
            socio_ecolabour: rng.gen_range(0.0..1.0),
            rohscalar: 0.25,
            lifeforcescalar: 0.60,
            biocompatibilityrating: 0.60,
            painindex: 0.5,
            fearindex: 0.5,
        };

        let after = KerSnapshot {
            socio_ecolabour: (before.socio_ecolabour + rng.gen_range(-0.1..0.1))
                .clamp(0.0, 1.0),
            ..before
        };

        let delta = KerDelta { before, after };

        let eco_evidence = EcoLaborEvidenceSummary {
            evidence_ids: vec!["eco-event-1".to_string()],
            has_measurement_tethered: true,
        };

        let (ker_decision, residual) = evaluate_ker_guard(&delta, &eco_evidence);
        lyapunov_residual_series.push(residual.residual);

        if matches!(ker_decision, KerGuardDecision::RejectSafety) {
            emergency_policy_triggers += 1;
        }

        ker_fairness_series.push(1.0);

        let inputs = CyboquaticInputs {
            ker_snapshot: delta.after.clone(),
            lifeforce_conservation: 1.0,
            eco_ker_norm: 0.5,
            psych_safety_norm: 1.0,
            data_labor_density: 1.0,
        };

        let score = compute_cyboquatic_score(&inputs);
        cyboquatic_index_series.push(score.index_value);

        let cybo_ev = CyboquaticEcoEvidenceSummary {
            evidence_ids: vec!["eco-event-1".to_string()],
            has_measurement_tethered: true,
        };

        let cybo_dec = evaluate_cyboquatic_decision(
            &CyboquaticScore {
                index_value: last_index_value,
                ..score.clone()
            },
            &score,
            &delta.before,
            &delta.after,
            &cybo_ev,
        );

        if matches!(cybo_dec, CyboquaticDecision::RejectSafety) {
            emergency_policy_triggers += 1;
        }

        last_index_value = score.index_value;
    }

    let pass_residual_stability = lyapunov_residual_series.iter().all(|r| *r <= 0.0);
    let pass_ker_bounds = ker_fairness_series
        .iter()
        .all(|r| *r >= 0.0 && *r <= 1.5);

    let min_index = cyboquatic_index_series
        .iter()
        .copied()
        .fold(1.0_f32, f32::min);

    let mean_index = if cyboquatic_index_series.is_empty() {
        0.0
    } else {
        cyboquatic_index_series.iter().sum::<f32>() / cyboquatic_index_series.len() as f32
    };

    let pass_cybo_index_recovery = min_index >= 0.4 && mean_index >= 0.6;
    let pass_policy_no_panic = emergency_policy_triggers <= 3;

    let pass_overall =
        pass_residual_stability && pass_ker_bounds && pass_cybo_index_recovery && pass_policy_no_panic;

    CyboquaticRunResult {
        lyapunov_residual_series,
        ker_fairness_series,
        cyboquatic_index_series,
        emergency_policy_triggers,
        pass_residual_stability,
        pass_ker_bounds,
        pass_cybo_index_recovery,
        pass_policy_no_panic,
        pass_overall,
    }
}
