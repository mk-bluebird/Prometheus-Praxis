// Filename: crates/deployment_accountability_core/src/verify.rs
#![allow(dead_code)]

use kani::proof;
use crate::{
    DeploymentAccountabilityParticle,
    RuntimeRisks,
    GoldCorridors,
    validate_deployment_particle,
};

fn sample_gold_corridors() -> GoldCorridors {
    GoldCorridors {
        energy_gold_max: 0.6,
        hydraulics_gold_max: 0.6,
        biology_gold_max: 0.6,
        carbon_gold_max: 0.6,
        materials_gold_max: 0.6,
        dataquality_gold_max: 0.6,
    }
}

fn sample_risks_good() -> RuntimeRisks {
    RuntimeRisks {
        energy_risk: 0.3,
        hydraulics_risk: 0.2,
        biology_risk: 0.25,
        carbon_risk: 0.15,
        materials_risk: 0.20,
        dataquality_risk: 0.10,
    }
}

fn sample_risks_bad() -> RuntimeRisks {
    RuntimeRisks {
        energy_risk: 0.7,         // exceeds gold
        hydraulics_risk: 0.2,
        biology_risk: 0.25,
        carbon_risk: 0.15,
        materials_risk: 0.20,
        dataquality_risk: 0.10,
    }
}

fn sample_particle_initial(deployable: bool) -> DeploymentAccountabilityParticle {
    DeploymentAccountabilityParticle {
        nodeid: "vault-001".to_string(),
        operatorid: "operator-xyz".to_string(),
        stakeholderid: "stakeholder-phoenix-school".to_string(),
        corridorinstanceid: "ecosafety.corridors.v2:airglobe-phx".to_string(),
        runtime_ker_k: 0.93,
        runtime_ker_e: 0.90,
        runtime_ker_r: 0.12,
        kerdeployable: deployable,
        evidencehex: "0xa1b2c3d4e5f6".to_string(),
    }
}

#[proof]
fn kerdeployable_set_false_when_risk_exceeds_gold() {
    let gold = sample_gold_corridors();
    let risks_bad = sample_risks_bad();
    let mut particle = sample_particle_initial(true);

    let ok = validate_deployment_particle(&mut particle, risks_bad, gold);
    kani::assert!(!ok);
    kani::assert!(!particle.kerdeployable);
}

#[proof]
fn kerdeployable_monotone_under_validation() {
    let gold = sample_gold_corridors();
    let risks_good = sample_risks_good();
    let risks_bad = sample_risks_bad();

    let mut particle = sample_particle_initial(true);

    // First, valid state should keep kerdeployable true.
    let ok1 = validate_deployment_particle(&mut particle, risks_good, gold);
    kani::assert!(ok1);
    kani::assert!(particle.kerdeployable);

    // Now, a later bad state must set kerdeployable to false.
    let ok2 = validate_deployment_particle(&mut particle, risks_bad, gold);
    kani::assert!(!ok2);
    kani::assert!(!particle.kerdeployable);

    // Re-validating with good risks must NOT flip kerdeployable back to true.
    let ok3 = validate_deployment_particle(&mut particle, risks_good, gold);
    // Depending on ker_bounds, ok3 may be true or false, but kerdeployable must remain false.
    kani::assert!(!particle.kerdeployable);
}
