use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::models::EcoperJouleRecord;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RankedWorkload {
    pub workload_id: String,
    pub node_id: Uuid,
    pub ecoper_joule: f64,
    pub energy_domain: String,
}

pub fn rank_by_ecoper(records: &[EcoperJouleRecord]) -> Vec<RankedWorkload> {
    let mut ranked: Vec<RankedWorkload> = records
        .iter()
        .map(|r| RankedWorkload {
            workload_id: r.workload_id.clone(),
            node_id: r.node_id,
            ecoper_joule: r.ecoper_joule,
            energy_domain: match r.energy_domain {
                crate::models::EnergyDomain::Actual => "ACTUAL".to_string(),
                crate::models::EnergyDomain::Modeled => "MODELED".to_string(),
            },
        })
        .collect();

    ranked.sort_by(|a, b| {
        b.ecoper_joule
            .partial_cmp(&a.ecoper_joule)
            .unwrap_or(std::cmp::Ordering::Equal)
    });

    ranked
}
