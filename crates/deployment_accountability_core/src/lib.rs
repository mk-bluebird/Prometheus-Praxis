// Filename: crates/deployment_accountability_core/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RuntimeRisks {
    pub energy_risk: f64,
    pub hydraulics_risk: f64,
    pub biology_risk: f64,
    pub carbon_risk: f64,
    pub materials_risk: f64,
    pub dataquality_risk: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct GoldCorridors {
    pub energy_gold_max: f64,
    pub hydraulics_gold_max: f64,
    pub biology_gold_max: f64,
    pub carbon_gold_max: f64,
    pub materials_gold_max: f64,
    pub dataquality_gold_max: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DeploymentAccountabilityParticle {
    pub nodeid: String,
    pub operatorid: String,
    pub stakeholderid: String,
    pub corridorinstanceid: String,
    pub runtime_ker_k: f64,
    pub runtime_ker_e: f64,
    pub runtime_ker_r: f64,
    pub kerdeployable: bool,
    pub evidencehex: String,
}

impl DeploymentAccountabilityParticle {
    pub fn ker_bounds_ok(&self) -> bool {
        self.runtime_ker_k >= 0.80 && self.runtime_ker_k <= 1.00 &&
        self.runtime_ker_e >= 0.80 && self.runtime_ker_e <= 1.00 &&
        self.runtime_ker_r >= 0.0  && self.runtime_ker_r <= 0.50
    }
}

/// Check whether all runtime risks are within their gold corridors.
fn all_risks_within_gold(risks: RuntimeRisks, gold: GoldCorridors) -> bool {
    risks.energy_risk      <= gold.energy_gold_max &&
    risks.hydraulics_risk  <= gold.hydraulics_gold_max &&
    risks.biology_risk     <= gold.biology_gold_max &&
    risks.carbon_risk      <= gold.carbon_gold_max &&
    risks.materials_risk   <= gold.materials_gold_max &&
    risks.dataquality_risk <= gold.dataquality_gold_max
}

/// Validation function:
/// - If any runtime risk > gold corridor, set kerdeployable = false.
/// - Otherwise, leave kerdeployable unchanged (subject to KER bounds).
pub fn validate_deployment_particle(
    particle: &mut DeploymentAccountabilityParticle,
    risks: RuntimeRisks,
    gold: GoldCorridors,
) -> bool {
    if !particle.ker_bounds_ok() {
        particle.kerdeployable = false;
        return false;
    }

    if !all_risks_within_gold(risks, gold) {
        particle.kerdeployable = false;
        return false;
    }

    true
}
