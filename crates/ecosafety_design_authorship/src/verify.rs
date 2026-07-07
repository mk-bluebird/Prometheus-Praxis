// Filename: crates/ecosafety_design_authorship/src/verify.rs
#![allow(dead_code)]
use kani::proof;
use crate::{DesignAuthorshipParticle, Domain, KaniManifest, validate_design_authorship_particle};

#[proof]
fn design_authorship_basic_valid() {
    let particle = DesignAuthorshipParticle {
        designkernelid: "tailwind_fsm".to_string(),
        designversion: "0.1.0".to_string(),
        designauthorid: "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".to_string(),
        domain: Domain::Cyboquatic,
        ker_K: 0.94,
        ker_E: 0.91,
        ker_R: 0.12,
        ncorridors: 12,
        nverifiedinvariants: 3,
        kani_proof_hashes: vec!["0xa3f5c7e9b1d20468c7e4a9d2b5f81357".to_string()],
        repo_path: "crates/tailwind_fsm".to_string(),
        commit_hash: "abc123def456".to_string(),
        ecosafety_core_dep: "ecosafety-core-v2.0.0".to_string(),
        evidencehex: "0xa1b2c3d4e5f67890f1e2d3c4b5a69788".to_string(),
    };

    let manifest = KaniManifest {
        crate_name: "tailwind_fsm".to_string(),
        repo_path: "crates/tailwind_fsm".to_string(),
        commit_hash: "abc123def456".to_string(),
        proof_hashes: vec!["0xa3f5c7e9b1d20468c7e4a9d2b5f81357".to_string()],
    };

    kani::assert!(validate_design_authorship_particle(&particle, &manifest));
}
