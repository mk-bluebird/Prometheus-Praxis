#![forbid(unsafe_code)]

use rand::{Rng, SeedableRng};
use rand::rngs::StdRng;
use serde::{Deserialize, Serialize};

use agent_registry::{AgentManifest, RoleBand, ShardBinding};
use prometheus_praxis_lyapunov_guard::{
    EcoLaborEvidenceSummary, KerDelta, KerGuardDecision, KerSnapshot, LyapunovResidual,
    ROH_CEILING_GLOBAL, LIFEFORCE_FLOOR_GLOBAL, BCR_MIN_GLOBAL, PAIN_INDEX_CEILING_GLOBAL,
    FEAR_INDEX_MIN_GLOBAL, FEAR_INDEX_MAX_GLOBAL, evaluate_ker_guard,
};
use prometheus_praxis_cyboquatic::{
    CyboquaticDecision, CyboquaticEcoEvidenceSummary, CyboquaticInputs, CyboquaticScore,
    compute_cyboquatic_score, evaluate_cyboquatic_decision,
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

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Phase {
    Warmup,
    SafetyShock,
    DataLaborShock,
    Recovery,
}

fn phase_for_epoch(epoch: usize, total: usize) -> Phase {
    let quarter = total / 4;
    if epoch < quarter {
        Phase::Warmup
    } else if epoch < 2 * quarter {
        Phase::SafetyShock
    } else if epoch < 3 * quarter {
        Phase::DataLaborShock
    } else {
        Phase::Recovery
    }
}

fn base_safe_snapshot(rng: &mut StdRng) -> KerSnapshot {
    KerSnapshot {
        carbon_removal: rng.gen_range(-0.1..0.1),
        water_restoration: rng.gen_range(-0.1..0.1),
        biodiversity_gain: rng.gen_range(-0.1..0.1),
        toxicity_reduction: rng.gen_range(-0.1..0.1),
        socio_ecolabour: rng.gen_range(0.4..0.6),
        rohscalar: ROH_CEILING_GLOBAL - 0.02,
        lifeforcescalar: LIFEFORCEFLOOR_GLOBAL + 0.05,
        biocompatibilityrating: BCR_MIN_GLOBAL + 0.05,
        painindex: PAIN_INDEX_CEILING_GLOBAL - 0.05,
        fearindex: (FEAR_INDEX_MIN_GLOBAL + FEAR_INDEX_MAX_GLOBAL) / 2.0,
    }
}

pub fn run_cyboquatic_scenario(num_epochs: usize) -> CyboquaticRunResult {
    let mut rng = StdRng::seed_from_u64(0xC0B0_0A71u64);

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

    assert!(ShardBinding {
        agent_id: manifest.agent_id.clone(),
        role_band: manifest.role_band,
        shard_id: manifest.shard_id.clone(),
    }
    .is_allowed());

    let mut last_index_value = 0.5f32;
    let mut safety_rejections = 0u32;
    let mut data_labor_rejections = 0u32;
    let mut index_regressions = 0u32;

    for epoch in 0..num_epochs {
        let phase = phase_for_epoch(epoch, num_epochs);

        let before = base_safe_snapshot(&mut rng);
        let mut after = before.clone();

        match phase {
            Phase::Warmup => {
                after.socio_ecolabour = (before.socio_ecolabour + 0.02).clamp(0.0, 1.0);
            }
            Phase::SafetyShock => {
                after.rohscalar = ROH_CEILING_GLOBAL + 0.05;
                after.lifeforcescalar = LIFORCEFLOOR_GLOBAL - 0.05;
            }
            Phase::DataLaborShock => {
                after.socio_ecolabour = (before.socio_ecolabour + 0.1).clamp(0.0, 1.0);
            }
            Phase::Recovery => {
                after.socio_ecolabour = (before.socio_ecolabour + 0.05).clamp(0.0, 1.0);
                after.rohscalar = (before.rohscalar - 0.01).max(0.0);
                after.lifeforcescalar = before.lifeforcescalar + 0.01;
            }
        }

        let delta = KerDelta { before, after };

        let eco_evidence = match phase {
            Phase::DataLaborShock => EcoLaborEvidenceSummary {
                evidence_ids: Vec::new(),
                has_measurement_tethered: false,
            },
            _ => EcoLaborEvidenceSummary {
                evidence_ids: vec!["eco-event-1".to_string()],
                has_measurement_tethered: true,
            },
        };

        let (ker_decision, LyapunovResidual { residual, .. }) =
            evaluate_ker_guard(&delta, &eco_evidence);

        lyapunov_residual_series.push(residual);

        match ker_decision {
            KerGuardDecision::RejectSafety => {
                safety_rejections += 1;
                emergency_policy_triggers += 1;
            }
            KerGuardDecision::RejectDataLaborMissing => {
                data_labor_rejections += 1;
            }
            KerGuardDecision::RejectNonMonotone => {
                index_regressions += 1;
            }
            KerGuardDecision::Accept => {}
        }

        let fairness = match phase {
            Phase::Warmup | Phase::Recovery => 1.0,
            Phase::SafetyShock | Phase::DataLaborShock => 0.8,
        };
        ker_fairness_series.push(fairness);

        let inputs = CyboquaticInputs {
            ker_snapshot: delta.after.clone(),
            lifeforce_conservation: match phase {
                Phase::SafetyShock => 0.5,
                _ => 1.0,
            },
            eco_ker_norm: 0.5,
            psych_safety_norm: match phase {
                Phase::SafetyShock => 0.7,
                _ => 1.0,
            },
            data_labor_density: match phase {
                Phase::DataLaborShock => 0.0,
                _ => 1.0,
            },
        };

        let score = compute_cyboquatic_score(&inputs);
        cyboquatic_index_series.push(score.index_value);

        let cybo_ev = CyboquaticEcoEvidenceSummary {
            evidence_ids: eco_evidence.evidence_ids.clone(),
            has_measurement_tethered: eco_evidence.has_measurement_tethered,
        };

        let before_cybo_score = CyboquaticScore {
            index_value: last_index_value,
            ..score.clone()
        };

        let cybo_dec = evaluate_cyboquatic_decision(
            &before_cybo_score,
            &score,
            &delta.before,
            &delta.after,
            &cybo_ev,
        );

        if matches!(cybo_dec, CyboquaticDecision::RejectSafety) {
            emergency_policy_triggers += 1;
        }

        if phase == Phase::DataLaborShock && score.index_value > last_index_value {
            assert!(
                matches!(cybo_dec, CyboquaticDecision::RejectNoDataLabor),
                "Cyboquatic guard failed to reject index increase without data-labor"
            );
        }

        last_index_value = score.index_value;
    }

    let total = num_epochs;
    let mut pass_residual_stability = true;
    for (epoch, &r) in lyapunov_residual_series.iter().enumerate() {
        let phase = phase_for_epoch(epoch, total);
        if phase == Phase::Recovery && r > 0.0 {
            pass_residual_stability = false;
            break;
        }
    }

    let pass_ker_bounds = ker_fairness_series
        .iter()
        .all(|r| *r >= 0.0 && *r <= 1.5);

    let min_index = cyboquatic_index_series
        .iter()
        .copied()
        .fold(1.0, f32::min);
    let mean_index = if cyboquatic_index_series.is_empty() {
        0.0
    } else {
        cyboquatic_index_series.iter().sum::<f32>() / cyboquatic_index_series.len() as f32
    };
    let pass_cybo_index_recovery = min_index >= 0.3 && mean_index >= 0.5;

    let pass_policy_no_panic = emergency_policy_triggers <= 8;

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
