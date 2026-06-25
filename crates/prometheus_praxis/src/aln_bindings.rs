use aln_runtime::AlnShard;
use crate::types::{TaskKind, PrometheusTask};
use transhuman_rights_core::{NeuroRightsEnvelope, EqualityEnvelope};

pub struct PrometheusPraxisConfig {
    pub roh_global_ceiling: f32,
    pub monotone_ota: bool,
}

impl PrometheusPraxisConfig {
    pub fn load_from_aln(shard: &AlnShard) -> anyhow::Result<Self> {
        // uses your existing ALN runtime to read invariants [file:14]
        let roh_global_ceiling = shard.get_f32("invariant.roh_global_ceiling")?;
        let monotone_ota = shard.get_bool("invariant.monotone_ota")?;
        Ok(Self { roh_global_ceiling, monotone_ota })
    }
}

// Example constructor for a task from ALN-derived data.
pub fn task_from_aln(
    shard: &AlnShard,
    row: &str,
    neurorights: NeuroRightsEnvelope,
    equality: EqualityEnvelope,
) -> anyhow::Result<PrometheusTask> {
    Ok(PrometheusTask {
        task_id: shard.get_str(&format!("{row}.field.task_id"))?.to_owned(),
        kind: match shard.get_str(&format!("{row}.field.kind"))? {
            "EcoRestoration" => TaskKind::EcoRestoration,
            "SmartCityUpgrade" => TaskKind::SmartCityUpgrade,
            "HealthcareProcedure" => TaskKind::HealthcareProcedure,
            "AugmentationUpgrade" => TaskKind::AugmentationUpgrade,
            "PaymentProgramRollout" => TaskKind::PaymentProgramRollout,
            other => return Err(anyhow::anyhow!("Unknown TaskKind {other}")),
        },
        jurisdiction_ref: shard.get_str(&format!("{row}.field.jurisdiction_ref"))?.to_owned(),
        service_class: shard.get_str(&format!("{row}.field.service_class"))?.to_owned(),
        eco_target: Bounded01::new(shard.get_f32(&format!("{row}.field.eco_target"))?)?,
        roh_target: Bounded01::new(shard.get_f32(&format!("{row}.field.roh_target"))?)?,
        neurorights_env: neurorights,
        equality_env: equality,
    })
}
